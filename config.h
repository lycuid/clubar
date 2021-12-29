#include <stdbool.h>

#define NAME "xdbar"
#define VERSION "0.1.0"

#define TAG_START "<"
#define TAG_END ">"

typedef struct {
  unsigned int x, y, w, h;
} Geometry;

struct BarConfig {
  bool topbar;
  Geometry geometry;
  struct {
    unsigned int left, right, top, bottom;
  } padding, margin;
  char *foreground, *background;
};

static const struct BarConfig barConfig = {.geometry = {0, 768 - 32, 1366, 32},
                                           .padding = {0, 0, 0, 0},
                                           .topbar = false,
                                           .margin = {0, 0, 0, 0},
                                           .foreground = "#efefef",
                                           .background = "#090909"};

// This cannot be empty, first font is default;
static const char *const fonts[] = {"sans-serif:size=9",
                                    "sans-serif:size=9:bold"};
