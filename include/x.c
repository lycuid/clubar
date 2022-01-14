#include "x.h"
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
  int x, width;
} RenderInfo;

typedef struct _ColorCache {
  char name[32];
  XftColor val;
  struct _ColorCache *previous;
} ColorCache;

typedef struct {
  int nfonts;
  Visual *vis;
  Colormap cmap;
  XftFont **fonts;
  ColorCache *colorcache;
} Context;

typedef struct {
  Window xwindow;
  XftDraw *canvas;
  Geometry window_g, canvas_g;
  XftColor foreground, background;
} Bar;

static XEvent e;
static XGlyphInfo extent;
static Context ctx;
static Bar bar;
static Display *dpy;
static Window root;
static int scr;
static char *wm_name;
static RenderInfo Infos[2][MAX_BLKS];

XftColor *get_cached_color(const char *);
int parseboxstring(const char *, char[32]);
void createctx();
void createbar();
void clearbar();
void xsetup();
void xsetatoms();
void xcleanup();
void xrenderblks(Block[MAX_BLKS], int, RenderInfo[MAX_BLKS]);
void preparestdinblks(Block[MAX_BLKS], int);
void preparecustomblks(Block[MAX_BLKS], int);
void handle_xbuttonpress(XButtonEvent *, Block[2][MAX_BLKS], int[2]);

XftColor *get_cached_color(const char *colorname) {
  ColorCache *cache = ctx.colorcache;
  while (cache != NULL) {
    if (strcmp(cache->name, colorname) == 0)
      return &cache->val;
    cache = cache->previous;
  }

  // @TODO: currently not handling error when allocating colors.
  ColorCache *color = (ColorCache *)malloc(sizeof(ColorCache));
  XftColorAllocName(dpy, ctx.vis, ctx.cmap, colorname, &color->val);

  strcpy(color->name, colorname);
  color->previous = ctx.colorcache;
  ctx.colorcache = color;

  return &ctx.colorcache->val;
}

int parseboxstring(const char *val, char color[32]) {
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

void createctx(const Config *const config) {
  ctx.vis = DefaultVisual(dpy, scr);
  ctx.cmap = DefaultColormap(dpy, scr);

  ctx.nfonts = config->nfonts;

  ctx.fonts = (XftFont **)malloc(ctx.nfonts * sizeof(XftFont *));
  for (int i = 0; i < ctx.nfonts; ++i)
    ctx.fonts[i] = XftFontOpenName(dpy, scr, config->fonts[i]);

  ctx.colorcache = NULL;
}

void createbar(const BarConfig *const barConfig) {
  Geometry geometry = barConfig->geometry;
  bar.window_g = (Geometry){geometry.x, geometry.y, geometry.w, geometry.h};

  bar.canvas_g.x = barConfig->padding.left;
  bar.canvas_g.y = barConfig->padding.top;
  bar.canvas_g.w =
      bar.window_g.w - barConfig->padding.left - barConfig->padding.right;
  bar.canvas_g.h =
      bar.window_g.h - barConfig->padding.top - barConfig->padding.bottom;

  bar.xwindow = XCreateSimpleWindow(dpy, root, bar.window_g.x, bar.window_g.y,
                                    bar.window_g.w, bar.window_g.h, 0,
                                    WhitePixel(dpy, scr), BlackPixel(dpy, scr));

  XftColorAllocName(dpy, ctx.vis, ctx.cmap, barConfig->foreground,
                    &bar.foreground);
  XftColorAllocName(dpy, ctx.vis, ctx.cmap, barConfig->background,
                    &bar.background);

  bar.canvas = XftDrawCreate(dpy, bar.xwindow, ctx.vis, ctx.cmap);
}

void clearbar() {
  XftDrawRect(bar.canvas, &bar.background, 0, 0, bar.window_g.w,
              bar.window_g.h);
}

void xsetup(const Config *const config) {
  XInitThreads();
  if ((dpy = XOpenDisplay(NULL)) == NULL)
    die("Cannot open display.\n");
  root = DefaultRootWindow(dpy);
  scr = DefaultScreen(dpy);

  createctx(config);
  createbar(&config->barConfig);

  XSelectInput(dpy, root, PropertyChangeMask);
  XSelectInput(dpy, bar.xwindow, ExposureMask | ButtonPressMask);
  xsetatoms(&config->barConfig);
  XMapWindow(dpy, bar.xwindow);
}

void xsetatoms(const BarConfig *const barConfig) {
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

void xcleanup() {
  while (ctx.colorcache != NULL) {
    XftColorFree(dpy, ctx.vis, ctx.cmap, &ctx.colorcache->val);
    ColorCache *stale = ctx.colorcache;
    ctx.colorcache = ctx.colorcache->previous;
    free(stale);
  }

  for (int fn = 0; fn < ctx.nfonts; ++fn)
    XftFontClose(dpy, ctx.fonts[fn]);

  XftColorFree(dpy, ctx.vis, ctx.cmap, &bar.foreground);
  XftColorFree(dpy, ctx.vis, ctx.cmap, &bar.background);
  XftDrawDestroy(bar.canvas);
  XCloseDisplay(dpy);
}

void xrenderblks(Block blks[MAX_BLKS], int nblk, RenderInfo ris[MAX_BLKS]) {
  int starty, size, fnindex;
  char color[32];
  XftColor *fg;
  Attribute *box;
  Geometry *canvas_g = &bar.canvas_g;

  for (int i = 0; i < nblk; ++i) {
    Block *blk = &blks[i];
    RenderInfo *ri = &ris[i];

    if (blk->attrs[Bg] != NULL)
      XftDrawRect(bar.canvas, get_cached_color(blk->attrs[Bg]->val), ri->x,
                  canvas_g->y, ri->width, canvas_g->h);

    box = blk->attrs[Box];
    while (box != NULL) {
      size = parseboxstring(box->val, color);
      if (size) {
        int bx = canvas_g->x, by = canvas_g->y, bw = 0, bh = 0;
        switch (box->extension) {
        case Top:
          bx = ri->x, bw = ri->width, bh = size;
          break;
        case Bottom:
          bx = ri->x, by = canvas_g->y + canvas_g->h - size, bw = ri->width,
          bh = size;
          break;
        case Left:
          bx = ri->x, bw = size, bh = canvas_g->h;
          break;
        case Right:
          bx = ri->x + ri->width - size, bw = size, bh = canvas_g->h;
          break;
        default:
          break;
        }
        XftColor *rectcolor = get_cached_color(color);
        XftDrawRect(bar.canvas, rectcolor, bx, by, bw, bh);
      }
      box = box->previous;
    }

    fnindex = blk->attrs[Fn] != NULL ? atoi(blk->attrs[Fn]->val) : 0;
    starty = canvas_g->y + (canvas_g->h - ctx.fonts[fnindex]->height) / 2 +
             ctx.fonts[fnindex]->ascent;
    fg = blk->attrs[Fg] != NULL ? get_cached_color(blk->attrs[Fg]->val)
                                : &bar.foreground;
    XftDrawStringUtf8(bar.canvas, fg, ctx.fonts[fnindex], ri->x, starty,
                      (FcChar8 *)blk->text, strlen(blk->text));
  }
}

void preparestdinblks(Block Blks[MAX_BLKS], int NBlks) {
  int renderx = 0;
  for (int i = 0; i < NBlks; ++i) {
    Block *blk = &Blks[i];
    int fnindex = blk->attrs[Fn] ? atoi(blk->attrs[Fn]->val) : 0;
    XftTextExtentsUtf8(dpy, ctx.fonts[fnindex], (FcChar8 *)blk->text,
                       strlen(blk->text), &extent);

    Infos[Stdin][i].width = extent.xOff;
    Infos[Stdin][i].x = renderx + extent.x;
    renderx += Infos[Stdin][i].width;
  }
}

void preparecustomblks(Block Blks[MAX_BLKS], int NBlks) {
  int renderx = bar.canvas_g.x + bar.canvas_g.w;
  for (int i = NBlks - 1; i >= 0; --i) {
    Block *blk = &Blks[i];
    int fnindex = blk->attrs[Fn] ? atoi(blk->attrs[Fn]->val) : 0;
    XftTextExtentsUtf8(dpy, ctx.fonts[fnindex], (FcChar8 *)blk->text,
                       strlen(blk->text), &extent);

    Infos[Custom][i].width = extent.xOff;
    renderx -= extent.x + extent.xOff;
    Infos[Custom][i].x = renderx;
  }
}

void renderblks(BlockType blktype, Block Blks[2][MAX_BLKS], int NBlks[2]) {
  XLockDisplay(dpy);
  switch (blktype) {
  case Stdin:
    preparestdinblks(Blks[Stdin], NBlks[Stdin]);
    break;
  case Custom:
    preparecustomblks(Blks[Custom], NBlks[Custom]);
    break;
  }
  clearbar();
  xrenderblks(Blks[Stdin], NBlks[Stdin], Infos[Stdin]);
  xrenderblks(Blks[Custom], NBlks[Custom], Infos[Custom]);
  XUnlockDisplay(dpy);
}

void handle_xbuttonpress(XButtonEvent *btn, Block Blks[2][MAX_BLKS],
                         int NBlks[2]) {
  for (int i = 0; i < NBlks[Stdin] + NBlks[Custom]; ++i) {
    Block *blk =
        i < NBlks[Stdin] ? &Blks[Stdin][i] : &Blks[Custom][i - NBlks[Stdin]];
    RenderInfo *ri =
        i < NBlks[Stdin] ? &Infos[Stdin][i] : &Infos[Custom][i - NBlks[Stdin]];

    if (btn->x >= ri->x && btn->x <= ri->x + ri->width) {
      Tag tag = btn->button == Button1   ? BtnL
                : btn->button == Button2 ? BtnM
                : btn->button == Button3 ? BtnR
                : btn->button == Button4 ? ScrlU
                : btn->button == Button5 ? ScrlD
                                         : NullTag;

      Extension ext = btn->state == ShiftMask     ? Shift
                      : btn->state == ControlMask ? Ctrl
                      : btn->state == Mod1Mask    ? Super
                      : btn->state == Mod4Mask    ? Alt
                                                  : NullExt;

      if (tag != NullTag) {
        Attribute *action = blk->attrs[tag];
        while (action != NULL) {
          if (strlen(action->val) &&
              (action->extension == NullExt || action->extension == ext))
            system(action->val);
          action = action->previous;
        }
      }
      break;
    }
  }
}

int handle_xevent(Block Blks[2][MAX_BLKS], int NBlks[2], char *name,
                  int *exposed) {
  if (XPending(dpy)) {
    XNextEvent(dpy, &e);

    if (e.type == Expose) {
      if (XStoreName(dpy, root, NAME "-" VERSION) == BadAlloc)
        eprintf("XStoreName failed.\n");
      *exposed = 1;
    }

    if (e.type == PropertyNotify && e.xproperty.window == root &&
        XFetchName(dpy, root, &wm_name) >= 0) {
      strcpy(name, wm_name);
      XFree(wm_name);
      return 1;
    }

    if (e.type == ButtonPress)
      handle_xbuttonpress(&e.xbutton, Blks, NBlks);
  }

  return 0;
}
