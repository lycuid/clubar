#include "config.h"
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define INTERVAL_MS         (1000 / 120.)
#define IS_SET(value, mask) (((value) & (mask)) != 0)
#define CLEAR_AND_RENDER_WITH(blktype)                                         \
  for (int _c = (pthread_mutex_lock(&clu_mutex), clu_clear(blktype), 1); _c;   \
       _c     = (clu_render(blktype), pthread_mutex_unlock(&clu_mutex), 0))

#define WITH_LOCK(lock_expr, unlock_expr)                                      \
  for (int __cond = ((lock_expr), 1); __cond; __cond = ((unlock_expr), 0))

#define WITH_MUTEX(mutex_ptr)                                                  \
  WITH_LOCK(pthread_mutex_lock(mutex_ptr), pthread_mutex_unlock(mutex_ptr))
#define WITH_RDLOCK(lock_ptr)                                                  \
  WITH_LOCK(pthread_rwlock_rdlock(lock_ptr), pthread_rwlock_unlock(lock_ptr))
#define WITH_WRLOCK(lock_ptr)                                                  \
  WITH_LOCK(pthread_rwlock_wrlock(lock_ptr), pthread_rwlock_unlock(lock_ptr))

typedef struct LineReader {
  char buffer[4096];
  int start, end;
} LineReader;
#define LINE_READER() (LineReader){.start = 0, .end = 0};

typedef struct ThreadSync {
  bool ready;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} ThreadSync;

// clang-format off
#define THREADSYNC()                                                           \
  {false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#define THREADSYNC_WAIT(t)                                                     \
  WITH_MUTEX(&t.mutex) if (!t.ready) pthread_cond_wait(&t.cond, &t.mutex);
#define THREADSYNC_SIGNAL(t)                                                   \
  WITH_MUTEX(&t.mutex) { t.ready = true; pthread_cond_signal(&t.cond); }
// clang-format on

static ThreadSync stdin_threadsync  = THREADSYNC();
static pthread_rwlock_t core_rwlock = PTHREAD_RWLOCK_INITIALIZER; // core apis.
static pthread_mutex_t clu_mutex    = PTHREAD_MUTEX_INITIALIZER; // 'clu_' apis.
static const struct timespec ts     = {.tv_nsec = 1e6 * INTERVAL_MS};

static inline char *readline(LineReader *lr)
{
  ssize_t nread;
  static const int size = sizeof(lr->buffer) - 1;
  if (lr->start > 0 && lr->buffer[lr->start - 1] == 0) {
    memmove(lr->buffer, lr->buffer + lr->start, lr->end -= lr->start);
    lr->start = 0;
  }
  if (lr->start == lr->end) {
    if (lr->end == size)
      lr->start = lr->end = 0;
    if ((nread = read(STDIN_FILENO, lr->buffer + lr->end, size - lr->end)) > 0)
      lr->end += nread;
  }
  for (; lr->start < lr->end; ++lr->start)
    if (lr->buffer[lr->start] == '\n' && !(lr->buffer[lr->start++] = 0))
      return lr->buffer;
  return NULL;
}
// clang-format off

static void *stdin_thread_handler(__attribute__((unused)) void *_)
{
  THREADSYNC_WAIT(stdin_threadsync);
  struct pollfd fd  = {.fd = STDIN_FILENO, .events = POLLIN};
  LineReader reader = LINE_READER();
  char *line;
  for (bool running = core->running; running; ) {
    if (poll(&fd, 1, 420) > 0) {
      if (IS_SET(fd.revents, POLLERR | POLLHUP)) {
        WITH_WRLOCK(&core_rwlock) { core->stop_running(); }
      }
      if (IS_SET(fd.revents, POLLIN)) {
        if ((line = readline(&reader)) == NULL) {
          nanosleep(&ts, NULL); // clubar </dev/zero
        } else if (*line) {
          CLEAR_AND_RENDER_WITH(Stdin) {
            WITH_WRLOCK(&core_rwlock) { core->update_blks(Stdin, line); }
          }
        }
      }
    }
    WITH_RDLOCK(&core_rwlock) { running = core->running; }
  }
  pthread_exit(0);
}

static void *sig_thread_handler(void *arg)
{
  sigset_t *sig_set = (sigset_t *)arg;
  for (int sig, running = core->running; running;) {
    switch (sigwait(sig_set, &sig), sig) {
      case SIGCHLD: { while (wait(NULL) > 0); } break;
      case SIGQUIT: // fallthrough.
      case SIGINT:  // fallthrough.
      case SIGHUP:  // fallthrough.
      case SIGTERM: { WITH_WRLOCK(&core_rwlock) core->stop_running(); } break;
      case SIGUSR1: { WITH_MUTEX(&clu_mutex)    clu_toggle(); }         break;
      default: break;
    }
    WITH_RDLOCK(&core_rwlock) { running = core->running; }
  }
  pthread_exit(0);
}

int main(int argc, char **argv)
{
  core->init(argc, argv);

  char buffer[BLK_BUFFER_SIZE];
  CluEvent clu_event;
  pthread_t stdin_thread, sig_thread;
  sigset_t sig_set;

  sigemptyset(&sig_set);
  sigaddset(&sig_set, SIGCHLD);
  sigaddset(&sig_set, SIGQUIT);
  sigaddset(&sig_set, SIGINT);
  sigaddset(&sig_set, SIGHUP);
  sigaddset(&sig_set, SIGTERM);
  sigaddset(&sig_set, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &sig_set, NULL);

  clu_setup();
  pthread_create(&stdin_thread, NULL, stdin_thread_handler, NULL);
  pthread_create(&sig_thread, NULL, sig_thread_handler, &sig_set);

  for (bool running = core->running; running; (void)nanosleep(&ts, NULL)) {
    WITH_MUTEX(&clu_mutex) { clu_event = clu_nextevent(buffer); }
    switch (clu_event) {
    case CLU_Ready: {
      THREADSYNC_SIGNAL(stdin_threadsync);
    } // fallthrough.
    case CLU_NewValue: {
      CLEAR_AND_RENDER_WITH(Custom) {
        WITH_WRLOCK(&core_rwlock) { core->update_blks(Custom, buffer); }
      }
    } break;
    case CLU_Reset: {
      CLEAR_AND_RENDER_WITH(Stdin) { (void)0; }
      CLEAR_AND_RENDER_WITH(Custom) { (void)0; }
    } break;
    case CLU_Noop: break;
    }
    WITH_RDLOCK(&core_rwlock) { running = core->running; }
  }

  pthread_join(stdin_thread, NULL);
  pthread_join(sig_thread, NULL);
  clu_cleanup();
  return 0;
}
