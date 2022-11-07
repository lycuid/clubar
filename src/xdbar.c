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
  for (int _c = (pthread_mutex_lock(&xdb_mutex), xdb_clear(blktype),           \
                 pthread_mutex_unlock(&xdb_mutex), 1);                         \
       _c; _c = (pthread_mutex_lock(&xdb_mutex), xdb_render(blktype),          \
                 pthread_mutex_unlock(&xdb_mutex)))

#define WITH_MUTEX(mutex_ptr)                                                  \
  /* @NOTE: Assuming lock/unlock is always successful (returns 0). */          \
  for (int __cond = (pthread_mutex_lock(mutex_ptr), 1); __cond;                \
       __cond     = pthread_mutex_unlock(mutex_ptr))

typedef struct IOReader {
  char buffer[1 << 13];
  int start, end;
} IOReader;
#define IOReader() (IOReader){.start = 0, .end = 0};

typedef struct ThreadSync {
  bool ready;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} ThreadSync;
// clang-format off
#define ThreadSync() {false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#define THREADSYNC_WAIT(t) WITH_MUTEX(&t.mutex) if (!t.ready) pthread_cond_wait(&t.cond, &t.mutex);
#define THREADSYNC_SIGNAL(t) WITH_MUTEX(&t.mutex) { t.ready = true; pthread_cond_signal(&t.cond); }

static ThreadSync stdin_threadsync  = ThreadSync();
static pthread_mutex_t  core_mutex  = PTHREAD_MUTEX_INITIALIZER, // core apis.
                         xdb_mutex  = PTHREAD_MUTEX_INITIALIZER; // 'xdb_' apis.
static const struct timespec ts     = {.tv_nsec = 1e6 * (25 /* ms. */)};
// clang-format on

static inline char *readline(IOReader *io)
{
  ssize_t nread;
  int size = sizeof(io->buffer);
  // A 'line' was returned in previous iteration.
  if (io->start > 0 && io->buffer[io->start - 1] == 0) {
    // shift string to beginning of the buffer.
    ssize_t diff = io->end - io->start;
    for (io->end = 0; io->end < diff; ++io->end)
      io->buffer[io->end] = io->buffer[io->start + io->end];
    io->start = 0;
  }
  if (io->start == io->end) { // empty string.
    if (io->end == size)      // reset buffer.
      io->start = io->end = 0;
    if ((nread = read(STDIN_FILENO, io->buffer + io->end, size - io->end)) > 0)
      io->end += nread;
  }
  for (; io->start < io->end; ++io->start)
    // try returning a 'line'.
    if (io->buffer[io->start] == '\n' && !(io->buffer[io->start++] = 0))
      return io->buffer;
  return NULL;
}

static void *stdin_thread_handler()
{
  THREADSYNC_WAIT(stdin_threadsync);
  IOReader io = IOReader();
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
  xdb_event_t event = XDBNoOp;

  core->init(argc, argv);
  signal(SIGINT, quit);
  signal(SIGHUP, quit);
  signal(SIGTERM, quit);
  signal(SIGUSR1, xdb_toggle);

  xdb_setup();
  pthread_create(&stdin_thread, NULL, stdin_thread_handler, NULL);
  for (bool running = core->running; running; nanosleep(&ts, NULL)) {
    switch (event = xdb_nextevent(buffer)) {
    case XDBReady:
      THREADSYNC_SIGNAL(stdin_threadsync); // fall through.
    case XDBNewValue:
      CLEAR_AND_RENDER_WITH(Custom) { core->update_blks(Custom, buffer); }
      break;
    case XDBReset:
      CLEAR_AND_RENDER_WITH(Stdin);
      CLEAR_AND_RENDER_WITH(Custom);
      break;
    case XDBNoOp:
    default:
      break;
    }
    WITH_MUTEX(&core_mutex) { running = core->running; }
  }
  pthread_join(stdin_thread, NULL);
  xdb_cleanup();
  return 0;
}
