#include "clubar.h"
#include "../src/config.h"
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

static inline void argparse(CluBar *);
static inline void create_config(CluBar *);

static CliArgs local_cli_args = {0, NULL};
CliArgs *cli_args             = &local_cli_args;

static Block blks[2][MAX_BLKS];

static char ConfigFile[1 << 10];

void load_fonts_from_string(char *str, Config *c)
{
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
#define CONFIG_BORDER     "border"
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
    puts("  --" CONFIG_GEOMETRY " values");
    puts("                  window geometry as 'x,y,width,height' (eg: '0,0,1280,720').");
    puts("  --" CONFIG_PADDING " values");
    puts("                  window padding as 'left,right,top,bot' (eg: '0,0,10,10').");
    puts("  --" CONFIG_MARGIN " values");
    puts("                  window margin as 'left,right,top,bot' (eg: '0,0,10,10').");
    puts("  --" CONFIG_BORDER " value");
    puts("                  window border as 'color:width' (eg: '#ee33ee:2', '#ddd').");
    puts("  --" CONFIG_FOREGROUND " color");
    puts("                  set default foreground color.");
    puts("  --" CONFIG_BACKGROUND " color");
    puts("                  set default background color.");
    puts("  --" CONFIG_FONTS " values");
    puts("                  comma seperated fonts (eg: 'arial-10,monospace-10:bold').");
    puts("SIGNALS:");
    puts("  USR1: toggle window visibility (e.g. pkill -USR1 clubar).");
    puts("  USR2: Reload configurations from external config file without reloading.");
} // clang-format on

static inline void argparse(CluBar *clubar)
{
    Config *c = &clubar->config;
    int arg, i = 0;

    // clang-format off
    struct option opts[] = {
        {CONFIG_GEOMETRY,   required_argument,  0,          0 },
        {CONFIG_PADDING,    required_argument,  0,          0 },
        {CONFIG_MARGIN,     required_argument,  0,          0 },
        {CONFIG_TOPBAR,     no_argument,        &c->topbar, 1   },
        {CONFIG_BORDER,     required_argument,  0,          0 },
        {CONFIG_FOREGROUND, required_argument,  0,          0   },
        {CONFIG_BACKGROUND, required_argument,  0,          0   },
        {CONFIG_FONTS,      required_argument,  0,          0   },
        {"help",            no_argument,        0,          'h' },
        {"version",         no_argument,        0,          'v' },
        {"config",          required_argument,  0,          'c' },
        {NULL,              0,                  NULL,       0   },
    }; // clang-format on

    while ((arg = getopt_long(cli_args->argc, (char *const *)cli_args->argv,
                              "hvtc:g:p:m:b:", opts, &i)) != -1) {
        switch (arg) {
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
        case 0: {
            if (optarg) {
                if (strcmp(CONFIG_GEOMETRY, opts[i].name) == 0) {
                    if (sscanf(optarg, "%u,%u,%u,%u", &c->geometry.x,
                               &c->geometry.y, &c->geometry.w,
                               &c->geometry.h) != 4)
                        die("Invalid value for argument: '" CONFIG_GEOMETRY
                            "'.\n");
                } else if (strcmp(CONFIG_PADDING, opts[i].name) == 0) {
                    if (sscanf(optarg, "%u,%u,%u,%u", &c->padding.left,
                               &c->padding.right, &c->padding.top,
                               &c->padding.bottom) != 4)
                        die("Invalid value for argument: '" CONFIG_PADDING
                            "'.\n");
                } else if (strcmp(CONFIG_MARGIN, opts[i].name) == 0) {
                    if (sscanf(optarg, "%u,%u,%u,%u", &c->margin.left,
                               &c->margin.right, &c->margin.top,
                               &c->margin.bottom) != 4)
                        die("Invalid value for argument: '" CONFIG_MARGIN
                            "'.\n");
                } else if (strcmp(CONFIG_BORDER, opts[i].name) == 0) {
                    c->border_width =
                        parse_color_string(optarg, c->border_color);
                } else if (strcmp("config", opts[i].name) == 0) {
                    strcpy(ConfigFile, optarg);
                } else if (strcmp(CONFIG_FOREGROUND, opts[i].name) == 0) {
                    strcpy(c->foreground, optarg);
                } else if (strcmp(CONFIG_BACKGROUND, opts[i].name) == 0) {
                    strcpy(c->background, optarg);
                } else if (strcmp(CONFIG_FONTS, opts[i].name) == 0) {
                    load_fonts_from_string(optarg, c);
                }
            }
        } break;

        default: exit(2);
        }
    }
}

#undef CONFIG_GEOMETRY
#undef CONFIG_PADDING
#undef CONFIG_MARGIN
#undef CONFIG_TOPBAR
#undef CONFIG_BORDER
#undef CONFIG_FOREGROUND
#undef CONFIG_BACKGROUND
#undef CONFIG_FONTS

static inline void create_config(CluBar *clubar)
{
    clubar->config.nfonts = sizeof(fonts) / sizeof(*fonts);
    clubar->config.fonts  = malloc(clubar->config.nfonts * sizeof(char *));
    for (int i = 0; i < clubar->config.nfonts; ++i)
        clubar->config.fonts[i] = strdup(fonts[i]);
    clubar->config.geometry = geometry;
    clubar->config.padding  = padding;
    clubar->config.margin   = margin;
    clubar->config.topbar   = topbar;
    clubar->config.border_width =
        parse_color_string(border, clubar->config.border_color);
    strcpy(clubar->config.foreground, foreground);
    strcpy(clubar->config.background, background);
}

void clubar_init(CluBar *clubar)
{
    clubar->blks[Stdin]  = blks[Stdin];
    clubar->blks[Custom] = blks[Custom];
    create_config(clubar);
    argparse(clubar);
}

void clubar_load_external_configs(CluBar *clubar)
{
    blks_free(clubar->blks[Stdin], MAX_BLKS);
    blks_free(clubar->blks[Custom], MAX_BLKS);

#ifdef __ENABLE_PLUGIN__xrmconfig__
    xrmconfig_merge(&clubar->config);
#endif
#ifdef __ENABLE_PLUGIN__luaconfig__
    luaconfig_merge(ConfigFile, &clubar->config);
#endif
}

void clubar_update_blks(CluBar *clubar, BlockType blktype, const char *buffer)
{
    blks_free(clubar->blks[blktype], MAX_BLKS);
    clubar->nblks[blktype] = blks_create(clubar->blks[blktype], buffer);
}
