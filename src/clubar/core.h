#ifndef __CORE_H__
#define __CORE_H__

#include <clubar/core/blocks.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_BLKS (1 << 6)

#define eprintf(...) fprintf(stderr, __VA_ARGS__);
#define die(...)                                                               \
  {                                                                            \
    eprintf("[ERROR] " __VA_ARGS__);                                           \
    exit(1);                                                                   \
  }

typedef enum {
  CLU_Ready,    // Window's canvas is ready and can now be drawn upon.
  CLU_NewValue, // New Value was just populated in the provided buffer.
  CLU_Reset,    // Reset window canvas.
  CLU_NoOp,     // NoOP.
} CluEvent;
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
  void (*stop_running)(void);
} *core;

/* These functions are supposed to be implemented by whichever 'frontend' that
 * is being used. */

// Setup and show window.
void clu_setup(void);
// Clear window canvas.
void clu_clear(BlockType);
// render on the window canvas.
void clu_render(BlockType);
// toggle window visibility.
void clu_toggle(int);
// get next window event.
CluEvent clu_nextevent(char[BLK_BUFFER_SIZE]);
// cleanup memory allocs and stuff and kill window.
void clu_cleanup(void);

#endif
