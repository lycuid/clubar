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
// clang-format off

#define INTERVAL_MS         (1000 / 120.)
#define IS_SET(value, mask) (((value) & (mask)) != 0)
#define CLEAR_AND_RENDER_WITH(blktype)                                         \
  for (int _c = (pthread_mutex_lock(&clu_mutex), clu_clear(blktype), 1); _c;   \
       _c     = (clu_render(blktype), pthread_mutex_unlock(&clu_mutex), 0))

#define WITH_MUTEX(mutex_ptr)                                                  \
  for (int __cond = (pthread_mutex_lock(mutex_ptr), 1); __cond;                \
       __cond     = (pthread_mutex_unlock(mutex_ptr), 0))

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

#define THREADSYNC()                                                           \
  {false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#define THREADSYNC_WAIT(t)                                                     \
  WITH_MUTEX(&t.mutex) if (!t.ready) pthread_cond_wait(&t.cond, &t.mutex);
#define THREADSYNC_SIGNAL(t)                                                   \
  WITH_MUTEX(&t.mutex) { t.ready = true; pthread_cond_signal(&t.cond); }

static ThreadSync stdin_threadsync  = THREADSYNC();
static pthread_mutex_t core_mutex   = PTHREAD_MUTEX_INITIALIZER, // core apis.
                       clu_mutex    = PTHREAD_MUTEX_INITIALIZER; // 'clu_' apis.

static inline char *readline(LineReader *lr)
{
  ssize_t nread;
  static const int size = sizeof(lr->buffer) - 1;
  // A 'line' was returned in previous iteration.
  if (lr->start > 0 && lr->buffer[lr->start - 1] == 0) {
    memmove(lr->buffer, lr->buffer + lr->start, lr->end -= lr->start);
    lr->start = 0;
  }
  if (lr->start == lr->end) { // empty string.
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

static void *stdin_thread_handler(__attribute__((unused)) void *_)
{
  THREADSYNC_WAIT(stdin_threadsync);
  struct pollfd fd  = {.fd = STDIN_FILENO, .events = POLLIN};
  LineReader reader = LINE_READER();
  char *line;
  for (bool running = core->running; running;) {
    if (poll(&fd, 1, INTERVAL_MS) > 0) {
      if (IS_SET(fd.revents, POLLERR | POLLHUP)) {
        WITH_MUTEX(&core_mutex) {
          core->stop_running();
        }
      }
      if (IS_SET(fd.revents, POLLIN) && (line = readline(&reader)) && *line) {
        CLEAR_AND_RENDER_WITH(Stdin) {
          WITH_MUTEX(&core_mutex) {
            core->update_blks(Stdin, line);
          }
        }
      }
    }
    WITH_MUTEX(&core_mutex) { running = core->running; }
  }
  pthread_exit(0);
}

static void sighandler(int sig)
{
  if (signal(sig, sighandler) == sighandler) {
    switch (sig) {
      case SIGCHLD: { while (wait(NULL) > 0); } break;
      case SIGQUIT: // fallthrough.
      case SIGINT:  // fallthrough.
      case SIGHUP:  // fallthrough.
      case SIGTERM: { WITH_MUTEX(&core_mutex) { core->stop_running(); } } break;
      case SIGUSR1: { clu_toggle(); } break;
      default: break;
    }
  }
}

int main(int argc, char **argv)
{
  char buffer[BLK_BUFFER_SIZE];
  pthread_t stdin_thread;
  CluEvent event;
  struct timespec ts = {.tv_nsec = 1e6 * INTERVAL_MS};

  core->init(argc, argv);
  sighandler(SIGCHLD);
  sighandler(SIGQUIT);
  sighandler(SIGINT);
  sighandler(SIGHUP);
  sighandler(SIGTERM);
  sighandler(SIGUSR1);

  clu_setup();
  pthread_create(&stdin_thread, NULL, stdin_thread_handler, NULL);
  for (bool running = core->running; running; (void)nanosleep(&ts, NULL)) {
    switch (event = clu_nextevent(buffer)) {
    case CLU_Ready: THREADSYNC_SIGNAL(stdin_threadsync); // fallthrough.
    case CLU_NewValue: {
      CLEAR_AND_RENDER_WITH(Custom) {
        WITH_MUTEX(&core_mutex) {
          core->update_blks(Custom, buffer);
        }
      }
    } break;
    case CLU_Reset: {
      CLEAR_AND_RENDER_WITH(Stdin);
      CLEAR_AND_RENDER_WITH(Custom);
    } break;
    case CLU_NoOp: break;
    }
    WITH_MUTEX(&core_mutex) { running = core->running; }
  }
  pthread_join(stdin_thread, NULL);
  clu_cleanup();
  return 0;
}
