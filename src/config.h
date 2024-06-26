/* These are the compile time configs, which are loaded by default.
 * Any runtime configs (e.g 'clubar.lua', '.Xresources'), if used, gets merged
 * into these.
 *
 * Pros: Built into the program, rather than loading from external source,
 * everytime the program runs.
 * Cons: Recompilation is necessary, when updated.
 */
#include <clubar.h>

static const Geometry geometry = {.x = 0, .y = 0, .w = 1366, .h = 32};
static const Direction padding = {.left = 0, .right = 0, .top = 0, .bottom = 0};
static const Direction margin  = {.left = 0, .right = 0, .top = 0, .bottom = 0};
static const int topbar        = 1;
static const char foreground[] = "#090909";
static const char background[] = "#efefef";

// This cannot be empty, first font is the default;
static const char *const fonts[] = {"monospace-9", "monospace-9:bold"};
