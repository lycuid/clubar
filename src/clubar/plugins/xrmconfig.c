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

void xrmconfig_merge(Config *config)
{
    char *value;
    XrmValue xrm_value;

    Display *dpy;
    if (!(dpy = XOpenDisplay(NULL)))
        die("unable to open display.\n");

    XrmInitialize();
    const char *resm = XResourceManagerString(dpy);
    XrmDatabase db   = XrmGetStringDatabase(resm);

    if (XrmGetResource(db, NAME ".fonts", "*", &value, &xrm_value) &&
        xrm_value.size > 0)
        load_fonts_from_string(xrm_value.addr);

    if (XrmGetResource(db, NAME ".geometry", "*", &value, &xrm_value))
        if (sscanf(xrm_value.addr, "%u,%u,%u,%u", &config->geometry.x,
                   &config->geometry.y, &config->geometry.w,
                   &config->geometry.h) != 4)
            die("Invalid Xrm config: 'geometry'.\n");

    if (XrmGetResource(db, NAME ".padding", "*", &value, &xrm_value))
        if (sscanf(xrm_value.addr, "%u,%u,%u,%u", &config->padding.left,
                   &config->padding.right, &config->padding.top,
                   &config->padding.bottom) != 4)
            die("Invalid Xrm config: 'padding'.\n");

    if (XrmGetResource(db, NAME ".margin", "*", &value, &xrm_value))
        if (sscanf(xrm_value.addr, "%u,%u,%u,%u", &config->margin.left,
                   &config->margin.right, &config->margin.top,
                   &config->margin.bottom) != 4)
            die("Invalid Xrm config: 'margin'.\n");

    if (XrmGetResource(db, NAME ".topbar", "*", &value, &xrm_value))
        if (xrm_value.size)
            config->topbar = memcmp(xrm_value.addr, "true", 4) == 0 ? 1
                             : memcmp(xrm_value.addr, "false", 5) == 0
                                 ? 0
                                 : config->topbar;

    if (XrmGetResource(db, NAME ".foreground", "*", &value, &xrm_value))
        memcpy(config->foreground, xrm_value.addr, xrm_value.size);

    if (XrmGetResource(db, NAME ".background", "*", &value, &xrm_value))
        memcpy(config->background, xrm_value.addr, xrm_value.size);

    XrmDestroyDatabase(db);
    XCloseDisplay(dpy);
}
