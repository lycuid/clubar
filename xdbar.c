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

#ifdef __ENABLE_PLUGIN__xrmconfig__
#include "include/plugins/xrmconfig.h"
#endif

#ifdef __ENABLE_PLUGIN__luaconfig__
#include "include/plugins/luaconfig.h"
#endif

pthread_t thread_handle;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
Block Blks[2][MAX_BLKS];
int NBlks[2] = {0, 0}, RunEventLoop = 1;
char *ConfigFile = NULL;

void *stdin_handler();
void create_config();
void setup(Config *);
void gracefully_exit();

#define UpdateBar(blktype, buffer)                                             \
  {                                                                            \
    pthread_mutex_lock(&mutex);                                                \
    xdb_clearblks(blktype, NBlks[blktype]);                                    \
    pthread_mutex_unlock(&mutex);                                              \
                                                                               \
    freeblks(Blks[blktype], MAX_BLKS);                                         \
    NBlks[blktype] = createblks(buffer, Blks[blktype]);                        \
                                                                               \
    pthread_mutex_lock(&mutex);                                                \
    xdb_renderblks(blktype, Blks[blktype], NBlks[blktype]);                    \
    pthread_mutex_unlock(&mutex);                                              \
  }

void *stdin_handler() {
  pthread_mutex_lock(&mutex);
  pthread_cond_wait(&cond, &mutex);
  pthread_mutex_unlock(&mutex);

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

#ifdef __ENABLE_PLUGIN__xrmconfig__
  merge_xrm_config(config);
#endif

#ifdef __ENABLE_PLUGIN__luaconfig__
  if (ConfigFile)
    merge_lua_config(ConfigFile, config);
#endif
}

void setup(Config *config) {
  pthread_create(&thread_handle, NULL, stdin_handler, NULL);
  xdb_setup(config);
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

  BarEvent event;
  while (RunEventLoop) {
    nanosleep(&ts, NULL);
    pthread_mutex_lock(&mutex);
    event = xdb_nextevent(Blks, NBlks, customstr);
    pthread_mutex_unlock(&mutex);
    switch (event) {
    case ReadyEvent:
      pthread_mutex_lock(&mutex);
      pthread_cond_broadcast(&cond);
      pthread_mutex_unlock(&mutex);
      break;
    case DrawEvent:
      UpdateBar(Custom, customstr);
      break;
    case NoActionEvent:
      break;
    }
  }

  // doesn't make any sense to keep the thread running, after event loop stops.
  // potential corruptions detected by AddressSanitizer, but what the hell, the
  // program freakin **quits**.
  pthread_cancel(thread_handle);
  xdb_cleanup();
  return 0;
}
