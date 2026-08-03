#include "gui.h"
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <unistd.h>

static CluBar _clubar = {0};
CluBar *clubar        = &_clubar;

Display *_dpy;

typedef struct GlyphInfo {
    int x, width;
} GlyphInfo;

typedef struct ColorCache {
    char name[32];
    XftColor val;
    struct ColorCache *prev, *next;
} ColorCache;

#define CC_ATTACH(c)                                                           \
    do { /* Attach item on top of the 'drw.colorcache' linked list.*/          \
        if ((c->prev = NULL, c->next = drw.colorcache))                        \
            drw.colorcache->prev = c;                                          \
        drw.colorcache = c;                                                    \
    } while (0)

#define CC_DETACH(c)                                                           \
    do { /* Detach item from the 'drw.colorcache' linked list.*/               \
        (void)(c->prev ? (c->prev->next = c->next)                             \
                       : (drw.colorcache = c->next));                          \
        (void)(c->next ? (c->next->prev = c->prev) : 0);                       \
    } while (0)

#define CC_FREE(cc)                                                            \
    do {                                                                       \
        __typeof__(cc) tmp = (cc);                                             \
        CC_DETACH(tmp);                                                        \
        XftColorFree(dpy(), vis(), cmap(), &(tmp)->val);                       \
        free(tmp);                                                             \
    } while (0)

static struct DrawContext {
    int nfonts;
    XftFont **fonts;
    GlyphInfo gis[2][MAX_BLKS];
    ColorCache *colorcache;
} drw = {0};

static struct Bar {
    Window window;
    XftDraw *canvas;
    Geometry window_g, canvas_g;
    XftColor foreground, background;
} bar = {0};

enum { WMName, NetWMWindowType, NetWMDock, NetWMStrut, NullWMAtom };
Atom atoms[NullWMAtom];

#define fill_rect(...)    XftDrawRect(bar.canvas, &bar.background, __VA_ARGS__)
#define alloc_color(p, c) XftColorAllocName(dpy(), vis(), cmap(), c, p)

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

static inline void drw_init(const Config *config)
{
    // As this function can be called multiple times, we need to deallocate the
    // previously allocated stuff.
    for (int i = 0; i < drw.nfonts; ++i)
        XftFontClose(dpy(), drw.fonts[i]);
    if (drw.fonts) {
        free(drw.fonts);
        drw.fonts = NULL;
    }

    drw.nfonts = config->nfonts;
    drw.fonts  = (XftFont **)realloc(drw.fonts, drw.nfonts * sizeof(XftFont *));
    for (int i = 0; i < drw.nfonts; ++i)
        drw.fonts[i] = XftFontOpenName(dpy(), scr(), config->fonts[i]);

    while (drw.colorcache)
        CC_FREE(drw.colorcache);
    drw.colorcache = NULL;
}

static inline void bar_init(const Config *config)
{
    Geometry geometry = config->geometry;

    if (!bar.window)
        bar.window = XCreateSimpleWindow(dpy(), root(), 0, 0, 10, 10, 0, 0, 0);
    if (!bar.canvas)
        bar.canvas = XftDrawCreate(dpy(), bar.window, vis(), cmap());

    /* window geometry and placement. */ {
        bar.window_g   = geometry;
        bar.canvas_g.x = config->padding.left;
        bar.canvas_g.y = config->padding.top;
        bar.canvas_g.w =
            bar.window_g.w - config->padding.left - config->padding.right;
        bar.canvas_g.h =
            bar.window_g.h - config->padding.top - config->padding.bottom;
        XMoveResizeWindow(dpy(), bar.window, bar.window_g.x, bar.window_g.y,
                          bar.window_g.w, bar.window_g.h);
    }

    /* allocating foreground and background colors for the window. */ {
        // @TODO: free these colors on reload config.

        XftColorFree(dpy(), vis(), cmap(), &bar.foreground);
        if (!alloc_color(&bar.foreground, config->foreground))
            alloc_color(&bar.foreground, "#ffffff");

        XftColorFree(dpy(), vis(), cmap(), &bar.background);
        if (!alloc_color(&bar.background, config->background))
            alloc_color(&bar.background, "#000000");
    }

    /* update window properties. */ {
        XSetWindowAttributes attrs = {
            .event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask,
            .override_redirect = True,
        };
        XChangeWindowAttributes(dpy(), bar.window,
                                CWEventMask | CWOverrideRedirect, &attrs);

        XChangeProperty(dpy(), bar.window, atoms[NetWMWindowType], XA_ATOM, 32,
                        PropModeReplace, (uint8_t *)&atoms[NetWMDock],
                        sizeof(Atom));

        long barheight = bar.window_g.h + clubar->config.margin.top +
                         clubar->config.margin.bottom;
        long strut[4] = {0, 0, clubar->config.topbar ? barheight : 0,
                         !clubar->config.topbar ? barheight : 0};
        XChangeProperty(dpy(), bar.window, atoms[NetWMStrut], XA_CARDINAL, 32,
                        PropModeReplace, (uint8_t *)strut, 4l);
        if (strlen(clubar->config.border_color) > 0) {
            XftColor *color = request_color(clubar->config.border_color);
            XSetWindowBorder(dpy(), bar.window, color->pixel);
            XSetWindowBorderWidth(dpy(), bar.window,
                                  clubar->config.border_width);
        } else {
            XSetWindowBorderWidth(dpy(), bar.window, 0);
        }
    }
}

static inline void generate_stdin_gis(void)
{
    XGlyphInfo extent;
    int fntindex, startx = 0;
    for (int i = 0; i < clubar->nblks[Stdin]; ++i) {
        const Block *blk = &clubar->blks[Stdin][i];
        fntindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
        XftTextExtentsUtf8(dpy(), drw.fonts[fntindex], (FcChar8 *)blk->text,
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
    for (int i = clubar->nblks[Custom] - 1; i >= 0; --i) {
        const Block *blk = &clubar->blks[Custom][i];
        fntindex = blk->tags[Fn] ? atoi(blk->tags[Fn]->val) % drw.nfonts : 0;
        XftTextExtentsUtf8(dpy(), drw.fonts[fntindex], (FcChar8 *)blk->text,
                           strlen(blk->text), &extent);
        drw.gis[Custom][i].width = extent.xOff;
        startx -= extent.x + extent.xOff;
        drw.gis[Custom][i].x = startx;
    }
}

static inline void xrender_bg(const Block *blk, const GlyphInfo *gi)
{
    XftDrawRect(bar.canvas, request_color(blk->tags[Bg]->val), gi->x,
                bar.canvas_g.y, gi->width, bar.canvas_g.h);
}

static inline void xrender_box(const Block *blk, const GlyphInfo *gi)
{
    char color[32];
    const Geometry *canvas_g = &bar.canvas_g;
    for (Tag *box = blk->tags[Box]; box != NULL; box = box->previous) {
        int size = parse_color_string(box->val, color);
        if (!size)
            continue;
        for (TagModifier tmod = 0; tmod != NullTagModifier; ++tmod) {
            int bx = canvas_g->x, by = canvas_g->y, bw = 0, bh = 0;
            if (box->tmod_mask & (1 << tmod)) {
                switch (tmod) {
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
                    bx = gi->x, by = canvas_g->y + canvas_g->h - size,
                    bw = gi->width, bh = size;
                } break;
                default: break;
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
    if (dpy())
        close(ConnectionNumber(dpy()));
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

bool get_window_name(char *buffer)
{
    char *wm_name;
    if (XFetchName(dpy(), root(), &wm_name) && wm_name) {
        strcpy(buffer, wm_name);
        XFree(wm_name);
        return true;
    }
    return false;
}

void onExpose(void) { fill_rect(0, 0, bar.window_g.w, bar.window_g.h); }

void onMapNotify(const XEvent *xevent, char *name)
{
    (void)xevent;
    XSelectInput(dpy(), root(), PropertyChangeMask);
    get_window_name(name);
}

bool onPropertyNotify(const XEvent *xevent, char *name)
{
    const XPropertyEvent *e = &xevent->xproperty;
    if (e->window == root() && e->atom == atoms[WMName])
        return get_window_name(name);
    return false;
}

void onButtonPress(const XEvent *xevent)
{
    const XButtonEvent *e     = &xevent->xbutton;
    TagName tag_name          = NullTagName;
    TagModifierMask tmod_mask = 0x0;
    for (int i = 0; i < clubar->nblks[Stdin] + clubar->nblks[Custom]; ++i) {
        const Block *blk =
            i < clubar->nblks[Stdin]
                ? &clubar->blks[Stdin][i]
                : &clubar->blks[Custom][i - clubar->nblks[Stdin]];
        const GlyphInfo *gi = i < clubar->nblks[Stdin]
                                  ? &drw.gis[Stdin][i]
                                  : &drw.gis[Custom][i - clubar->nblks[Stdin]];
        // check if click event coordinates match with any coordinate on the bar
        // window.
        if (e->x >= gi->x && e->x <= gi->x + gi->width) {

            switch (e->button) {
            case Button1: tag_name = BtnL; break;
            case Button2: tag_name = BtnM; break;
            case Button3: tag_name = BtnR; break;
            case Button4: tag_name = ScrlU; break;
            case Button5: tag_name = ScrlD; break;
            default: tag_name = NullTagName; break;
            }

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

void gui_init(void)
{
    for (int i = 0; i < MAX_BLKS; ++i)
        drw.gis[Stdin][i] = drw.gis[Custom][i] = (GlyphInfo){0, 0};
    if ((dpy() = XOpenDisplay(NULL)) == NULL)
        die("Cannot open display.\n");

    atoms[WMName]          = XInternAtom(dpy(), "WM_NAME", False);
    atoms[NetWMDock]       = XInternAtom(dpy(), "_NET_WM_WINDOW_TYPE_DOCK", 0);
    atoms[NetWMStrut]      = XInternAtom(dpy(), "_NET_WM_STRUT", 0);
    atoms[NetWMWindowType] = XInternAtom(dpy(), "_NET_WM_WINDOW_TYPE", 0);
}

void gui_load(void)
{
    drw_init(&clubar->config);
    bar_init(&clubar->config);
    fill_rect(0, 0, bar.window_g.w, bar.window_g.h);

    XStoreName(dpy(), bar.window, NAME);
    XSetClassHint(dpy(), bar.window,
                  &(XClassHint){.res_name = NAME, .res_class = NAME});

    XMapWindow(dpy(), bar.window);
}

void gui_toggle(void)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(dpy(), bar.window, &attrs);
    attrs.map_state == IsUnmapped ? XMapWindow(dpy(), bar.window)
                                  : XUnmapWindow(dpy(), bar.window);
}

void gui_clear(BlockType blktype)
{
    if (clubar->nblks[blktype]) {
        GlyphInfo *first = &drw.gis[blktype][0],
                  *last  = &drw.gis[blktype][clubar->nblks[blktype] - 1];
        fill_rect(first->x, 0, last->x + last->width, bar.window_g.h);
    }
}

void gui_draw(BlockType blktype)
{
    switch (blktype) {
    case Stdin: {
        generate_stdin_gis();
    } break;
    case Custom: {
        generate_custom_gis();
    } break;
    }
    for (int i = 0; i < clubar->nblks[blktype]; ++i) {
        const Block *blk    = &clubar->blks[blktype][i];
        const GlyphInfo *gi = &drw.gis[blktype][i];

        if (blk->tags[Bg] != NULL)
            xrender_bg(blk, gi);
        if (blk->tags[Box] != NULL)
            xrender_box(blk, gi);
        xrender_string(blk, gi);
    }
}

void gui_destroy(void)
{
    while (drw.colorcache)
        CC_FREE(drw.colorcache);

    for (int i = 0; i < drw.nfonts; ++i)
        XftFontClose(dpy(), drw.fonts[i]);
    if (drw.fonts)
        free(drw.fonts);

    XftColorFree(dpy(), vis(), cmap(), &bar.foreground);
    XftColorFree(dpy(), vis(), cmap(), &bar.background);
    XftDrawDestroy(bar.canvas);
    XCloseDisplay(dpy());
}
