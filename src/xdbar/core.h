#ifndef __CORE_H__
#define __CORE_H__

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <xdbar/core/blocks.h>

#define MAX_BLKS       (1 << 6)
#define BLOCK_BUF_SIZE (1 << 10)

#define eprintf(...) fprintf(stderr, __VA_ARGS__);
#define die(...)                                                               \
  {                                                                            \
    eprintf("[ERROR] " __VA_ARGS__);                                           \
    exit(1);                                                                   \
  }

typedef enum { ReadyEvent, DrawEvent, NoActionEvent } BarEvent;
typedef enum { Stdin, Custom } BlockType;

typedef struct {
  uint32_t x, y, w, h;
} Geometry;

typedef struct {
  int topbar : 1;
  Geometry geometry;
  struct {
    uint32_t left, right, top, bottom;
  } padding, margin;
  char foreground[16], background[16];
} BarConfig;

typedef struct {
  int nfonts;
  char **fonts;
  BarConfig barConfig;
} Config;

extern const struct Core {
  bool running : 1;
  Block blks[2][MAX_BLKS];
  int nblks[2];
  void (*init)(int argc, char *const *argv, Config *);
  void (*update_blks)(BlockType, const char *);
  void (*stop_running)();
} * core;

#endif