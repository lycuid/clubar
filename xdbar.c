#include "config.h"
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <blocks.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__);
#define die(...)                                                               \
  do {                                                                         \
    eprintf("[ERROR] ");                                                       \
    eprintf(__VA_ARGS__);                                                      \
    exit(1);                                                                   \
  } while (0);

#define STDIN_BUF_SIZE (1 << 10)
#define MAX_BLKS (1 << 6)

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

XftColor *get_cached_color(const char *);
int parseboxstring(const char *, char[32]);
void createctx();
void createbar();
void clearbar();
void xsetup();
void xsetatoms();
void xcleanup();
void xrenderblks(Block[MAX_BLKS], int, RenderInfo[MAX_BLKS]);
void handle_buttonpress(XButtonEvent *);
void render();
void *spawn_stdin_listener();
void handle_wmname(const char *);

Context ctx;
Bar bar;
Display *dpy;
int scr;
Window rootwindow;

Block Blks[2][MAX_BLKS];
RenderInfo Infos[2][MAX_BLKS];
int NBlks[2];

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

void createctx() {
  ctx.vis = DefaultVisual(dpy, scr);
  ctx.cmap = DefaultColormap(dpy, scr);

  ctx.nfonts = sizeof(fonts) / sizeof *fonts;

  ctx.fonts = (XftFont **)malloc(ctx.nfonts * sizeof(XftFont *));
  for (int i = 0; i < ctx.nfonts; ++i)
    ctx.fonts[i] = XftFontOpenName(dpy, scr, fonts[i]);

  ctx.colorcache = NULL;
}

void createbar() {
  Geometry geometry = barConfig.geometry;
  bar.window_g = (Geometry){geometry.x, geometry.y, geometry.w, geometry.h};

  bar.canvas_g.x = barConfig.padding.left;
  bar.canvas_g.y = barConfig.padding.top;
  bar.canvas_g.w =
      bar.window_g.w - barConfig.padding.left - barConfig.padding.right;
  bar.canvas_g.h =
      bar.window_g.h - barConfig.padding.top - barConfig.padding.bottom;

  bar.xwindow = XCreateSimpleWindow(
      dpy, rootwindow, bar.window_g.x, bar.window_g.y, bar.window_g.w,
      bar.window_g.h, 0, WhitePixel(dpy, scr), BlackPixel(dpy, scr));

  XftColorAllocName(dpy, ctx.vis, ctx.cmap, barConfig.foreground,
                    &bar.foreground);
  XftColorAllocName(dpy, ctx.vis, ctx.cmap, barConfig.background,
                    &bar.background);

  bar.canvas = XftDrawCreate(dpy, bar.xwindow, ctx.vis, ctx.cmap);
}

void clearbar() {
  XftDrawRect(bar.canvas, &bar.background, 0, 0, bar.window_g.w,
              bar.window_g.h);
}

void xsetup() {
  if ((dpy = XOpenDisplay(NULL)) == NULL)
    die("Cannot open display.\n");
  scr = DefaultScreen(dpy);
  rootwindow = DefaultRootWindow(dpy);
  if (XStoreName(dpy, rootwindow, NAME "-" VERSION) < 0)
    eprintf("XStoreName failed.\n");

  createctx();
  createbar();

  XSelectInput(dpy, rootwindow, PropertyChangeMask);
  XSelectInput(dpy, bar.xwindow, ExposureMask | ButtonPress);
  xsetatoms();
  XMapWindow(dpy, bar.xwindow);
}

void xsetatoms() {
  long barheight =
      bar.window_g.h + barConfig.margin.top + barConfig.margin.bottom;
  long top = barConfig.topbar ? barheight : 0;
  long bottom = !barConfig.topbar ? barheight : 0;
  /* left, right, top, bottom, left_start_y, left_end_y, right_start_y, */
  /* right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x */
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

  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < MAX_BLKS; ++j)
      free(Blks[i][j].text);

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

void handle_buttonpress(XButtonEvent *btn) {
  for (int i = 0; i < NBlks[0] + NBlks[1]; ++i) {
    Block *blk = i < NBlks[0] ? &Blks[0][i] : &Blks[1][i - NBlks[0]];
    RenderInfo *ri = i < NBlks[0] ? &Infos[0][i] : &Infos[1][i - NBlks[0]];

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

void render() {
  XLockDisplay(dpy);
  clearbar();
  xrenderblks(Blks[0], NBlks[0], Infos[0]);
  xrenderblks(Blks[1], NBlks[1], Infos[1]);
  XUnlockDisplay(dpy);
}

void *spawn_stdin_listener() {
  size_t alloc = STDIN_BUF_SIZE, nbuf;
  char *stdinbuf = malloc(alloc);
  char previous[STDIN_BUF_SIZE];
  memset(stdinbuf, 0, STDIN_BUF_SIZE);
  memset(previous, 0, STDIN_BUF_SIZE);

  while ((nbuf = getline(&stdinbuf, &alloc, stdin)) > 1) {
    stdinbuf[nbuf - 1] = 0;
    if (strcmp(previous, stdinbuf) == 0)
      continue;
    else
      strcpy(previous, stdinbuf);

    freeblks(Blks[0], MAX_BLKS);
    NBlks[0] = createblks(stdinbuf, Blks[0]);
    memset(stdinbuf, 0, STDIN_BUF_SIZE);

    int renderx = 0;
    XGlyphInfo extent;
    for (int i = 0; i < NBlks[0]; ++i) {
      Block *blk = &Blks[0][i];
      int fnindex = blk->attrs[Fn] ? atoi(blk->attrs[Fn]->val) : 0;
      XftTextExtentsUtf8(dpy, ctx.fonts[fnindex], (FcChar8 *)blk->text,
                         strlen(blk->text), &extent);

      Infos[0][i].width = extent.xOff;
      Infos[0][i].x = renderx + extent.x;
      renderx += Infos[0][i].width;
    }
    render();
  }
  pthread_exit(0);
}

void handle_wmname(const char *wmname) {
  freeblks(Blks[1], MAX_BLKS);
  NBlks[1] = createblks(wmname, Blks[1]);

  int renderx = bar.canvas_g.x + bar.canvas_g.w;
  XGlyphInfo extent;
  for (int i = NBlks[1] - 1; i >= 0; --i) {
    Block *blk = &Blks[1][i];
    int fnindex = blk->attrs[Fn] ? atoi(blk->attrs[Fn]->val) : 0;
    XftTextExtentsUtf8(dpy, ctx.fonts[fnindex], (FcChar8 *)blk->text,
                       strlen(blk->text), &extent);

    Infos[1][i].width = extent.xOff;
    renderx -= extent.x + extent.xOff;
    Infos[1][i].x = renderx;
  }
}

int main(void) {
  XInitThreads();
  XEvent e;
  char *wmname;
  pthread_t thread_handle;
  struct timespec ts = {.tv_nsec = 1e6 * 25};

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < MAX_BLKS; ++j)
      // using 'malloc' instead of stack memory, because, later we might need
      // to add use 'realloc' (Currently doesn't seem to be a problem, but has
      // potential for overflow).
      Blks[i][j].text = malloc(64);
    NBlks[i] = 0;
  }

  xsetup();
  signal(SIGINT, xcleanup);
  signal(SIGHUP, xcleanup);
  signal(SIGTERM, xcleanup);

  while (1) {
    nanosleep(&ts, &ts);

    if (XPending(dpy)) {
      XNextEvent(dpy, &e);
      if (e.type == Expose)
        pthread_create(&thread_handle, NULL, spawn_stdin_listener, NULL);
      if (e.type == ButtonPress)
        handle_buttonpress(&e.xbutton);
      if (e.xproperty.window == rootwindow &&
          XFetchName(dpy, rootwindow, &wmname) >= 0) {
        handle_wmname(wmname);
        render();

        // @VALGRIND: 'XFetchName' using 'malloc' everytime.
        // This improved memory usage significantly.
        free(wmname);
      }
    }
  }

  pthread_join(thread_handle, NULL);
  xcleanup();
  return 0;
}
