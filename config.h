#include <stdbool.h>

#define NAME "xdbar"
#define VERSION "0.1.0"

#define TAG_START "<"
#define TAG_END ">"

struct BarConfig {
  unsigned int x, y, w, h;
  bool topbar;
  struct {
    unsigned int left, right, top, bottom;
  } padding, margin;
  char *foreground, *background;
};

static const struct BarConfig barConfig = {.x = 0,
                                           .y = 768 - 32,
                                           .w = 1366,
                                           .h = 32,
                                           .topbar = false,
                                           .padding = {0, 0, 0, 0},
                                           .margin = {0, 0, 0, 0},
                                           .foreground = "#efefef",
                                           .background = "#090909"};

// This cannot be empty, first font is default;
static const char *const fonts[] = {"sans-serif:size=9",
                                    "sans-serif:size=9:bold"};
