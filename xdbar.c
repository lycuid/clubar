#include "config.h"
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <blocks.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__);
#define die(...)                                                               \
  do {                                                                         \
    eprintf("[ERROR] ");                                                       \
    eprintf(__VA_ARGS__);                                                      \
    exit(1);                                                                   \
  } while (0);

#define STDIN_BUF_SIZE 1 << 10
#define BLOCKS_SIZE 1 << 6

typedef struct {
  int x, width;
} RenderInfo;

typedef struct _ColorCache {
  char *name;
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
  int x, y, w, h;
  XftDraw *drawable;
  Window xwindow;
  XftColor foreground, background;
} Bar;

XftColor *get_cached_color(char *);
void createctx();
void createbar();
void xsetup();
void xsetatoms();
void xcleanup();
void xrenderblks(Block **, int, RenderInfo **);

static Context *ctx;
static Bar *bar;
static Display *dpy;
static int scr;
static Window root;

static Block *BLKS[2][BLOCKS_SIZE];
static RenderInfo *INFOS[2][BLOCKS_SIZE];
static int NBLKS[2];

XftColor *get_cached_color(char *colorname) {
  ColorCache *cache = ctx->colorcache;
  while (cache != NULL) {
    if (strcmp(cache->name, colorname) == 0)
      return &cache->val;
    cache = cache->previous;
  }

  // @TODO: currently not handling error when allocating colors.
  ColorCache *color = (ColorCache *)malloc(sizeof(ColorCache));
  XftColorAllocName(dpy, ctx->vis, ctx->cmap, colorname, &color->val);
  color->name = malloc(strlen(colorname));
  strcpy(color->name, colorname);
  color->previous = ctx->colorcache;

  ctx->colorcache = color;

  return &ctx->colorcache->val;
}

int parseboxstring(const char *val, char color[24]) {
  int ptr = 0, c = 0, nval = strlen(val), size = 0;
  memset(color, 0, 24);

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
  ctx = (Context *)malloc(sizeof(Context));
  ctx->vis = DefaultVisual(dpy, scr);
  ctx->cmap = DefaultColormap(dpy, scr);

  ctx->nfonts = sizeof(fonts) / sizeof *fonts + 1;

  ctx->fonts = (XftFont **)malloc(ctx->nfonts * sizeof(XftFont *));
  ctx->fonts[0] = XftFontOpenName(dpy, scr, font);
  for (int i = 1; i < ctx->nfonts; ++i)
    ctx->fonts[i] = XftFontOpenName(dpy, scr, fonts[i - 1]);

  ctx->colorcache = NULL;
}

void createbar() {
  bar = (Bar *)malloc(sizeof(Bar));
  // @TODO: remove hard coded values.
  bar->x = 0, bar->y = 768 - barheight, bar->w = barwidth, bar->h = barheight;

  bar->xwindow =
      XCreateSimpleWindow(dpy, root, bar->x, bar->y, bar->w, bar->h, 0,
                          WhitePixel(dpy, scr), BlackPixel(dpy, scr));

  XftColorAllocName(dpy, ctx->vis, ctx->cmap, foreground, &bar->foreground);
  XftColorAllocName(dpy, ctx->vis, ctx->cmap, background, &bar->background);

  bar->drawable = XftDrawCreate(dpy, bar->xwindow, ctx->vis, ctx->cmap);
}

void clearbar() {
  XftDrawRect(bar->drawable, &bar->background, 0, 0, bar->w, bar->h);
}

void xsetup() {
  if ((dpy = XOpenDisplay(NULL)) == NULL)
    die("Cannot open display.\n");
  scr = DefaultScreen(dpy);
  root = DefaultRootWindow(dpy);
  if (XStoreName(dpy, root, NAME "-" VERSION) < 0)
    eprintf("XStoreName failed.\n");

  createctx();
  createbar();

  XSelectInput(dpy, root, PropertyChangeMask);
  XSelectInput(dpy, bar->xwindow, ExposureMask | ButtonPress);
  xsetatoms();
  XMapWindow(dpy, bar->xwindow);
}

void xsetatoms() {
  long top = topbar ? bar->h : 0;
  long bottom = !topbar ? bar->h : 0;
  /* left, right, top, bottom, left_start_y, left_end_y, right_start_y, */
  /* right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x */
  long strut[12] = {0, 0, top, bottom, 0, 0, 0, 0, 0, 0, 0, 0};

  XChangeProperty(dpy, bar->xwindow, XInternAtom(dpy, "_NET_WM_STRUT", 0),
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 4);
  XChangeProperty(dpy, bar->xwindow,
                  XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", 0), XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)strut, 12l);

  Atom property[2];
  property[0] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", 0);
  property[1] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", 0);
  XChangeProperty(dpy, bar->xwindow, property[1], XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)property, 1l);

  XClassHint *classhint = XAllocClassHint();
  classhint->res_name = NAME;
  classhint->res_class = NAME;
  XSetClassHint(dpy, bar->xwindow, classhint);
  XStoreName(dpy, bar->xwindow, NAME);
}

void xcleanup() {
  while (ctx->colorcache != NULL) {
    XftColorFree(dpy, ctx->vis, ctx->cmap, &ctx->colorcache->val);
    ColorCache *stale = ctx->colorcache;
    ctx->colorcache = ctx->colorcache->previous;
    free(stale);
  }

  XftColorFree(dpy, ctx->vis, ctx->cmap, &bar->foreground);
  XftColorFree(dpy, ctx->vis, ctx->cmap, &bar->background);
  XftDrawDestroy(bar->drawable);
  XCloseDisplay(dpy);
}

void xrenderblks(Block **blks, int nblk, RenderInfo **renderinfos) {
  int starty, size, fnindex;
  char color[24];
  XftColor *fg;
  Attribute *box;

  for (int i = 0; i < nblk; ++i) {
    Block *blk = blks[i];
    RenderInfo *ri = renderinfos[i];

    if (blk->attrs[Bg] != NULL)
      XftDrawRect(bar->drawable, get_cached_color(blk->attrs[Bg]->val), ri->x,
                  0, ri->width, bar->h);

    box = blk->attrs[Box];
    while (box != NULL) {
      size = parseboxstring(box->val, color);
      if (size) {
        int bx = 0, by = 0, bw = 0, bh = 0;
        switch (box->extension) {
        case Top:
          bx = ri->x, by = 0, bw = ri->width, bh = size;
          break;
        case Bottom:
          bx = ri->x, by = bar->h - size, bw = ri->width, bh = size;
          break;
        case Left:
          bx = ri->x, by = 0, bw = size, bh = bar->h;
          break;
        case Right:
          bx = ri->x + ri->width - size, by = 0, bw = size, bh = bar->h;
          break;
        default:
          break;
        }
        XftColor *rectcolor = get_cached_color(color);
        XftDrawRect(bar->drawable, rectcolor, bx, by, bw, bh);
      }
      box = box->previous;
    }

    fnindex = blk->attrs[Fn] != NULL ? atoi(blk->attrs[Fn]->val) : 0;
    starty = (bar->h - ctx->fonts[fnindex]->height) / 2 +
             ctx->fonts[fnindex]->ascent;
    fg = blk->attrs[Fg] != NULL ? get_cached_color(blk->attrs[Fg]->val)
                                : &bar->foreground;
    XftDrawStringUtf8(bar->drawable, fg, ctx->fonts[fnindex], ri->x, starty,
                      (FcChar8 *)blk->text, blk->ntext);
  }
}

void handle_buttonpress(XButtonEvent *btn) {
  for (int i = 0; i < NBLKS[0] + NBLKS[1]; ++i) {
    Block *blk = i < NBLKS[0] ? BLKS[0][i] : BLKS[1][i - NBLKS[0]];
    RenderInfo *ri = i < NBLKS[0] ? INFOS[0][i] : INFOS[1][i - NBLKS[0]];

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
  // @TODO: indentify and fix this problem.
  // *problem: stdin starts blocking in some cases, after calling 'system' func.
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

void freeinfos(RenderInfo **infos) {
  for (int i = 0; i < BLOCKS_SIZE; ++i) {
    if (infos[i] != NULL) {
      free(infos[i]);
      infos[i] = NULL;
    }
  }
}

void handle_stdin(char *stdinbuf, size_t nbuf) {
  for (int i = 0; i < nbuf; ++i) {
    if (stdinbuf[i] == '\n') {
      stdinbuf[i] = 0;
      break;
    }
  }

  freeblks(BLKS[0], BLOCKS_SIZE);
  NBLKS[0] = createblks(stdinbuf, BLKS[0]);

  int renderx = 0;
  XGlyphInfo extent;
  freeinfos(INFOS[0]);
  for (int i = 0; i < NBLKS[0]; ++i) {
    Block *blk = BLKS[0][i];
    int fnindex = blk->attrs[Fn] ? atoi(blk->attrs[Fn]->val) : 0;
    XftTextExtentsUtf8(dpy, ctx->fonts[fnindex], (FcChar8 *)blk->text,
                       blk->ntext, &extent);

    INFOS[0][i] = (RenderInfo *)malloc(sizeof(RenderInfo));
    INFOS[0][i]->width = extent.xOff;
    INFOS[0][i]->x = renderx + extent.x;
    renderx += INFOS[0][i]->width;
  }

  memset(stdinbuf, 0, STDIN_BUF_SIZE);
}

void handle_wmname(char *wmname) {
  freeblks(BLKS[1], BLOCKS_SIZE);
  NBLKS[1] = createblks(wmname, BLKS[1]);

  int renderx = bar->w;
  XGlyphInfo extent;
  freeinfos(INFOS[1]);
  for (int i = NBLKS[1] - 1; i >= 0; --i) {
    Block *blk = BLKS[1][i];
    int fnindex = blk->attrs[Fn] ? atoi(blk->attrs[Fn]->val) : 0;
    XftTextExtentsUtf8(dpy, ctx->fonts[fnindex], (FcChar8 *)blk->text,
                       blk->ntext, &extent);

    INFOS[1][i] = (RenderInfo *)malloc(sizeof(RenderInfo));
    INFOS[1][i]->width = extent.xOff;
    renderx -= extent.x + extent.xOff;
    INFOS[1][i]->x = renderx;
  }
}

int main(void) {
  signal(SIGINT, xcleanup);
  signal(SIGHUP, xcleanup);
  signal(SIGTERM, xcleanup);

  XEvent e;
  char *wmname;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < BLOCKS_SIZE; ++j) {
      BLKS[i][j] = NULL;
      INFOS[i][j] = NULL;
    }
    NBLKS[i] = 0;
  }

  size_t nbuf;
  char stdinbuf[STDIN_BUF_SIZE];
  struct timespec ts = {.tv_nsec = 1e6 * 25};
  int renderflag = 0;

  memset(stdinbuf, 0, STDIN_BUF_SIZE);
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

  xsetup();
  while (1) {
    nanosleep(&ts, &ts);
    renderflag = 0;

    if (XPending(dpy)) {
      XNextEvent(dpy, &e);
      if (e.type == ButtonPress)
        handle_buttonpress(&e.xbutton);
      if (e.xproperty.window == root && XFetchName(dpy, root, &wmname) >= 0) {
        handle_wmname(wmname);
        renderflag = 1;
      }
    }

    if ((nbuf = read(STDIN_FILENO, &stdinbuf, STDIN_BUF_SIZE)) != -1) {
      handle_stdin(stdinbuf, nbuf);
      renderflag = 1;
    }

    if (renderflag) {
      clearbar();
      xrenderblks(BLKS[0], NBLKS[0], INFOS[0]);
      xrenderblks(BLKS[1], NBLKS[1], INFOS[1]);
    }
  }

  xcleanup();
  return 0;
}
