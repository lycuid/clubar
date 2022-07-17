#include "x.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>

static inline XftColor *get_cached_color(const char *);
static inline int parse_box_string(const char *, char[32]);
static inline void createdrw(const Config *);
static inline void createbar(const BarConfig *);
static inline void prepare_stdinblks(const Block[MAX_BLKS], int);
static inline void prepare_customblks(const Block[MAX_BLKS], int);
static void xrenderblks(BlockType, const Block[MAX_BLKS], int);
static void onButtonPress(const XEvent *, Block[2][MAX_BLKS], int[2]);
static bool onPropertyNotify(const XEvent *, char *);

#define ClearBar(bar, x, y, w, h)                                              \
  XftDrawRect(bar.canvas, &bar.background, x, y, w, h)

typedef struct {
  int x, width;
} GlyphInfo;

typedef struct _ColorCache {
  char name[32];
  XftColor val;
  struct _ColorCache *previous;
} ColorCache;

static struct Draw {
  int nfonts;
  Visual *vis;
  Colormap cmap;
  XftFont **fonts;
  GlyphInfo gis[2][MAX_BLKS];
  ColorCache *colorcache;
} drw;

static struct Bar {
  Window window;
  XftDraw *canvas;
  Geometry window_g, canvas_g;
  XftColor foreground, background;
} bar;

static Display *dpy;
static Window root;
static Atom ATOM_WM_NAME;

static inline XftColor *get_cached_color(const char *colorname) {
  for (ColorCache *c = drw.colorcache; c != NULL; c = c->previous)
    if (strcmp(c->name, colorname) == 0)
      return &c->val;

  // @TODO: currently not handling error when allocating colors.
  ColorCache *color = (ColorCache *)malloc(sizeof(ColorCache));
  XftColorAllocName(dpy, drw.vis, drw.cmap, colorname, &color->val);

  strcpy(color->name, colorname);
  color->previous = drw.colorcache;
  drw.colorcache = color;

  return &drw.colorcache->val;
}

static inline int parse_box_string(const char *val, char color[32]) {
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

static inline void createdrw(const Config *config) {
  int scr = DefaultScreen(dpy);
  drw.vis = DefaultVisual(dpy, scr);
  drw.cmap = DefaultColormap(dpy, scr);

  drw.nfonts = config->nfonts;

  drw.fonts = (XftFont **)malloc(drw.nfonts * sizeof(XftFont *));
  for (int i = 0; i < drw.nfonts; ++i)
    drw.fonts[i] = XftFontOpenName(dpy, scr, config->fonts[i]);

  drw.colorcache = NULL;
}

static inline void createbar(const BarConfig *barConfig) {
  Geometry geometry = barConfig->geometry;
  ATOM_WM_NAME = XInternAtom(dpy, "WM_NAME", False);
  bar.window_g = (Geometry){geometry.x, geometry.y, geometry.w, geometry.h};

  bar.canvas_g.x = barConfig->padding.left;
  bar.canvas_g.y = barConfig->padding.top;
  bar.canvas_g.w =
      bar.window_g.w - barConfig->padding.left - barConfig->padding.right;
  bar.canvas_g.h =
      bar.window_g.h - barConfig->padding.top - barConfig->padding.bottom;

  bar.window =
      XCreateSimpleWindow(dpy, root, bar.window_g.x, bar.window_g.y,
                          bar.window_g.w, bar.window_g.h, 0, 0xffffff, 0);

  XftColorAllocName(dpy, drw.vis, drw.cmap, barConfig->foreground,
                    &bar.foreground);
  XftColorAllocName(dpy, drw.vis, drw.cmap, barConfig->background,
                    &bar.background);

  bar.canvas = XftDrawCreate(dpy, bar.window, drw.vis, drw.cmap);
}

static inline void prepare_stdinblks(const Block blks[MAX_BLKS], int nblks) {
  XGlyphInfo extent;
  int fnindex, startx = 0;
  for (int i = 0; i < nblks; ++i) {
    const Block *blk = &blks[i];
    fnindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
    XftTextExtentsUtf8(dpy, drw.fonts[fnindex], (FcChar8 *)blk->text,
                       blk->ntext, &extent);

    drw.gis[Stdin][i].width = extent.xOff;
    drw.gis[Stdin][i].x = startx + extent.x;
    startx += drw.gis[Stdin][i].width;
  }
}

static inline void prepare_customblks(const Block blks[MAX_BLKS], int nblks) {
  XGlyphInfo extent;
  int fnindex, startx = bar.canvas_g.x + bar.canvas_g.w;
  for (int i = nblks - 1; i >= 0; --i) {
    const Block *blk = &blks[i];
    fnindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
    XftTextExtentsUtf8(dpy, drw.fonts[fnindex], (FcChar8 *)blk->text,
                       blk->ntext, &extent);

    drw.gis[Custom][i].width = extent.xOff;
    startx -= extent.x + extent.xOff;
    drw.gis[Custom][i].x = startx;
  }
}

static void xrenderblks(BlockType blktype, const Block blks[MAX_BLKS],
                        int nblk) {
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
      if (!(size = parse_box_string(box->val, color)))
        continue;
      const TagModifier *mods = ValidTagModifiers[Box];
      for (int e = 0; mods[e] != NullModifier; ++e) {
        int bx = canvas_g->x, by = canvas_g->y, bw = 0, bh = 0;
        if (box->tmod_mask & (1 << mods[e])) {
          switch (mods[e]) {
          case Left:
            bx = gi->x, bw = size, bh = canvas_g->h;
            break;
          case Right:
            bx = gi->x + gi->width - size, bw = size, bh = canvas_g->h;
            break;
          case Top:
            bx = gi->x, bw = gi->width, bh = size;
            break;
          case Bottom: // default.
          default:
            bx = gi->x, by = canvas_g->y + canvas_g->h - size, bw = gi->width,
            bh = size;
          }
          XftColor *rectcolor = get_cached_color(color);
          XftDrawRect(bar.canvas, rectcolor, bx, by, bw, bh);
        }
      }
      box = box->previous;
    }

    fnindex = blk->tags[Fn] != NULL ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
    starty = canvas_g->y + (canvas_g->h - drw.fonts[fnindex]->height) / 2 +
             drw.fonts[fnindex]->ascent;
    fg = blk->tags[Fg] != NULL ? get_cached_color(blk->tags[Fg]->val)
                               : &bar.foreground;
    XftDrawStringUtf8(bar.canvas, fg, drw.fonts[fnindex], gi->x, starty,
                      (FcChar8 *)blk->text, blk->ntext);
  }
}

void xsetup(const Config *config) {
  for (int i = 0; i < MAX_BLKS; ++i)
    drw.gis[Stdin][i] = drw.gis[Custom][i] = (GlyphInfo){0, 0};

  if ((dpy = XOpenDisplay(NULL)) == NULL)
    die("Cannot open display.\n");
  root = DefaultRootWindow(dpy);

  // initialize bar.
  createdrw(config);
  createbar(&config->barConfig);

  // set bar window properties.
  XSetWindowAttributes attrs = {.event_mask = StructureNotifyMask |
                                              ExposureMask | ButtonPressMask,
                                .override_redirect = True};
  XChangeWindowAttributes(dpy, bar.window, CWEventMask | CWOverrideRedirect,
                          &attrs);

  Atom NetWMDock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", 0);
  XChangeProperty(dpy, bar.window, XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", 0),
                  XA_ATOM, 32, PropModeReplace, (uint8_t *)&NetWMDock,
                  sizeof(Atom));

  long barheight = bar.window_g.h + config->barConfig.margin.top +
                   config->barConfig.margin.bottom;
  long strut[4] = {0, 0, config->barConfig.topbar ? barheight : 0,
                   !config->barConfig.topbar ? barheight : 0};
  XChangeProperty(dpy, bar.window, XInternAtom(dpy, "_NET_WM_STRUT", 0),
                  XA_CARDINAL, 32, PropModeReplace, (uint8_t *)strut, 4l);

  XStoreName(dpy, bar.window, NAME);
  XClassHint *classhint = XAllocClassHint();
  classhint->res_name = NAME;
  classhint->res_class = NAME;
  XSetClassHint(dpy, bar.window, classhint);
  XFree(classhint);
  XMapWindow(dpy, bar.window);
}

void clear(BlockType blktype, int nblks) {
  if (nblks) {
    GlyphInfo *first = &drw.gis[blktype][0],
              *last = &drw.gis[blktype][nblks - 1];
    ClearBar(bar, first->x, 0, last->x + last->width, bar.window_g.h);
  }
}

void render(BlockType blktype, const Block blks[MAX_BLKS], int nblks) {
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

static void onButtonPress(const XEvent *xevent, Block blks[2][MAX_BLKS],
                          int nblks[2]) {
  const XButtonEvent *e = &xevent->xbutton;
  for (int i = 0; i < nblks[Stdin] + nblks[Custom]; ++i) {
    Block *blk =
        i < nblks[Stdin] ? &blks[Stdin][i] : &blks[Custom][i - nblks[Stdin]];
    GlyphInfo *gi = i < nblks[Stdin] ? &drw.gis[Stdin][i]
                                     : &drw.gis[Custom][i - nblks[Stdin]];

    if (e->x >= gi->x && e->x <= gi->x + gi->width) {
      TagKey tkey = e->button == Button1   ? BtnL
                    : e->button == Button2 ? BtnM
                    : e->button == Button3 ? BtnR
                    : e->button == Button4 ? ScrlU
                    : e->button == Button5 ? ScrlD
                                           : NullKey;

      TagModifierMask tmod_mask = 0x0;
      if (e->state & ShiftMask)
        tmod_mask |= (1 << Shift);
      if (e->state & ControlMask)
        tmod_mask |= (1 << Ctrl);
      if (e->state & Mod1Mask)
        tmod_mask |= (1 << Super);
      if (e->state & Mod4Mask)
        tmod_mask |= (1 << Alt);

      if (tkey != NullKey) {
        Tag *action_tag = blk->tags[tkey];
        while (action_tag != NULL) {
          if (strlen(action_tag->val) && action_tag->tmod_mask == tmod_mask)
            system(action_tag->val);
          action_tag = action_tag->previous;
        }
      }
      break;
    }
  }
}

static bool onPropertyNotify(const XEvent *xevent, char *name) {
  const XPropertyEvent *e = &xevent->xproperty;
  char *wm_name;
  if (e->window == root && e->atom == ATOM_WM_NAME) {
    if (XFetchName(dpy, root, &wm_name) && wm_name) {
      strcpy(name, wm_name);
      XFree(wm_name);
      return true;
    }
  }
  return false;
}

BarEvent handle_xevent(Block blks[2][MAX_BLKS], int nblks[2],
                       char name[BLOCK_BUF_SIZE]) {
  XEvent e;
  if (XPending(dpy)) {
    XNextEvent(dpy, &e);

    switch (e.type) {
    case MapNotify:
      // start watching for property change event on root window, only after the
      // bar window maps, as program might segfault, if tried to render on
      // window that is not visible.
      XSelectInput(dpy, root, PropertyChangeMask);
      XStoreName(dpy, root, NAME "-" VERSION);
      return ReadyEvent;
    case ButtonPress:
      onButtonPress(&e, blks, nblks);
      break;
    // Reseting the bar on every 'Expose'.
    // Xft drawable gets cleared, when another window comes on top of it (that
    // is only if no compositor is running, something like 'picom').
    case Expose:
      ClearBar(bar, 0, 0, bar.window_g.w, bar.window_g.h);
      render(Stdin, blks[Stdin], nblks[Stdin]);
      render(Custom, blks[Custom], nblks[Custom]);
      break;

    // root window events.
    case PropertyNotify:
      if (onPropertyNotify(&e, name))
        return DrawEvent;
      break;
    }
  }
  return NoActionEvent;
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
