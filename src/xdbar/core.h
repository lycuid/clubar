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

typedef enum {
  XDBReady,    // Window's canvas is ready and can now be drawn upon.
  XDBNewValue, // New Value was just populated in the provided buffer.
  XDBReset,    // Reset window canvas.
  XDBNoOp,     // NoOP.
} xdb_event_t;
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

/* These functions are supposed to be implemented by whichever 'backend' that is
 * being used. */

// Setup and show window.
void xdb_setup(void);
// Clear window canvas.
void xdb_clear(BlockType);
// render on the window canvas.
void xdb_render(BlockType);
// toggle window visibility.
void xdb_toggle();
// get next window event.
xdb_event_t xdb_nextevent(char[BLK_BUFFER_SIZE]);
// cleanup memory allocs and stuff and kill window.
void xdb_cleanup(void);

#endif
