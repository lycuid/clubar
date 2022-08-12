#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdint.h>
#include <xdbar.h>
#include <xdbar/core.h>
#include <xdbar/core/blocks.h>

typedef struct {
  int x, width;
} GlyphInfo;

typedef struct ColorCache {
  char name[32];
  XftColor val;
  struct ColorCache *previous;
} ColorCache;

static struct Draw {
  int nfonts;
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

static inline XftColor *get_cached_color(const char *);
static inline int parse_box_string(const char *, char[32]);
static inline void drw_init(const Config *);
static inline void bar_init(const BarConfig *);
static inline void prepare_stdinblks(void);
static inline void prepare_customblks(void);
static inline void xrender_bg(const Block *, const GlyphInfo *);
static inline void xrender_box(const Block *, const GlyphInfo *);
static inline void xrender_string(const Block *, const GlyphInfo *);
static void onButtonPress(const XEvent *);
static bool onPropertyNotify(const XEvent *, char *);

static Display *dpy;
static Atom ATOM_WM_NAME;

#define FILL(...) XftDrawRect(bar.canvas, &bar.background, __VA_ARGS__)
#define root      (DefaultRootWindow(dpy))
#define scr       (DefaultScreen(dpy))
#define vis       (DefaultVisual(dpy, scr))
#define cmap      (DefaultColormap(dpy, scr))

static inline XftColor *get_cached_color(const char *colorname)
{
  for (ColorCache *c = drw.colorcache; c != NULL; c = c->previous)
    if (strcmp(c->name, colorname) == 0)
      return &c->val;
  // @TODO: currently not handling error when allocating colors.
  ColorCache *color = (ColorCache *)malloc(sizeof(ColorCache));
  XftColorAllocName(dpy, vis, cmap, colorname, &color->val);
  strcpy(color->name, colorname);
  color->previous = drw.colorcache;
  drw.colorcache  = color;
  return &drw.colorcache->val;
}

static inline int parse_box_string(const char *val, char color[32])
{
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

static inline void drw_init(const Config *config)
{
  drw.nfonts = config->nfonts;
  drw.fonts  = (XftFont **)malloc(drw.nfonts * sizeof(XftFont *));
  for (int i = 0; i < drw.nfonts; ++i)
    drw.fonts[i] = XftFontOpenName(dpy, scr, config->fonts[i]);
  drw.colorcache = NULL;
}

static inline void bar_init(const BarConfig *barConfig)
{
  Geometry geometry = barConfig->geometry;
  ATOM_WM_NAME      = XInternAtom(dpy, "WM_NAME", False);
  bar.window_g   = (Geometry){geometry.x, geometry.y, geometry.w, geometry.h};
  bar.canvas_g.x = barConfig->padding.left;
  bar.canvas_g.y = barConfig->padding.top;
  bar.canvas_g.w =
      bar.window_g.w - barConfig->padding.left - barConfig->padding.right;
  bar.canvas_g.h =
      bar.window_g.h - barConfig->padding.top - barConfig->padding.bottom;
  bar.window =
      XCreateSimpleWindow(dpy, root, bar.window_g.x, bar.window_g.y,
                          bar.window_g.w, bar.window_g.h, 0, 0xffffff, 0);
  XftColorAllocName(dpy, vis, cmap, barConfig->foreground, &bar.foreground);
  XftColorAllocName(dpy, vis, cmap, barConfig->background, &bar.background);
  bar.canvas = XftDrawCreate(dpy, bar.window, vis, cmap);
}

static inline void prepare_stdinblks(void)
{
  XGlyphInfo extent;
  int fnindex, startx = 0;
  for (int i = 0; i < core->nblks[Stdin]; ++i) {
    const Block *blk = &core->blks[Stdin][i];
    fnindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
    XftTextExtentsUtf8(dpy, drw.fonts[fnindex], (FcChar8 *)blk->text,
                       blk->ntext, &extent);
    drw.gis[Stdin][i].width = extent.xOff;
    drw.gis[Stdin][i].x     = startx + extent.x;
    startx += drw.gis[Stdin][i].width;
  }
}

static inline void prepare_customblks(void)
{
  XGlyphInfo extent;
  int fnindex, startx = bar.canvas_g.x + bar.canvas_g.w;
  for (int i = core->nblks[Custom] - 1; i >= 0; --i) {
    const Block *blk = &core->blks[Custom][i];
    fnindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
    XftTextExtentsUtf8(dpy, drw.fonts[fnindex], (FcChar8 *)blk->text,
                       blk->ntext, &extent);
    drw.gis[Custom][i].width = extent.xOff;
    startx -= extent.x + extent.xOff;
    drw.gis[Custom][i].x = startx;
  }
}

static inline void xrender_bg(const Block *blk, const GlyphInfo *gi)
{
  const Geometry *canvas_g = &bar.canvas_g;
  if (blk->tags[Bg] != NULL)
    XftDrawRect(bar.canvas, get_cached_color(blk->tags[Bg]->val), gi->x,
                canvas_g->y, gi->width, canvas_g->h);
}

static inline void xrender_box(const Block *blk, const GlyphInfo *gi)
{
  char color[32];
  const Geometry *canvas_g = &bar.canvas_g;
  for (Tag *box = blk->tags[Box]; box != NULL; box = box->previous) {
    int size = parse_box_string(box->val, color);
    if (!size)
      continue;
    const TagModifier *mods = ValidTagModifiers[Box];
    for (int e = 0; mods[e] != NullTagModifier; ++e) {
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
  }
}

static void xrender_string(const Block *blk, const GlyphInfo *gi)
{
  Geometry *canvas_g = &bar.canvas_g;
  int fnindex =
      blk->tags[Fn] != NULL ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
  int starty = canvas_g->y + (canvas_g->h - drw.fonts[fnindex]->height) / 2 +
               drw.fonts[fnindex]->ascent;
  XftColor *fg = blk->tags[Fg] != NULL ? get_cached_color(blk->tags[Fg]->val)
                                       : &bar.foreground;
  XftDrawStringUtf8(bar.canvas, fg, drw.fonts[fnindex], gi->x, starty,
                    (FcChar8 *)blk->text, blk->ntext);
}

static void onButtonPress(const XEvent *xevent)
{
  const XButtonEvent *e     = &xevent->xbutton;
  TagName tag_name          = NullTagName;
  TagModifierMask tmod_mask = 0x0;
  for (int i = 0; i < core->nblks[Stdin] + core->nblks[Custom]; ++i) {
    const Block *blk    = i < core->nblks[Stdin]
                              ? &core->blks[Stdin][i]
                              : &core->blks[Custom][i - core->nblks[Stdin]];
    const GlyphInfo *gi = i < core->nblks[Stdin]
                              ? &drw.gis[Stdin][i]
                              : &drw.gis[Custom][i - core->nblks[Stdin]];
    // check if click event coordinates match with any coordinate on the bar
    // window.
    if (e->x >= gi->x && e->x <= gi->x + gi->width) {
      tag_name = e->button == Button1   ? BtnL
                 : e->button == Button2 ? BtnM
                 : e->button == Button3 ? BtnR
                 : e->button == Button4 ? ScrlU
                 : e->button == Button5 ? ScrlD
                                        : NullTagName;

      tmod_mask = 0x0;
      if (e->state & ShiftMask)
        tmod_mask |= (1 << Shift);
      if (e->state & ControlMask)
        tmod_mask |= (1 << Ctrl);
      if (e->state & Mod1Mask)
        tmod_mask |= (1 << Super);
      if (e->state & Mod4Mask)
        tmod_mask |= (1 << Alt);

      if (tag_name != NullTagName)
        for (Tag *tag = blk->tags[tag_name]; tag; tag = tag->previous)
          if (strlen(tag->val) && tag->tmod_mask == tmod_mask)
            system(tag->val);
      break;
    }
  }
}

static bool onPropertyNotify(const XEvent *xevent, char *name)
{
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

void xdb_setup(const Config *config)
{
  for (int i = 0; i < MAX_BLKS; ++i)
    drw.gis[Stdin][i] = drw.gis[Custom][i] = (GlyphInfo){0, 0};
  if ((dpy = XOpenDisplay(NULL)) == NULL)
    die("Cannot open display.\n");
  // initialize drw.
  drw_init(config);
  // initialize bar.
  bar_init(&config->barConfig);
  // set window properties.
  XSetWindowAttributes attrs = {.event_mask = StructureNotifyMask |
                                              ExposureMask | ButtonPressMask,
                                .override_redirect = True};
  XChangeWindowAttributes(dpy, bar.window, CWEventMask | CWOverrideRedirect,
                          &attrs);
  // ewmh: set window type
  Atom NetWMDock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", 0);
  XChangeProperty(dpy, bar.window, XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", 0),
                  XA_ATOM, 32, PropModeReplace, (uint8_t *)&NetWMDock,
                  sizeof(Atom));
  // ewmh: set window struct values.
  long barheight = bar.window_g.h + config->barConfig.margin.top +
                   config->barConfig.margin.bottom;
  long strut[4] = {0, 0, config->barConfig.topbar ? barheight : 0,
                   !config->barConfig.topbar ? barheight : 0};
  XChangeProperty(dpy, bar.window, XInternAtom(dpy, "_NET_WM_STRUT", 0),
                  XA_CARDINAL, 32, PropModeReplace, (uint8_t *)strut, 4l);
  // set window resource title, class and instance name.
  XStoreName(dpy, bar.window, NAME);
  XSetClassHint(dpy, bar.window,
                &(XClassHint){.res_name = NAME, .res_class = NAME});
  XMapWindow(dpy, bar.window);
}

void xdb_clear(BlockType blktype)
{
  if (core->nblks[blktype]) {
    GlyphInfo *first = &drw.gis[blktype][0],
              *last  = &drw.gis[blktype][core->nblks[blktype] - 1];
    FILL(first->x, 0, last->x + last->width, bar.window_g.h);
  }
}

void xdb_render(BlockType blktype)
{
  switch (blktype) {
  case Stdin:
    prepare_stdinblks();
    break;
  case Custom:
    prepare_customblks();
    break;
  }
  for (int i = 0; i < core->nblks[blktype]; ++i) {
    const Block *blk    = &core->blks[blktype][i];
    const GlyphInfo *gi = &drw.gis[blktype][i];
    xrender_bg(blk, gi);
    xrender_box(blk, gi);
    xrender_string(blk, gi);
  }
}

BarEvent xdb_nextevent(char name[BLOCK_BUF_SIZE])
{
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
      onButtonPress(&e);
      break;
    // Reseting the bar on every 'Expose'.
    // Xft drawable gets cleared, when another window comes on top of it (that
    // is only if no compositor is running, something like 'picom').
    case Expose:
      FILL(0, 0, bar.window_g.w, bar.window_g.h);
      xdb_render(Stdin);
      xdb_render(Custom);
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

void xdb_cleanup(void)
{
  while (drw.colorcache != NULL) {
    XftColorFree(dpy, vis, cmap, &drw.colorcache->val);
    ColorCache *stale = drw.colorcache;
    drw.colorcache    = drw.colorcache->previous;
    free(stale);
  }
  for (int fn = 0; fn < drw.nfonts; ++fn)
    XftFontClose(dpy, drw.fonts[fn]);
  XftColorFree(dpy, vis, cmap, &bar.foreground);
  XftColorFree(dpy, vis, cmap, &bar.background);
  XftDrawDestroy(bar.canvas);
  XCloseDisplay(dpy);
}
