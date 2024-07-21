/* These are the compile time configs, which are loaded by default.
 * Any runtime configs (e.g 'clubar.lua', '.Xresources'), if used, gets merged
 * into these.
 *
 * Pros: Built into the program, rather than loading from external source,
 * everytime the program runs.
 * Cons: Recompilation is necessary, when updated.
 */
#include <clubar.h>

#define BAR_HEIGHT 32

static const Geometry geometry = {
    .x = 0,
    .y = 768 - BAR_HEIGHT,
    .w = 1366,
    .h = BAR_HEIGHT,
};

static const Direction padding = {.left = 0, .right = 0, .top = 0, .bottom = 0};

static const Direction margin = {.left = 0, .right = 0, .top = 0, .bottom = 0};

static const int topbar = 0;

static const char border[] = ""; // format -> "color:width" (e.g. "#ee33ee:2"
                                 // or "#ee33ee", default width=1)

static const char foreground[] = "#efefef";

static const char background[] = "#090909";

// This cannot be empty, first font is the default;
static const char *const fonts[] = {"monospace-9", "monospace-9:bold"};

#undef BAR_HEIGHT
