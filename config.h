#include "include/utils.h"

static const BarConfig barConfig = {.geometry = {0, 768 - 32, 1366, 32},
                                    .padding = {0, 0, 0, 0},
                                    .topbar = 0,
                                    .margin = {0, 0, 0, 0},
                                    .foreground = "#efefef",
                                    .background = "#090909"};

// This cannot be empty, first font is default;
static const char *const fonts[] = {"monospace-9", "monospace-9:bold"};
