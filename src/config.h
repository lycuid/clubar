/* These are the compile time configs, which are loaded by default.
 * Any runtime configs (e.g 'clubar.lua', '.Xresources'), if used, gets merged
 * into these.
 *
 * Pros: Built into the program, rather than loading from external source,
 * everytime the program runs.
 * Cons: Recompilation is necessary, when updated.
 */
#include <clubar/core.h>

static const BarConfig barConfig = {
    .geometry   = {0, 768 - 32, 1366, 32},
    .padding    = {0, 0, 0, 0},
    .margin     = {0, 0, 0, 0},
    .topbar     = 0,
    .foreground = "#efefef",
    .background = "#090909",
};

// This cannot be empty, first font is the default;
static const char *const fonts[] = {"monospace-9", "monospace-9:bold"};
