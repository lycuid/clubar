#ifndef __CORE_H__
#define __CORE_H__

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <xdbar/core/blocks.h>

#define MAX_BLKS (1 << 6)

#define eprintf(...) fprintf(stderr, __VA_ARGS__);
#define die(...)                                                               \
  {                                                                            \
    eprintf("[ERROR] " __VA_ARGS__);                                           \
    exit(1);                                                                   \
  }

typedef enum { ReadyEvent, RenderEvent, ResetEvent, NoActionEvent } XDBEvent;
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
  bool running;
  Block *blks[2];
  int nblks[2];
  Config config;
  void (*init)(int argc, char *const *argv);
  void (*update_blks)(BlockType, const char *);
  void (*stop_running)();
} * core;

// These are supposed to be implemented by whatever 'backend' thats being used.
void xdb_setup(void);
void xdb_clear(BlockType);
void xdb_render(BlockType);
void xdb_toggle();
XDBEvent xdb_nextevent(char[BLK_BUFFER_SIZE]);
void xdb_cleanup(void);

#endif
