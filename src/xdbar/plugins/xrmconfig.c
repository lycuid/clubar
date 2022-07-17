/* Enable Plugin with: `make PLUGINS=xrmconfig`
 * This plugin enables runtime configuration support using X Resources.
 * check 'examples' directory for sample configs.
 */
#include "xrmconfig.h"
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void merge_xrm_config(Config *config)
{
  char *value;
  XrmValue xrm_value;
  BarConfig *barConfig = &config->barConfig;

  Display *dpy;
  if (!(dpy = XOpenDisplay(NULL)))
    die("unable to open display.\n");

  XrmInitialize();
  const char *resm = XResourceManagerString(dpy);
  XrmDatabase db   = XrmGetStringDatabase(resm);

#define Chunk(x_ptr, x_ptr_start, x_ptr_end, xs, nxs)                          \
  {                                                                            \
    int size = x_ptr_end - x_ptr_start + 1;                                    \
    xs       = realloc(xs, (nxs + 1) * sizeof(char *));                        \
    xs[nxs]  = malloc(size);                                                   \
    memcpy(xs[nxs], x_ptr[x_ptr_start], size);                                 \
    xs[nxs++][size - 1] = 0;                                                   \
    x_ptr_start         = x_ptr_end + 1;                                       \
  }
  if (XrmGetResource(db, NAME ".fonts", "*", &value, &xrm_value)) {
    size_t start, current;
    config->fonts  = NULL;
    config->nfonts = 0;
    for (start = current = 0; current < xrm_value.size; ++current) {
      if (xrm_value.addr[current] == ',' && start < current)
        Chunk(&xrm_value.addr, start, current, config->fonts, config->nfonts);
    }
    if (start < current)
      Chunk(&xrm_value.addr, start, xrm_value.size, config->fonts,
            config->nfonts);
  }
#undef Chunk

  if (XrmGetResource(db, NAME ".barConfig.geometry", "*", &value, &xrm_value))
    if (sscanf(xrm_value.addr, "%u,%u,%u,%u", &barConfig->geometry.x,
               &barConfig->geometry.y, &barConfig->geometry.w,
               &barConfig->geometry.h) != 4)
      die("Invalid Xrm config: 'barConfig.geometry'.\n");

  if (XrmGetResource(db, NAME ".barConfig.padding", "*", &value, &xrm_value))
    if (sscanf(xrm_value.addr, "%u,%u,%u,%u", &barConfig->padding.left,
               &barConfig->padding.right, &barConfig->padding.top,
               &barConfig->padding.bottom) != 4)
      die("Invalid Xrm config: 'barConfig.padding'.\n");

  if (XrmGetResource(db, NAME ".barConfig.margin", "*", &value, &xrm_value))
    if (sscanf(xrm_value.addr, "%u,%u,%u,%u", &barConfig->margin.left,
               &barConfig->margin.right, &barConfig->margin.top,
               &barConfig->margin.bottom) != 4)
      die("Invalid Xrm config: 'barConfig.margin'.\n");

  if (XrmGetResource(db, NAME ".barConfig.topbar", "*", &value, &xrm_value))
    if (xrm_value.size)
      config->barConfig.topbar = memcmp(xrm_value.addr, "true", 4) == 0 ? 1
                                 : memcmp(xrm_value.addr, "false", 5) == 0
                                     ? 0
                                     : config->barConfig.topbar;

  if (XrmGetResource(db, NAME ".barConfig.foreground", "*", &value, &xrm_value))
    memcpy(config->barConfig.foreground, xrm_value.addr, xrm_value.size);

  if (XrmGetResource(db, NAME ".barConfig.background", "*", &value, &xrm_value))
    memcpy(config->barConfig.background, xrm_value.addr, xrm_value.size);

  XrmDestroyDatabase(db);
  XCloseDisplay(dpy);
}
