#ifndef __WITH_X11__GUI_H__
#define __WITH_X11__GUI_H__

#include <X11/Xutil.h>
#include <clubar.h>

extern CluBar *clubar;
extern Display *_dpy;

#define dpy()  _dpy
#define root() (DefaultRootWindow(dpy()))
#define scr()  (DefaultScreen(dpy()))
#define vis()  (DefaultVisual(dpy(), scr()))
#define cmap() (DefaultColormap(dpy(), scr()))

void onExpose(void);
void onMapNotify(const XEvent *, char *);
bool onPropertyNotify(const XEvent *, char *);
void onButtonPress(const XEvent *);

void gui_init(void);
void gui_load(void);
void gui_toggle(void);
void gui_clear(BlockType);
void gui_draw(BlockType);
void gui_destroy(void);

#endif
