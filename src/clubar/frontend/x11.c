#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <clubar/core.h>
#include <clubar/core/blocks.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

typedef struct GlyphInfo {
  int x, width;
} GlyphInfo;

typedef struct ColorCache {
  char name[32];
  XftColor val;
  struct ColorCache *prev, *next;
} ColorCache;

#define CC_ATTACH(c)                                                           \
  do { /* Attach item on top of the 'drw.colorcache' linked list.*/            \
    if ((c->prev = NULL, c->next = drw.colorcache))                            \
      drw.colorcache->prev = c;                                                \
    drw.colorcache = c;                                                        \
  } while (0)
#define CC_DETACH(c)                                                           \
  do { /* Detach item from the 'drw.colorcache' linked list.*/                 \
    (void)(c->prev ? (c->prev->next = c->next) : (drw.colorcache = c->next));  \
    (void)(c->next ? (c->next->prev = c->prev) : 0);                           \
  } while (0)

#define CC_FREE(cc)                                                            \
  do {                                                                         \
    __typeof__(cc) tmp = (cc);                                                 \
    CC_DETACH(tmp);                                                            \
    XftColorFree(dpy, vis, cmap, &(tmp)->val);                                 \
    free(tmp);                                                                 \
  } while (0)

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

static Display *dpy;
static Atom ATOM_WM_NAME;

#define root              (DefaultRootWindow(dpy))
#define scr               (DefaultScreen(dpy))
#define vis               (DefaultVisual(dpy, scr))
#define cmap              (DefaultColormap(dpy, scr))
#define fill_rect(...)    XftDrawRect(bar.canvas, &bar.background, __VA_ARGS__)
#define alloc_color(p, c) XftColorAllocName(dpy, vis, cmap, c, p)

static XftColor *request_color(const char *colorname)
{
  static int capacity = 1 << 5;
  ColorCache *last    = NULL;
  for (ColorCache *c = drw.colorcache; c; last = c, c = c->next) {
    if (strcmp(c->name, colorname) == 0) {
      CC_DETACH(c);
      CC_ATTACH(c);
      return &c->val;
    }
  }
  XftColor xft_color;
  if (!alloc_color(&xft_color, colorname))
    return &bar.foreground;

  if (capacity)
    capacity--;
  else if (last)
    CC_FREE(last);
  ColorCache *color = (ColorCache *)malloc(sizeof(ColorCache));
  memmove(&color->val, &xft_color, sizeof(xft_color));
  strcpy(color->name, colorname);
  CC_ATTACH(color);
  return &drw.colorcache->val;
}

static inline int parse_box_string(const char *val, char color[32])
{
  int cursor = 0, c = 0, nval = strlen(val), size = 0;
  memset(color, 0, 32);
  while (cursor < nval && val[cursor] != ':')
    color[c++] = val[cursor++];
  if (val[cursor++] == ':')
    while (cursor < nval && val[cursor] >= '0' && val[cursor] <= '9')
      size = size * 10 + val[cursor++] - '0';
  if (cursor < nval - 1 && (size = -1) == -1)
    eprintf("Invalid Box template string: '%s'\n", val);
  return size + (size <= 0);
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
  if (!alloc_color(&bar.foreground, barConfig->foreground))
    alloc_color(&bar.foreground, "#ffffff");
  if (!alloc_color(&bar.background, barConfig->background))
    alloc_color(&bar.background, "#000000");
  bar.canvas = XftDrawCreate(dpy, bar.window, vis, cmap);
}

static inline void generate_stdin_gis(void)
{
  XGlyphInfo extent;
  int fntindex, startx = 0;
  for (int i = 0; i < core->nblks[Stdin]; ++i) {
    const Block *blk = &core->blks[Stdin][i];
    fntindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
    XftTextExtentsUtf8(dpy, drw.fonts[fntindex], (FcChar8 *)blk->text,
                       strlen(blk->text), &extent);
    drw.gis[Stdin][i].width = extent.xOff;
    drw.gis[Stdin][i].x     = startx + extent.x;
    startx += drw.gis[Stdin][i].width;
  }
}

static inline void generate_custom_gis(void)
{
  XGlyphInfo extent;
  int fntindex, startx = bar.canvas_g.x + bar.canvas_g.w;
  for (int i = core->nblks[Custom] - 1; i >= 0; --i) {
    const Block *blk = &core->blks[Custom][i];
    fntindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
    XftTextExtentsUtf8(dpy, drw.fonts[fntindex], (FcChar8 *)blk->text,
                       strlen(blk->text), &extent);
    drw.gis[Custom][i].width = extent.xOff;
    startx -= extent.x + extent.xOff;
    drw.gis[Custom][i].x = startx;
  }
}

static inline void xrender_bg(const Block *blk, const GlyphInfo *gi)
{
  const Geometry *canvas_g = &bar.canvas_g;
  if (blk->tags[Bg] != NULL)
    XftDrawRect(bar.canvas, request_color(blk->tags[Bg]->val), gi->x,
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
        case Left: {
          bx = gi->x, bw = size, bh = canvas_g->h;
        } break;
        case Right: {
          bx = gi->x + gi->width - size, bw = size, bh = canvas_g->h;
        } break;
        case Top: {
          bx = gi->x, bw = gi->width, bh = size;
        } break;
        case Bottom: {
          bx = gi->x, by = canvas_g->y + canvas_g->h - size, bw = gi->width,
          bh = size;
        } break;
        default:
          break;
        }
        XftDrawRect(bar.canvas, request_color(color), bx, by, bw, bh);
      }
    }
  }
}

static inline void xrender_string(const Block *blk, const GlyphInfo *gi)
{
  Geometry *canvas_g = &bar.canvas_g;
  int fntindex =
      blk->tags[Fn] != NULL ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
  int starty = canvas_g->y + (canvas_g->h - drw.fonts[fntindex]->height) / 2 +
               drw.fonts[fntindex]->ascent;
  XftColor *fg = blk->tags[Fg] != NULL ? request_color(blk->tags[Fg]->val)
                                       : &bar.foreground;
  XftDrawStringUtf8(bar.canvas, fg, drw.fonts[fntindex], gi->x, starty,
                    (FcChar8 *)blk->text, strlen(blk->text));
}

static void execute_cmd(char *const cmd)
{
  if (fork())
    return;
  if (dpy)
    close(ConnectionNumber(dpy));
  setsid();

  char *words[1 << 6];
  uint32_t cursor = 0;
  for (uint32_t i = 0, len = strlen(cmd); i < len;) {
    for (; i < len && cmd[i] == ' '; ++i)
      cmd[i] = 0;
    if (i < len)
      words[cursor++] = cmd + i;
    while (i < len && cmd[i] != ' ')
      ++i;
  }
  words[cursor] = NULL;

  execvp((char *)words[0], (char **)words);
  exit(EXIT_SUCCESS);
}

static inline bool get_window_name(char *buffer)
{
  char *wm_name;
  if (XFetchName(dpy, root, &wm_name) && wm_name) {
    strcpy(buffer, wm_name);
    XFree(wm_name);
    return true;
  }
  return false;
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
            execute_cmd(tag->val);
      break;
    }
  }
}

static bool onPropertyNotify(const XEvent *xevent, char *name)
{
  const XPropertyEvent *e = &xevent->xproperty;
  if (e->window == root && e->atom == ATOM_WM_NAME)
    return get_window_name(name);
  return false;
}

void clu_setup(void)
{
  for (int i = 0; i < MAX_BLKS; ++i)
    drw.gis[Stdin][i] = drw.gis[Custom][i] = (GlyphInfo){0, 0};
  if ((dpy = XOpenDisplay(NULL)) == NULL)
    die("Cannot open display.\n");
  drw_init(&core->config);
  bar_init(&core->config.barConfig);

  XSetWindowAttributes attrs = {
      .event_mask        = StructureNotifyMask | ExposureMask | ButtonPressMask,
      .override_redirect = True,
  };
  XChangeWindowAttributes(dpy, bar.window, CWEventMask | CWOverrideRedirect,
                          &attrs);

  Atom NetWMDock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", 0);
  XChangeProperty(dpy, bar.window, XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", 0),
                  XA_ATOM, 32, PropModeReplace, (uint8_t *)&NetWMDock,
                  sizeof(Atom));

  long barheight = bar.window_g.h + core->config.barConfig.margin.top +
                   core->config.barConfig.margin.bottom;
  long strut[4] = {0, 0, core->config.barConfig.topbar ? barheight : 0,
                   !core->config.barConfig.topbar ? barheight : 0};
  XChangeProperty(dpy, bar.window, XInternAtom(dpy, "_NET_WM_STRUT", 0),
                  XA_CARDINAL, 32, PropModeReplace, (uint8_t *)strut, 4l);

  XStoreName(dpy, bar.window, NAME);
  XSetClassHint(dpy, bar.window,
                &(XClassHint){.res_name = NAME, .res_class = NAME});
  XMapWindow(dpy, bar.window);
}

void clu_clear(BlockType blktype)
{
  if (core->nblks[blktype]) {
    GlyphInfo *first = &drw.gis[blktype][0],
              *last  = &drw.gis[blktype][core->nblks[blktype] - 1];
    fill_rect(first->x, 0, last->x + last->width, bar.window_g.h);
  }
}

void clu_render(BlockType blktype)
{
  switch (blktype) {
  case Stdin:
    generate_stdin_gis();
    break;
  case Custom:
    generate_custom_gis();
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

void clu_toggle(void)
{
  XWindowAttributes attrs;
  XGetWindowAttributes(dpy, bar.window, &attrs);
  attrs.map_state == IsUnmapped ? XMapWindow(dpy, bar.window)
                                : XUnmapWindow(dpy, bar.window);
}

CluEvent clu_nextevent(char value[BLK_BUFFER_SIZE])
{
  CluEvent clu_event = CLU_Noop;
  XEvent e;
  if (XPending(dpy)) {
    switch (XNextEvent(dpy, &e), e.type) {
    case MapNotify: {
      XSelectInput(dpy, root, PropertyChangeMask);
      get_window_name(value);
      return CLU_Ready;
    } break;
    case ButtonPress: {
      onButtonPress(&e);
    } break;
    case Expose: {
      fill_rect(0, 0, bar.window_g.w, bar.window_g.h);
      clu_event = CLU_Reset;
    } break;
    // root window events.
    case PropertyNotify: {
      if (onPropertyNotify(&e, value))
        clu_event = CLU_NewValue;
    } break;
    }
  }
  return clu_event;
}

void clu_cleanup(void)
{

  while (drw.colorcache)
    CC_FREE(drw.colorcache);
  for (int fnt = 0; fnt < drw.nfonts; ++fnt)
    XftFontClose(dpy, drw.fonts[fnt]);
  XftColorFree(dpy, vis, cmap, &bar.foreground);
  XftColorFree(dpy, vis, cmap, &bar.background);
  XftDrawDestroy(bar.canvas);
  XCloseDisplay(dpy);
}
