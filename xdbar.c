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

#define UpdateBar(blktype, buffer)                                             \
  {                                                                            \
    pthread_mutex_lock(&mutex);                                                \
    Bar_ClearBlks(blktype, NBlks[blktype]);                                    \
    pthread_mutex_unlock(&mutex);                                              \
                                                                               \
    freeblks(Blks[blktype], MAX_BLKS);                                         \
    NBlks[blktype] = createblks(buffer, Blks[blktype]);                        \
                                                                               \
    pthread_mutex_lock(&mutex);                                                \
    Bar_RenderBlks(blktype, Blks[blktype], NBlks[blktype]);                    \
    pthread_mutex_unlock(&mutex);                                              \
  }

BarEvent get_event(char[BLOCK_BUF_SIZE]);
void *stdin_handler();
void create_config();
void setup(Config *);
void gracefully_exit();

pthread_t thread_handle;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
Block Blks[2][MAX_BLKS];
int NBlks[2], RunEventLoop = 1;
char *ConfigFile = NULL;

BarEvent get_event(char string[BLOCK_BUF_SIZE]) {
  pthread_mutex_lock(&mutex);
  BarEvent event = Bar_HandleEvent(Blks, NBlks, string);
  pthread_mutex_unlock(&mutex);
  return event;
}

void *stdin_handler() {
  ssize_t nbuf;
  size_t size = BLOCK_BUF_SIZE;
  char *stdinstr = malloc(size), previous[size];
  memset(stdinstr, 0, size);
  memset(previous, 0, size);

  while ((nbuf = getline(&stdinstr, &size, stdin)) > 1) {
    // trim off newline.
    stdinstr[nbuf - 1] = 0;
    if (strcmp(previous, stdinstr) == 0)
      continue;

    UpdateBar(Stdin, stdinstr);
    memcpy(previous, stdinstr, BLOCK_BUF_SIZE);
    memset(stdinstr, 0, BLOCK_BUF_SIZE);
  }
  pthread_exit(0);
}

void create_config(Config *config) {
  config->nfonts = sizeof(fonts) / sizeof(*fonts);
  config->fonts = (char **)fonts;
  config->barConfig = barConfig;

#ifdef Patch_xrmconfig
#include "include/patches/xrmconfig.h"
  merge_xrm_config(&config);
#endif

#ifdef Patch_luaconfig
#include "include/patches/luaconfig.h"
  if (ConfigFile)
    merge_lua_config(ConfigFile, &config);
#endif
}

void setup(Config *config) {
  for (int i = 0; i < 2; ++i)
    NBlks[i] = 0;
  Bar_Setup(config);
  signal(SIGINT, gracefully_exit);
  signal(SIGHUP, gracefully_exit);
  signal(SIGTERM, gracefully_exit);
}

void gracefully_exit() { RunEventLoop = 0; }

int main(int argc, char **argv) {
  Config config;
  char arg, customstr[BLOCK_BUF_SIZE];
  while ((arg = getopt(argc, argv, "hvc:")) != -1) {
    switch (arg) {
    case 'h':
      usage();
      exit(EXIT_SUCCESS);
    case 'v':
      puts(NAME ": v" VERSION);
      exit(EXIT_SUCCESS);
    case 'c':
      ConfigFile = optarg;
      break;
    default:
      exit(2);
    }
  }

  create_config(&config);
  setup(&config);
  struct timespec ts = {.tv_nsec = 1e6 * 25};

  while (RunEventLoop) {
    nanosleep(&ts, NULL);
    switch (get_event(customstr)) {
    case ReadyEvent:
      pthread_create(&thread_handle, NULL, stdin_handler, NULL);
      break;
    case DrawEvent:
      UpdateBar(Custom, customstr);
      break;
    case NoActionEvent:
      break;
    }
  }

  // doesn't make any sense to keep the thread running, after event loop stops.
  pthread_cancel(thread_handle);
  Bar_Cleanup();
  return 0;
}
