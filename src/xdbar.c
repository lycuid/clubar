#include "config.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <xdbar/core/blocks.h>

#define ThreadLocked(block)                                                    \
  {                                                                            \
    pthread_mutex_lock(&mutex);                                                \
    block;                                                                     \
    pthread_mutex_unlock(&mutex);                                              \
  }

#define UpdateBar(blktype, buffer)                                             \
  {                                                                            \
    ThreadLocked(xdb_clear(blktype));                                          \
    core->update_blks(blktype, buffer);                                        \
    ThreadLocked(xdb_render(blktype));                                         \
  }

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond   = PTHREAD_COND_INITIALIZER;

void *stdin_thread_handler()
{
  ThreadLocked(pthread_cond_wait(&cond, &mutex));
  size_t size    = BLK_BUFFER_SIZE;
  char *stdinstr = malloc(BLK_BUFFER_SIZE), previous[BLK_BUFFER_SIZE];
  memset(stdinstr, 0, size);
  memset(previous, 0, size);

  for (ssize_t nbuf; (nbuf = getline(&stdinstr, &size, stdin)) > 1;) {
    // trim off newline.
    stdinstr[nbuf - 1] = 0;
    // drawing is expensive.
    if (strcmp(previous, stdinstr) == 0)
      continue;
    UpdateBar(Stdin, stdinstr);
    memcpy(previous, stdinstr, BLK_BUFFER_SIZE);
    memset(stdinstr, 0, BLK_BUFFER_SIZE);
  }
  pthread_exit(0);
}

int main(int argc, char **argv)
{
  char customstr[BLK_BUFFER_SIZE];
  pthread_t stdin_thread;
  struct timespec ts = {.tv_nsec = 1e6 * 25};
  BarEvent event;

  core->init(argc, argv);
  signal(SIGINT, core->stop_running);
  signal(SIGHUP, core->stop_running);
  signal(SIGTERM, core->stop_running);

  xdb_setup();
  pthread_create(&stdin_thread, NULL, stdin_thread_handler, NULL);

  while (core->running) {
    nanosleep(&ts, NULL);
    ThreadLocked(event = xdb_nextevent(customstr));
    switch (event) {
    case ReadyEvent:
      ThreadLocked(pthread_cond_broadcast(&cond));
      break;
    case DrawEvent:
      UpdateBar(Custom, customstr);
      break;
    default:
      break;
    }
  }

  pthread_cancel(stdin_thread);
  xdb_cleanup();
  return 0;
}
