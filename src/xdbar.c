#include "config.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <xdbar/core/blocks.h>
#include <xdbar/x.h>
#ifdef __ENABLE_PLUGIN__xrmconfig__
#include <xdbar/plugins/xrmconfig.h>
#endif
#ifdef __ENABLE_PLUGIN__luaconfig__
#include <xdbar/plugins/luaconfig.h>
#endif

#define ThreadLocked(block)                                                    \
  {                                                                            \
    pthread_mutex_lock(&mutex);                                                \
    block;                                                                     \
    pthread_mutex_unlock(&mutex);                                              \
  }

#define UpdateBar(blktype, buffer)                                             \
  {                                                                            \
    ThreadLocked(xdb_clear(blktype, NBlks[blktype]));                          \
                                                                               \
    blks_free(Blks[blktype], MAX_BLKS);                                        \
    NBlks[blktype] = blks_create(buffer, Blks[blktype]);                       \
                                                                               \
    ThreadLocked(xdb_render(blktype, Blks[blktype], NBlks[blktype]));          \
  }

pthread_t thread_handle;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond   = PTHREAD_COND_INITIALIZER;
Block Blks[2][MAX_BLKS];
int NBlks[2] = {0, 0}, RunEventLoop = 1;
char *ConfigFile = NULL;

void *stdin_handler();
static inline void create_config(Config *);
static inline void setup(Config *);
void gracefully_exit();

void *stdin_handler()
{
  ThreadLocked(pthread_cond_wait(&cond, &mutex));

  ssize_t nbuf;
  size_t size    = BLOCK_BUF_SIZE;
  char *stdinstr = malloc(size), previous[size];
  memset(stdinstr, 0, size);
  memset(previous, 0, size);

  while ((nbuf = getline(&stdinstr, &size, stdin)) > 1) {
    // trim off newline.
    stdinstr[nbuf - 1] = 0;
    // painting is expensive.
    if (strcmp(previous, stdinstr) == 0)
      continue;

    UpdateBar(Stdin, stdinstr);
    memcpy(previous, stdinstr, BLOCK_BUF_SIZE);
    memset(stdinstr, 0, BLOCK_BUF_SIZE);
  }
  pthread_exit(0);
}

static inline void create_config(Config *config)
{
  config->nfonts    = sizeof(fonts) / sizeof(*fonts);
  config->fonts     = (char **)fonts;
  config->barConfig = barConfig;

#ifdef __ENABLE_PLUGIN__xrmconfig__
  xrmconfig_merge(config);
#endif
#ifdef __ENABLE_PLUGIN__luaconfig__
  luaconfig_merge(ConfigFile, config);
#endif
}

static inline void setup(Config *config)
{
  pthread_create(&thread_handle, NULL, stdin_handler, NULL);
  xdb_setup(config);
  signal(SIGINT, gracefully_exit);
  signal(SIGHUP, gracefully_exit);
  signal(SIGTERM, gracefully_exit);
}

void gracefully_exit() { RunEventLoop = 0; }

int main(int argc, char **argv)
{
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
    ThreadLocked(event = xdb_nextevent(Blks, NBlks, customstr));
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

  pthread_cancel(thread_handle);
  xdb_cleanup();
  return 0;
}
