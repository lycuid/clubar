/* Enable Patch with: `make CFLAGS=xrmconfig`
 * This patch enables runtime configuration support using X Resources.
 */
#include "xrm.h"
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void merge_xrm_config(Config *config) {
  char *value;
  XrmValue xrm_value;
  BarConfig *barConfig = &config->barConfig;

  Display *dpy;
  if (!(dpy = XOpenDisplay(NULL)))
    die("unable to open display.\n");

  XrmInitialize();
  const char *resm = XResourceManagerString(dpy);
  XrmDatabase db = XrmGetStringDatabase(resm);

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
