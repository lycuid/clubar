#include "core.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __ENABLE_PLUGIN__xrmconfig__
#include <xdbar/plugins/xrmconfig.h>
#endif
#ifdef __ENABLE_PLUGIN__luaconfig__
#include <xdbar/plugins/luaconfig.h>
#endif

static inline void argparse(int, char *const *);
static inline void create_config(Config *);
void core_init(int argc, char *const *argv, Config *);
void core_update_blks(BlockType, const char *);
void core_stop_running();

static struct Core local = {
    .nblks        = {0, 0},
    .running      = 1,
    .init         = core_init,
    .update_blks  = core_update_blks,
    .stop_running = core_stop_running,
};

const struct Core *core = &local;
static char *ConfigFile = NULL;

static inline void argparse(int argc, char *const *argv)
{
  char arg;
  while ((arg = getopt(argc, argv, "hvc:")) != -1) {
    switch (arg) {
    case 'h':
      puts("USAGE: " NAME " [FLAGS|OPTIONS]");
      puts("FLAGS:");
      puts("  -h    print this help message.");
      puts("  -v    print version.");
      puts("OPTIONS:");
      puts("  -c <config_file>");
      puts("        filepath for runtime configs (supports: lua).");
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

void core_init(int argc, char *const *argv, Config *config)
{
  argparse(argc, argv);
  create_config(config);
}

void core_update_blks(BlockType blktype, const char *buffer)
{
  blks_free(local.blks[blktype], MAX_BLKS);
  local.nblks[blktype] = blks_create(buffer, local.blks[blktype]);
}

void core_stop_running() { local.running = 0; }
