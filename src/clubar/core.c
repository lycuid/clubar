#include "core.h"
#include "config.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __ENABLE_PLUGIN__xrmconfig__
#include <clubar/plugins/xrmconfig.h>
#endif
#ifdef __ENABLE_PLUGIN__luaconfig__
#include <clubar/plugins/luaconfig.h>
#endif

static inline void argparse(void);
static inline void create_config(void);
void core_init(void);
void core_load_external_configs(void);
void core_update_blks(BlockType, const char *);
void core_stop_running(void);

static CliArgs local_cli_args = {0, NULL};
CliArgs *cli_args = &local_cli_args;

static Block blks[2][MAX_BLKS];

static struct Core local = {
    .running               = true,
    .nblks                 = {0, 0},
    .init                  = core_init,
    .load_external_configs = core_load_external_configs,
    .update_blks           = core_update_blks,
    .stop_running          = core_stop_running,
};

const struct Core *const core = &local;
static char ConfigFile[1 << 10];

void load_fonts_from_string(char *str)
{
    Config *c = &local.config;
    if (c->fonts) {
        for (int i = 0; i < c->nfonts; ++i)
            free(c->fonts[i]);
        free(c->fonts);
    }
    c->fonts = NULL, c->nfonts = 0;
    for (char *token = strtok(str, ","); token; token = strtok(0, ",")) {
        while (token && *token == ' ')
            token++;
        int len = strlen(token) - 1;
        while (len >= 0 && token[len] == ' ')
            token[len--] = 0;
        if (len > 0) {
            c->fonts = realloc(c->fonts, (c->nfonts + 1) * sizeof(char *));
            c->fonts[c->nfonts++] = strdup(token);
        }
    }
}

#define CONFIG_GEOMETRY   "geometry"
#define CONFIG_PADDING    "padding"
#define CONFIG_MARGIN     "margin"
#define CONFIG_TOPBAR     "topbar"
#define CONFIG_FOREGROUND "foreground"
#define CONFIG_BACKGROUND "background"
#define CONFIG_FONTS      "fonts"

static inline void usage(void)
{ // clang-format off
    puts("USAGE: " NAME " [OPTIONS]...");
    puts("OPTIONS:");
    puts("  -h, --help      print this help message.");
    puts("  -v, --version   print version.");
    puts("  -t, --topbar    window position (top/bottom edge of the screen).");
    puts("  -c file, --config[=file]");
    puts("                  filepath for runtime configs (supports: lua).");
    puts("  -g values, --" CONFIG_GEOMETRY "[=values]");
    puts("                  window geometry as 'x,y,width,height' (eg: '0,0,1280,720').");
    puts("  -p values, --" CONFIG_PADDING "[=values]");
    puts("                  window padding as 'left,right,top,bot' (eg: '0,0,10,10').");
    puts("  -m values, --" CONFIG_MARGIN "[=values]");
    puts("                  window margin as 'left,right,top,bot' (eg: '0,0,10,10').");
    puts("  -f color, --" CONFIG_FOREGROUND "[=color]");
    puts("                  set default foreground color.");
    puts("  -b color, --" CONFIG_BACKGROUND "[=color]");
    puts("                  set default background color.");
    puts("  --" CONFIG_FONTS "[=values]");
    puts("                  comma seperated fonts (eg: 'arial-10,monospace-10:bold').");
    puts("SIGNALS:");
    puts("  USR1: toggle window visibility (e.g. pkill -USR1 clubar).");
} // clang-format on

static inline void argparse(void)
{
    Config *c = &local.config;
    int arg, i = 0;

    // clang-format off
    struct option opts[] = {
        {CONFIG_GEOMETRY,   required_argument,  0, 'g'},
        {CONFIG_PADDING,    required_argument,  0, 'p'},
        {CONFIG_MARGIN,     required_argument,  0, 'm'},
        {CONFIG_TOPBAR,     no_argument,        &c->topbar, 1},
        {CONFIG_FOREGROUND, required_argument,  0, 'f'},
        {CONFIG_BACKGROUND, required_argument,  0, 'b'},
        {CONFIG_FONTS,      required_argument,  0, 0},
        {"help",            no_argument,        0, 'h'},
        {"version",         no_argument,        0, 'v'},
        {"config",          required_argument,  0, 'c'},
        {NULL,              0,                  NULL, 0},
    }; // clang-format on

    while ((arg = getopt_long(cli_args->argc, (char *const *)cli_args->argv,
                              "hvtc:g:p:m:f:b:", opts, &i)) != -1) {
        switch (arg) {
        case 0: {
            if (strcmp(CONFIG_FONTS, opts[i].name) == 0 && optarg)
                load_fonts_from_string(optarg);
        } break;
        case 'h': {
            usage();
            exit(EXIT_SUCCESS);
        } break;
        case 'v': {
            puts(NAME ": v" VERSION);
            exit(EXIT_SUCCESS);
        } break;
        case 't': {
            c->topbar = 1;
        } break;
        case 'g': {
            if (optarg)
                if (sscanf(optarg, "%u,%u,%u,%u", &c->geometry.x,
                           &c->geometry.y, &c->geometry.w, &c->geometry.h) != 4)
                    die("Invalid value for argument: '" CONFIG_GEOMETRY "'.\n");
        } break;
        case 'p': {
            if (optarg)
                if (sscanf(optarg, "%u,%u,%u,%u", &c->padding.left,
                           &c->padding.right, &c->padding.top,
                           &c->padding.bottom) != 4)
                    die("Invalid value for argument: '" CONFIG_PADDING "'.\n");
        } break;
        case 'm': {
            if (optarg)
                if (sscanf(optarg, "%u,%u,%u,%u", &c->padding.left,
                           &c->padding.right, &c->padding.top,
                           &c->padding.bottom) != 4)
                    die("Invalid value for argument: '" CONFIG_MARGIN "'.\n");
        } break;
        case 'f': {
            if (optarg)
                strcpy(c->foreground, optarg);
        } break;
        case 'b': {
            if (optarg)
                strcpy(c->background, optarg);
        } break;
        case 'c':
            if (optarg)
                strcpy(ConfigFile, optarg);
            break;
        default: exit(2);
        }
    }
}

#undef CONFIG_GEOMETRY
#undef CONFIG_PADDING
#undef CONFIG_MARGIN
#undef CONFIG_TOPBAR
#undef CONFIG_FOREGROUND
#undef CONFIG_BACKGROUND
#undef CONFIG_FONTS

static inline void create_config(void)
{
    local.config.nfonts = sizeof(fonts) / sizeof(*fonts);
    local.config.fonts  = malloc(local.config.nfonts * sizeof(char *));
    for (int i = 0; i < local.config.nfonts; ++i)
        local.config.fonts[i] = strdup(fonts[i]);
    local.config.geometry = geometry;
    local.config.padding  = padding;
    local.config.margin   = margin;
    local.config.topbar   = topbar;
    strcpy(local.config.foreground, foreground);
    strcpy(local.config.background, background);
}

void core_init(void)
{
    local.blks[Stdin]  = blks[Stdin];
    local.blks[Custom] = blks[Custom];
    create_config();
    argparse();
    core_load_external_configs();
}

void core_load_external_configs(void)
{
    blks_free(local.blks[Stdin], MAX_BLKS);
    blks_free(local.blks[Custom], MAX_BLKS);

#ifdef __ENABLE_PLUGIN__xrmconfig__
    xrmconfig_merge(&local.config);
#endif
#ifdef __ENABLE_PLUGIN__luaconfig__
    luaconfig_merge(ConfigFile, &local.config);
#endif
}

void core_update_blks(BlockType blktype, const char *buffer)
{
    blks_free(local.blks[blktype], MAX_BLKS);
    local.nblks[blktype] = blks_create(local.blks[blktype], buffer);
}

void core_stop_running(void) { local.running = false; }
