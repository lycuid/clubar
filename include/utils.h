#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdarg.h>
#include <stdbool.h>

#define NAME "xdbar"
#define VERSION "0.1.0"

#define MAX_BLKS (1 << 6)

#define eprintf(...) fprintf(stderr, __VA_ARGS__);
#define die(...)                                                               \
  do {                                                                         \
    eprintf("[ERROR] ");                                                       \
    eprintf(__VA_ARGS__);                                                      \
    exit(1);                                                                   \
  } while (0);

typedef enum { Stdin, Custom } BlockType;

typedef struct {
  unsigned int x, y, w, h;
} Geometry;

typedef struct {
  bool topbar;
  Geometry geometry;
  struct {
    unsigned int left, right, top, bottom;
  } padding, margin;
  char *foreground, *background;
} BarConfig;

typedef struct {
  int nfonts;
  char **fonts;
  BarConfig barConfig;
} Config;

#endif
