#include "config.h"
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <blocks.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define NAME "xdbar"
#define VERSION "0.1.0"
#define STDIN_BUF_SIZE 1<<10

#define eprintf(...) fprintf(stderr, __VA_ARGS__);
#define die(...)                                                               \
  do {                                                                         \
    eprintf("[ERROR] ");                                                       \
    eprintf(__VA_ARGS__);                                                      \
  } while (0);

static const long NS_TO_MS = 1e6;

typedef struct _ColorCache {
  char *name;
  XftColor val;
  struct _ColorCache *previous;
} ColorCache;

typedef struct {
  int nfonts, nrects;
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
void xrenderblks(Block *, int);

static Context *ctx;
static Bar *bar;
static Display *dpy;
static int scr;
static Window root;

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

void createrenderinfo(const char *text, int ntext, int fnindex,
                      RenderInfo *renderinfo) {
  XGlyphInfo extent;
  XftTextExtentsUtf8(dpy, ctx->fonts[fnindex], (FcChar8 *)text, ntext, &extent);
  renderinfo->x = extent.x;
  renderinfo->width = extent.xOff;
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
  bar->w = 1366, bar->h = barheight, bar->x = 0,
  bar->y = topbar ? 0 : 768 - bar->h;

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

void xrenderblks(Block *blks, int nblk) {
  int starty = (bar->h - ctx->fonts[0]->height) / 2 + ctx->fonts[0]->ascent,
      size;
  char color[24];
  XftColor *fg, *bg;
  Attribute *box;

  for (int i = 0; i < nblk; ++i) {
    Block *blk = &blks[i];

    fg = blk->attrs[Fg] != NULL ? get_cached_color(blk->attrs[Fg]->val)
                                : &bar->foreground;
    bg = blk->attrs[Bg] != NULL ? get_cached_color(blk->attrs[Bg]->val)
                                : &bar->background;

    XftDrawRect(bar->drawable, bg, blk->renderinfo.x, 0, blk->renderinfo.width,
                bar->h);

    box = blk->attrs[Box];
    while (box != NULL) {
      size = parseboxstring(box->val, color);
      if (size) {
        int bx = 0, by = 0, bw = 0, bh = 0;
        switch (box->extension) {
        case Top:
          bx = blk->renderinfo.x, by = 0,
          bw = blk->renderinfo.x + blk->renderinfo.width, bh = size;
          break;
        case Bottom:
          bx = blk->renderinfo.x, by = bar->h - size,
          bw = blk->renderinfo.x + blk->renderinfo.width, bh = size;
          break;
        case Left:
          bx = blk->renderinfo.x, by = 0, bw = size, bh = bar->h;
          break;
        case Right:
          bx = blk->renderinfo.x + blk->renderinfo.width - size, by = 0,
          bw = size, bh = bar->h;
          break;
        default:
          break;
        }
        XftColor *rectcolor = get_cached_color(color);
        XftDrawRect(bar->drawable, rectcolor, bx, by, bw, bh);
      }
      box = box->previous;
    }

    int fnindex = blk->attrs[Fn] != NULL ? atoi(blk->attrs[Fn]->val) : 0;
    XftDrawStringUtf8(bar->drawable, fg, ctx->fonts[fnindex], blk->renderinfo.x,
                      starty, (FcChar8 *)blk->text, blk->ntext);
  }
}

void handlebuttonpress(Block *blk, XButtonEvent *e) {
  AttrTag tag = e->button == Button1   ? BtnL
                : e->button == Button2 ? BtnM
                : e->button == Button3 ? BtnR
                : e->button == Button4 ? ScrlU
                : e->button == Button5 ? ScrlD
                                       : NullAttrTag;

  AttrExt ext = e->state == ShiftMask     ? Shift
                : e->state == ControlMask ? Ctrl
                : e->state == Mod1Mask    ? Super
                : e->state == Mod4Mask    ? Alt
                                          : NullAttrExt;

  // @TODO: weird state/mask behaviour.
  if (tag != NullAttrTag) {
    Attribute *action = blk->attrs[tag];
    while (action != NULL) {
      if (strlen(action->val) &&
          (action->extension == NullAttrExt || action->extension == ext))
        system(action->val);
      action = action->previous;
    }
  }
}

void createleftblks(char *buf, Block *blks, int *nblks) {
  freeblks(blks, *nblks);
  *nblks = createblks(buf, blks);

  int renderx = 0;
  for (int i = 0; i < *nblks; ++i) {
    blks[i].renderinfo.x = renderx + blks[i].renderinfo.x;
    renderx += blks[i].renderinfo.width;
  }
}

void createrightblks(char *buf, Block *blks, int *nblks) {
  freeblks(blks, *nblks);
  *nblks = createblks(buf, blks);

  int renderx = bar->w;
  for (int i = *nblks - 1; i >= 0; --i) {
    renderx -= blks[i].renderinfo.x + blks[i].renderinfo.width;
    blks[i].renderinfo.x = renderx;
  }
}

void getlastline(char *buf, int nchars, char *dest, int ndest) {
  memset(dest, 0, ndest);
  int i = nchars - 1 - (int)(buf[nchars - 1] == '\n');
  for (; i >= 0; --i) {
    if (buf[i] == '\n') {
      strncpy(dest, &buf[i + 1], nchars - i - 2);
      break;
    }
  }
  if (i < 0)
    strncpy(dest, buf, nchars - 1);
}

int main(void) {
  XEvent e;
  int nbuf, renderflag, eventflag;
  char stdinbuf[STDIN_BUF_SIZE], *namebuf;
  int nleftblks = 0, nrightblks = 0;
  Block leftblks[64], rightblks[64];

  struct timespec ts = {.tv_sec = 0, .tv_nsec = (long)25 * NS_TO_MS};
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

  xsetup();
  while (1) {
    nanosleep(&ts, &ts);
    renderflag = 0, eventflag = 0;

    if (XPending(dpy)) {
      XNextEvent(dpy, &e);
      eventflag = 1;
    }

    if ((nbuf = read(STDIN_FILENO, &stdinbuf, STDIN_BUF_SIZE)) > 0) {
      char line[nbuf];
      getlastline(stdinbuf, nbuf, line, nbuf);
      createleftblks(line, leftblks, &nleftblks);
      renderflag = 1;
    }

    if (eventflag) {
      if ((e.type == Expose || e.xproperty.window == root) &&
          XFetchName(dpy, root, &namebuf) >= 0) {
        createrightblks(namebuf, rightblks, &nrightblks);
        renderflag = 1;
      }

      if (e.type == ButtonPress) {
        for (int i = 0; i < nleftblks + nrightblks; ++i) {
          Block *blk = i < nleftblks ? &leftblks[i] : &rightblks[i - nleftblks];
          if (e.xbutton.x >= blk->renderinfo.x &&
              e.xbutton.x <= blk->renderinfo.x + blk->renderinfo.width)
            handlebuttonpress(blk, &e.xbutton);
        }
      }
    }

    if (renderflag) {
      clearbar();
      xrenderblks(leftblks, nleftblks);
      xrenderblks(rightblks, nrightblks);
    }
  }

  xcleanup();
  return 0;
}
