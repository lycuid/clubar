#include "config.h"
#include "include/blocks.h"
#include "include/x.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK_BUF_SIZE (1 << 10)

void *stdin_handler();
void setup();
void gracefully_exit();

static Block Blks[2][MAX_BLKS];
static int NBlks[2], BarExposed = 0, EventLoopRunning = 1;
static pthread_t thread_handle;

void *stdin_handler() {
  struct timespec ts = {.tv_nsec = 50 * 1e6};
  while (!BarExposed)
    nanosleep(&ts, &ts);

  size_t nbuf, alloc = BLOCK_BUF_SIZE;
  char *stdinbuf = malloc(alloc), previous[alloc];
  memset(stdinbuf, 0, alloc);
  memset(previous, 0, alloc);

  while ((nbuf = getline(&stdinbuf, &alloc, stdin)) > 1) {
    stdinbuf[nbuf - 1] = 0;
    if (strcmp(previous, stdinbuf) == 0)
      continue;

    freeblks(Blks[Stdin], MAX_BLKS);
    NBlks[Stdin] = createblks(stdinbuf, Blks[Stdin]);
    b_RenderBlks(Stdin, Blks, NBlks);

    memcpy(previous, stdinbuf, nbuf);
    memset(stdinbuf, 0, nbuf);
  }
  pthread_exit(0);
}

void setup() {
  pthread_create(&thread_handle, NULL, stdin_handler, NULL);
  for (int i = 0; i < 2; ++i)
    NBlks[i] = 0;

  b_Setup();
  signal(SIGINT, gracefully_exit);
  signal(SIGHUP, gracefully_exit);
  signal(SIGTERM, gracefully_exit);
}

void gracefully_exit() { EventLoopRunning = 0; }

int main(void) {
  setup();
  char customstr[BLOCK_BUF_SIZE];
  struct timespec ts = {.tv_nsec = 1e6 * 25};

  while (EventLoopRunning) {
    nanosleep(&ts, &ts);
    if (b_HandleEvent(Blks, NBlks, customstr, &BarExposed)) {
      freeblks(Blks[Custom], MAX_BLKS);
      NBlks[Custom] = createblks(customstr, Blks[Custom]);
      b_RenderBlks(Custom, Blks, NBlks);
    }
  }

  pthread_cancel(thread_handle);
  b_Cleanup();
  return 0;
}
