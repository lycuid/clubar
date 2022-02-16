#include "x.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
  int x, width;
} GlyphInfo;

typedef struct _ColorCache {
  char name[32];
  XftColor val;
  struct _ColorCache *previous;
} ColorCache;

struct Draw {
  int nfonts;
  Visual *vis;
  Colormap cmap;
  XftFont **fonts;
  GlyphInfo gis[2][MAX_BLKS];
  ColorCache *colorcache;
} drw;

struct Bar {
  Window xwindow;
  XftDraw *canvas;
  Geometry window_g, canvas_g;
  XftColor foreground, background;
} bar;

#define ClearBar(x, y, w, h)                                                   \
  XftDrawRect(bar.canvas, &bar.background, x, y, w, h)

Display *dpy;
Window root;
int scr;
XEvent e;
char *wm_name;
static Atom AtomWMName;
static int BarExposed = 0;

XftColor *get_cached_color(const char *colorname) {
  ColorCache *cache = drw.colorcache;
  while (cache != NULL) {
    if (strcmp(cache->name, colorname) == 0)
      return &cache->val;
    cache = cache->previous;
  }

  // @TODO: currently not handling error when allocating colors.
  ColorCache *color = (ColorCache *)malloc(sizeof(ColorCache));
  XftColorAllocName(dpy, drw.vis, drw.cmap, colorname, &color->val);

  strcpy(color->name, colorname);
  color->previous = drw.colorcache;
  drw.colorcache = color;

  return &drw.colorcache->val;
}

int parse_box_string(const char *val, char color[32]) {
  int ptr = 0, c = 0, nval = strlen(val), size = 0;
  memset(color, 0, 32);

  while (ptr < nval && val[ptr] != ':')
    color[c++] = val[ptr++];

  if (val[ptr++] == ':')
    while (ptr < nval && val[ptr] >= '0' && val[ptr] <= '9')
      size = (size * 10) + val[ptr++] - '0';

  if (ptr < nval - 1)
    die("Invalid Box template string: %s\n", val);

  return size > 0 ? size : 1;
}

void createdrw(const Config *config) {
  drw.vis = DefaultVisual(dpy, scr);
  drw.cmap = DefaultColormap(dpy, scr);

  drw.nfonts = config->nfonts;

  drw.fonts = (XftFont **)malloc(drw.nfonts * sizeof(XftFont *));
  for (int i = 0; i < drw.nfonts; ++i)
    drw.fonts[i] = XftFontOpenName(dpy, scr, config->fonts[i]);

  drw.colorcache = NULL;
}

void createbar(const BarConfig *barConfig) {
  Geometry geometry = barConfig->geometry;
  bar.window_g = (Geometry){geometry.x, geometry.y, geometry.w, geometry.h};

  bar.canvas_g.x = barConfig->padding.left;
  bar.canvas_g.y = barConfig->padding.top;
  bar.canvas_g.w =
      bar.window_g.w - barConfig->padding.left - barConfig->padding.right;
  bar.canvas_g.h =
      bar.window_g.h - barConfig->padding.top - barConfig->padding.bottom;

  bar.xwindow =
      XCreateSimpleWindow(dpy, root, bar.window_g.x, bar.window_g.y,
                          bar.window_g.w, bar.window_g.h, 0, 0xffffff, 0);

  XftColorAllocName(dpy, drw.vis, drw.cmap, barConfig->foreground,
                    &bar.foreground);
  XftColorAllocName(dpy, drw.vis, drw.cmap, barConfig->background,
                    &bar.background);

  bar.canvas = XftDrawCreate(dpy, bar.xwindow, drw.vis, drw.cmap);
}

void xsetup(const Config *config) {
  for (int i = 0; i < MAX_BLKS; ++i)
    drw.gis[Stdin][i] = drw.gis[Custom][i] = (GlyphInfo){0, 0};

  if ((dpy = XOpenDisplay(NULL)) == NULL)
    die("Cannot open display.\n");
  root = DefaultRootWindow(dpy);
  scr = DefaultScreen(dpy);
  AtomWMName = XInternAtom(dpy, "WM_NAME", 0);

  createdrw(config);
  createbar(&config->barConfig);

  XSelectInput(dpy, bar.xwindow, ExposureMask | ButtonPressMask);
  xsetatoms(&config->barConfig);
  XMapWindow(dpy, bar.xwindow);
}

void xsetatoms(const BarConfig *barConfig) {
  long barheight =
      bar.window_g.h + barConfig->margin.top + barConfig->margin.bottom;
  long top = barConfig->topbar ? barheight : 0;
  long bottom = !barConfig->topbar ? barheight : 0;
  /* left, right, top, bottom, left_start_y, left_end_y, right_start_y,
   * right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x */
  long strut[12] = {0, 0, top, bottom, 0, 0, 0, 0, 0, 0, 0, 0};

  XChangeProperty(dpy, bar.xwindow, XInternAtom(dpy, "_NET_WM_STRUT", 0),
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 4);
  XChangeProperty(dpy, bar.xwindow,
                  XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", 0), XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)strut, 12l);

  Atom property[2];
  property[0] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", 0);
  property[1] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", 0);
  XChangeProperty(dpy, bar.xwindow, property[1], XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)property, 1l);

  XClassHint *classhint = XAllocClassHint();
  classhint->res_name = NAME;
  classhint->res_class = NAME;
  XSetClassHint(dpy, bar.xwindow, classhint);
  XFree(classhint);
  XStoreName(dpy, bar.xwindow, NAME);
}

void xcleanup(void) {
  while (drw.colorcache != NULL) {
    XftColorFree(dpy, drw.vis, drw.cmap, &drw.colorcache->val);
    ColorCache *stale = drw.colorcache;
    drw.colorcache = drw.colorcache->previous;
    free(stale);
  }

  for (int fn = 0; fn < drw.nfonts; ++fn)
    XftFontClose(dpy, drw.fonts[fn]);

  XftColorFree(dpy, drw.vis, drw.cmap, &bar.foreground);
  XftColorFree(dpy, drw.vis, drw.cmap, &bar.background);
  XftDrawDestroy(bar.canvas);
  XCloseDisplay(dpy);
}

void xrenderblks(BlockType blktype, const Block blks[MAX_BLKS], int nblk) {
  int starty, size, fnindex;
  char color[32];
  XftColor *fg;
  Tag *box;
  Geometry *canvas_g = &bar.canvas_g;

  for (int i = 0; i < nblk; ++i) {
    const Block *blk = &blks[i];
    const GlyphInfo *gi = &drw.gis[blktype][i];

    if (blk->tags[Bg] != NULL)
      XftDrawRect(bar.canvas, get_cached_color(blk->tags[Bg]->val), gi->x,
                  canvas_g->y, gi->width, canvas_g->h);

    box = blk->tags[Box];
    while (box != NULL) {
      size = parse_box_string(box->val, color);
      if (size) {
        int bx = canvas_g->x, by = canvas_g->y, bw = 0, bh = 0;
        switch (box->modifier) {
        case Top:
          bx = gi->x, bw = gi->width, bh = size;
          break;
        case Bottom:
          bx = gi->x, by = canvas_g->y + canvas_g->h - size, bw = gi->width,
          bh = size;
          break;
        case Left:
          bx = gi->x, bw = size, bh = canvas_g->h;
          break;
        case Right:
          bx = gi->x + gi->width - size, bw = size, bh = canvas_g->h;
          break;
        default:
          break;
        }
        XftColor *rectcolor = get_cached_color(color);
        XftDrawRect(bar.canvas, rectcolor, bx, by, bw, bh);
      }
      box = box->previous;
    }

    fnindex = blk->tags[Fn] != NULL ? atoi(blk->tags[Fn]->val) : 0;
    starty = canvas_g->y + (canvas_g->h - drw.fonts[fnindex]->height) / 2 +
             drw.fonts[fnindex]->ascent;
    fg = blk->tags[Fg] != NULL ? get_cached_color(blk->tags[Fg]->val)
                               : &bar.foreground;
    XftDrawStringUtf8(bar.canvas, fg, drw.fonts[fnindex], gi->x, starty,
                      (FcChar8 *)blk->text, blk->ntext);
  }
}

void clearblks(BlockType blktype, int nblks) {
  if (nblks) {
    GlyphInfo *first = &drw.gis[blktype][0],
              *last = &drw.gis[blktype][nblks - 1];
    ClearBar(first->x, 0, last->x + last->width, bar.window_g.h);
  }
}

__inline__ void prepare_stdinblks(const Block blks[MAX_BLKS], int nblks) {
  XGlyphInfo extent;
  int fnindex, startx = 0;
  for (int i = 0; i < nblks; ++i) {
    const Block *blk = &blks[i];
    fnindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) : 0;
    XftTextExtentsUtf8(dpy, drw.fonts[fnindex], (FcChar8 *)blk->text,
                       blk->ntext, &extent);

    drw.gis[Stdin][i].width = extent.xOff;
    drw.gis[Stdin][i].x = startx + extent.x;
    startx += drw.gis[Stdin][i].width;
  }
}

__inline__ void prepare_customblks(const Block blks[MAX_BLKS], int nblks) {
  XGlyphInfo extent;
  int fnindex, startx = bar.canvas_g.x + bar.canvas_g.w;
  for (int i = nblks - 1; i >= 0; --i) {
    const Block *blk = &blks[i];
    fnindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) : 0;
    XftTextExtentsUtf8(dpy, drw.fonts[fnindex], (FcChar8 *)blk->text,
                       blk->ntext, &extent);

    drw.gis[Custom][i].width = extent.xOff;
    startx -= extent.x + extent.xOff;
    drw.gis[Custom][i].x = startx;
  }
}

void renderblks(BlockType blktype, const Block blks[MAX_BLKS], int nblks) {
  switch (blktype) {
  case Stdin:
    prepare_stdinblks(blks, nblks);
    break;
  case Custom:
    prepare_customblks(blks, nblks);
    break;
  }
  xrenderblks(blktype, blks, nblks);
}

int onExpose(Block blks[2][MAX_BLKS], int nblks[2]) {
  ClearBar(0, 0, bar.window_g.w, bar.window_g.h);
  if (!BarExposed && (BarExposed = 1)) {
    XSelectInput(dpy, root, PropertyChangeMask);
    XStoreName(dpy, root, NAME "-" VERSION);
    return 1;
  }
  renderblks(Stdin, blks[Stdin], nblks[Stdin]);
  renderblks(Custom, blks[Custom], nblks[Custom]);
  return 0;
}

void onButtonPress(const XEvent *xe, Block blks[2][MAX_BLKS], int nblks[2]) {
  const XButtonEvent *e = &xe->xbutton;
  for (int i = 0; i < nblks[Stdin] + nblks[Custom]; ++i) {
    Block *blk =
        i < nblks[Stdin] ? &blks[Stdin][i] : &blks[Custom][i - nblks[Stdin]];
    GlyphInfo *gi = i < nblks[Stdin] ? &drw.gis[Stdin][i]
                                     : &drw.gis[Custom][i - nblks[Stdin]];

    if (e->x >= gi->x && e->x <= gi->x + gi->width) {
      TagKey key = e->button == Button1   ? BtnL
                   : e->button == Button2 ? BtnM
                   : e->button == Button3 ? BtnR
                   : e->button == Button4 ? ScrlU
                   : e->button == Button5 ? ScrlD
                                          : NullKey;

      TagModifier mod = e->state == ShiftMask     ? Shift
                        : e->state == ControlMask ? Ctrl
                        : e->state == Mod1Mask    ? Super
                        : e->state == Mod4Mask    ? Alt
                                                  : NullModifier;

      if (key != NullKey) {
        Tag *action = blk->tags[key];
        while (action != NULL) {
          if (strlen(action->val) && action->modifier == mod)
            system(action->val);
          action = action->previous;
        }
      }
      break;
    }
  }
}

int onPropertyNotify(const XEvent *xe, char *name) {
  const XPropertyEvent *e = &xe->xproperty;
  if (e->window == root && e->atom == AtomWMName) {
    if (XFetchName(dpy, root, &wm_name) >= 0 && wm_name) {
      strcpy(name, wm_name);
      XFree(wm_name);
      return 1;
    }
  }
  return 0;
}

BarEvent handle_xevent(Block blks[2][MAX_BLKS], int nblks[2],
                       char name[BLOCK_BUF_SIZE]) {
  if (XPending(dpy)) {
    XNextEvent(dpy, &e);

    switch (e.type) {
    // bar window events.
    case Expose:
      if (onExpose(blks, nblks))
        return ReadyEvent;
      break;
    case ButtonPress:
      onButtonPress(&e, blks, nblks);
      break;

    // root window events.
    case PropertyNotify:
      if (onPropertyNotify(&e, name))
        return DrawEvent;
      break;

    default:
      break;
    }
  }
  return NoActionEvent;
}
