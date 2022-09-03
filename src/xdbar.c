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

#define LOCKED(mutex_ptr)                                                      \
  /* @GOTCHA: Assuming lock/unlock is always successful. */                    \
  for (int __cond = pthread_mutex_lock(mutex_ptr) + 1; __cond;                 \
       __cond     = pthread_mutex_unlock(mutex_ptr))

#define RENDER(blktype, buffer)                                                \
  {                                                                            \
    LOCKED(&mutex) { xdb_clear(blktype); }                                     \
    core->update_blks(blktype, buffer);                                        \
    LOCKED(&mutex) { xdb_render(blktype); }                                    \
  }

static pthread_mutex_t mutex     = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t read_stdin = PTHREAD_COND_INITIALIZER;
static bool window_ready         = false;
static const struct timespec ts  = {.tv_nsec = 1e6 * (25 /* ms. */)};

void *stdin_thread_handler()
{
  LOCKED(&mutex)
  {
    if (!window_ready)
      pthread_cond_wait(&read_stdin, &mutex);
  }
  char line[BLK_BUFFER_SIZE], buffer[BLK_BUFFER_SIZE], byte;
  int cursor = 0;

  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
  for (bool running = core->running; running;) {
    for (ssize_t n = 0; running && (n = read(STDIN_FILENO, &byte, 1)) > 0;) {
      line[cursor++] = byte;
      if (byte == '\n' || cursor == BLK_BUFFER_SIZE) {
        cursor = line[cursor - 1] = 0;
        if (strcmp(buffer, line)) // drawing is expensive.
          RENDER(Stdin, strcpy(buffer, line));
        break; // No sleep here.. don't wanna read forever (xdbar </dev/zero).
      }
    }
    nanosleep(&ts, NULL);
    LOCKED(&mutex) { running = core->running; }
  }
  pthread_exit(0);
}

static void quit(__attribute__((unused)) int _arg)
{
  LOCKED(&mutex) { core->stop_running(); }
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

  for (bool running = core->running; running;) {
    LOCKED(&mutex) { event = xdb_nextevent(buffer); }
    switch (event) {
    case ReadyEvent:
      LOCKED(&mutex)
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
    nanosleep(&ts, NULL);
    LOCKED(&mutex) { running = core->running; }
  }

  pthread_join(stdin_thread, NULL);
  xdb_cleanup();
  return 0;
}
