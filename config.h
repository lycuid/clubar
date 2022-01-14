#include "include/utils.h"

static const BarConfig barConfig = {.geometry = {0, 768 - 32, 1366, 32},
                                    .padding = {0, 0, 0, 0},
                                    .topbar = false,
                                    .margin = {0, 0, 0, 0},
                                    .foreground = "#efefef",
                                    .background = "#090909"};

// This cannot be empty, first font is default;
static const char *const fonts[] = {"sans-serif:size=9",
                                    "sans-serif:size=9:bold"};
