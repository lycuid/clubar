#include "config.h"
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CLEAR_AND_RENDER_WITH(blktype)                                         \
  /* @NOTE: Assuming lock/unlock is always successful (returns 0). */          \
  for (int _c = (pthread_mutex_lock(&clu_mutex), clu_clear(blktype),           \
                 pthread_mutex_unlock(&clu_mutex), 1);                         \
       _c; _c = (pthread_mutex_lock(&clu_mutex), clu_render(blktype),          \
                 pthread_mutex_unlock(&clu_mutex)))

#define WITH_MUTEX(mutex_ptr)                                                  \
  /* @NOTE: Assuming lock/unlock is always successful (returns 0). */          \
  for (int __cond = (pthread_mutex_lock(mutex_ptr), 1); __cond;                \
       __cond     = pthread_mutex_unlock(mutex_ptr))

typedef struct IOReader {
  char buffer[1 << 13];
  int start, end;
} IOReader;
#define IOREADER() (IOReader){.start = 0, .end = 0};

typedef struct ThreadSync {
  bool ready;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} ThreadSync;
// clang-format off
#define THREADSYNC()                                                            \
  {false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#define THREADSYNC_WAIT(t)                                                      \
  WITH_MUTEX(&t.mutex) if (!t.ready) pthread_cond_wait(&t.cond, &t.mutex);
#define THREADSYNC_SIGNAL(t)                                                    \
  WITH_MUTEX(&t.mutex) { t.ready = true; pthread_cond_signal(&t.cond); }

static ThreadSync stdin_threadsync  = THREADSYNC();
static pthread_mutex_t core_mutex   = PTHREAD_MUTEX_INITIALIZER, // core apis.
                       clu_mutex    = PTHREAD_MUTEX_INITIALIZER; // 'clu_' apis.
static const struct timespec ts     = {.tv_nsec = 1e6 * 1000 / 120};
// clang-format on

static inline char *readline(IOReader *io)
{
  ssize_t nread;
  static const int size = sizeof(io->buffer);
  // A 'line' was returned in previous iteration.
  if (io->start > 0 && io->buffer[io->start - 1] == 0) {
    memmove(io->buffer, io->buffer + io->start, io->end -= io->start);
    io->start = 0;
  }
  if (io->start == io->end) { // empty string.
    if (io->end == size)      // reset buffer.
      io->start = io->end = 0;
    if ((nread = read(STDIN_FILENO, io->buffer + io->end, size - io->end)) > 0)
      io->end += nread;
  }
  for (; io->start < io->end; ++io->start)
    if (io->buffer[io->start] == '\n' && !(io->buffer[io->start++] = 0))
      return io->buffer;
  return NULL;
}

static void *stdin_thread_handler()
{
  THREADSYNC_WAIT(stdin_threadsync);
  IOReader io = IOREADER();
  char *line;
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
  for (bool running = core->running; running; nanosleep(&ts, NULL)) {
    if ((line = readline(&io)) && strlen(line) > 0)
      CLEAR_AND_RENDER_WITH(Stdin) { core->update_blks(Stdin, line); }
    WITH_MUTEX(&core_mutex) { running = core->running; }
  }
  pthread_exit(0);
}

static void quit(__attribute__((unused)) int _arg)
{
  WITH_MUTEX(&core_mutex) { core->stop_running(); }
}

int main(int argc, char **argv)
{
  char buffer[BLK_BUFFER_SIZE];
  pthread_t stdin_thread;
  CluEvent event;

  core->init(argc, argv);
  signal(SIGINT, quit);
  signal(SIGHUP, quit);
  signal(SIGTERM, quit);
  signal(SIGUSR1, clu_toggle);

  clu_setup();
  pthread_create(&stdin_thread, NULL, stdin_thread_handler, NULL);
  for (bool running = core->running; running; nanosleep(&ts, NULL)) {
    switch (event = clu_nextevent(buffer)) {
    case CLU_Ready:
      THREADSYNC_SIGNAL(stdin_threadsync); // fallthrough.
    case CLU_NewValue:
      CLEAR_AND_RENDER_WITH(Custom) { core->update_blks(Custom, buffer); }
      break;
    case CLU_Reset:
      CLEAR_AND_RENDER_WITH(Stdin);
      CLEAR_AND_RENDER_WITH(Custom);
      break;
    case CLU_NoOp: // fallthrough.
    default:
      break;
    }
    WITH_MUTEX(&core_mutex) { running = core->running; }
  }
  pthread_join(stdin_thread, NULL);
  clu_cleanup();
  return 0;
}
