#include "config.h"
#include "include/blocks.h"
#include "include/x.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BLOCK_BUF_SIZE (1 << 10)

void *stdin_handler();
Config create_config();
void setup(Config *);
void gracefully_exit();

static Block Blks[2][MAX_BLKS];
static int NBlks[2], BarExposed = 0, RunEventLoop = 1;
static pthread_t thread_handle;
static char *CONFIG_FILE = NULL;

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
    Bar_RenderBlks(Stdin, Blks, NBlks);

    memcpy(previous, stdinbuf, nbuf);
    memset(stdinbuf, 0, nbuf);
  }
  pthread_exit(0);
}

Config create_config() {
  Config config;
  config.nfonts = sizeof(fonts) / sizeof(*fonts);
  config.fonts = (char **)fonts;
  config.barConfig = barConfig;

#if CONFIG_TYPE == lua
#include "include/lua.h"
  if (CONFIG_FILE)
    load_lua_config(CONFIG_FILE, &config);
#endif

  return config;
}

void setup(Config *config) {
  pthread_create(&thread_handle, NULL, stdin_handler, NULL);
  for (int i = 0; i < 2; ++i)
    NBlks[i] = 0;

  Bar_Setup(config);
  signal(SIGINT, gracefully_exit);
  signal(SIGHUP, gracefully_exit);
  signal(SIGTERM, gracefully_exit);
}

void gracefully_exit() { RunEventLoop = 0; }

int main(int argc, char **argv) {
  char arg;
  while ((arg = getopt(argc, argv, "hvc:")) != -1) {
    switch (arg) {
    case 'h':
      usage();
      exit(0);
    case 'v':
      puts(NAME "-" VERSION);
      exit(0);
    case 'c':
      CONFIG_FILE = optarg;
      break;
    default:
      exit(2);
    }
  }

  char customstr[BLOCK_BUF_SIZE];
  struct timespec ts = {.tv_nsec = 1e6 * 25};

  Config config = create_config();
  setup(&config);

  while (RunEventLoop) {
    nanosleep(&ts, &ts);
    if (Bar_HandleEvent(Blks, NBlks, customstr, &BarExposed)) {
      freeblks(Blks[Custom], MAX_BLKS);
      NBlks[Custom] = createblks(customstr, Blks[Custom]);
      Bar_RenderBlks(Custom, Blks, NBlks);
    }
  }

  // doesn't make any sense to keep the thread running, after event loop stops.
  pthread_cancel(thread_handle);
  Bar_Cleanup();
  return 0;
}
