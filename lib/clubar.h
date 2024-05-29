#ifndef __CLUBAR_H__
#define __CLUBAR_H__

#include <clubar/blocks.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_BLKS (1 << 6)

#define IS_SET(value, mask) (((value) & (mask)) != 0)

#define eprintf(...) fprintf(stderr, __VA_ARGS__);
#define die(...)                                                               \
    {                                                                          \
        eprintf("[ERROR] " __VA_ARGS__);                                       \
        exit(1);                                                               \
    }

typedef enum { Stdin, Custom } BlockType;

typedef struct {
    uint32_t x, y, w, h;
} Geometry;

typedef struct {
    uint32_t left, right, top, bottom;
} Direction;

typedef struct {
    int nfonts;
    char **fonts;
    int topbar;
    Geometry geometry;
    Direction padding, margin;
    char foreground[16], background[16];
} Config;

struct CliArgs {
    int argc;
    const char **argv;
};
typedef struct CliArgs CliArgs;
extern CliArgs *cli_args;

typedef struct CluBar {
    Block *blks[2];
    int nblks[2];
    Config config;
} CluBar;

typedef struct CluBar CluBar;

void load_fonts_from_string(char *, Config *);

#define GUARD(lock_expr, unlock_expr)                                          \
    for (int __cond = ((lock_expr), 1); __cond; __cond = ((unlock_expr), 0))

void clubar_init(CluBar *);
void clubar_load_external_configs(CluBar *);
void clubar_update_blks(CluBar *, BlockType, const char *);

#endif
