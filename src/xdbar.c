#include "config.h"
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <xdbar/core/blocks.h>

#define THREADLOCK(mutex_ptr)                                                  \
  /* @NOTE: Assuming lock/unlock is always successful. */                      \
  for (int __cond = pthread_mutex_lock(mutex_ptr) + 1; __cond;                 \
       __cond     = pthread_mutex_unlock(mutex_ptr))

#define RENDER(blktype, buffer)                                                \
  {                                                                            \
    THREADLOCK(&mutex) { xdb_clear(blktype); }                                 \
    core->update_blks(blktype, buffer);                                        \
    THREADLOCK(&mutex) { xdb_render(blktype); }                                \
  }

static pthread_mutex_t mutex     = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t read_stdin = PTHREAD_COND_INITIALIZER;
static bool window_ready         = false;
static const struct timespec ts  = {.tv_nsec = 1e6 * (25 /* ms. */)};

typedef struct IOReader {
  char buffer[1 << 16];
  int start, end;
} IOReader;
#define IOReader() (IOReader){.start = 0, .end = 0};

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

void *stdin_thread_handler()
{
  THREADLOCK(&mutex)
  {
    if (!window_ready)
      pthread_cond_wait(&read_stdin, &mutex);
  }
  IOReader io = IOReader();
  char *line;
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
  for (bool running = core->running; running; nanosleep(&ts, NULL)) {
    if ((line = readline(&io)) && strlen(line) > 0)
      RENDER(Stdin, line);
    THREADLOCK(&mutex) { running = core->running; }
  }
  pthread_exit(0);
}

static void quit(__attribute__((unused)) int _arg)
{
  THREADLOCK(&mutex) { core->stop_running(); }
}

int main(int argc, char **argv)
{
  char buffer[BLK_BUFFER_SIZE];
  pthread_t stdin_thread;
  XDBEvent event = NoActionEvent;

  core->init(argc, argv);
  signal(SIGINT, quit);
  signal(SIGHUP, quit);
  signal(SIGTERM, quit);
  signal(SIGUSR1, xdb_toggle);

  xdb_setup();
  pthread_create(&stdin_thread, NULL, stdin_thread_handler, NULL);
  for (bool running = core->running; running; nanosleep(&ts, NULL)) {
    THREADLOCK(&mutex) { event = xdb_nextevent(buffer); }
    switch (event) {
    case ReadyEvent:
      THREADLOCK(&mutex)
      {
        window_ready = true;
        pthread_cond_signal(&read_stdin);
      }
      break;
    case RenderEvent:
      RENDER(Custom, buffer) break;
    default:
      break;
    }
    THREADLOCK(&mutex) { running = core->running; }
  }
  pthread_join(stdin_thread, NULL);
  xdb_cleanup();
  return 0;
}
