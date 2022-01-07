#ifndef __BLOCKS_H__
#define __BLOCKS_H__

typedef enum { Fn, Fg, Bg, Box, BtnL, BtnM, BtnR, ScrlU, ScrlD, NullTag } Tag;
static const char *const TagRepr[NullTag] = {
    [Fn] = "Fn",     [Fg] = "Fg",       [Bg] = "Bg",
    [Box] = "Box",   [BtnL] = "BtnL",   [BtnM] = "BtnM",
    [BtnR] = "BtnR", [ScrlU] = "ScrlU", [ScrlD] = "ScrlD"};

typedef enum {
  Shift,
  Ctrl,
  Super,
  Alt,
  Left,
  Right,
  Top,
  Bottom,
  NullExt
} Extension;
static const char *const ExtRepr[NullExt] = {
    [Shift] = "Shift", [Ctrl] = "Ctrl",    [Super] = "Super",
    [Alt] = "Alt",     [Left] = "Left",    [Right] = "Right",
    [Top] = "Top",     [Bottom] = "Bottom"};

static const Extension AllowedTagExtensions[NullTag][NullExt] = {
    [Fn] = {NullExt},
    [Fg] = {NullExt},
    [Bg] = {NullExt},
    [Box] = {Left, Right, Top, Bottom, NullExt},
    [BtnL] = {Shift, Ctrl, Super, Alt, NullExt},
    [BtnM] = {Shift, Ctrl, Super, Alt, NullExt},
    [BtnR] = {Shift, Ctrl, Super, Alt, NullExt},
    [ScrlU] = {Shift, Ctrl, Super, Alt, NullExt},
    [ScrlD] = {Shift, Ctrl, Super, Alt, NullExt}};

typedef struct _Attribute {
  char val[64];
  Extension extension;
  struct _Attribute *previous;
} Attribute;

typedef struct {
  char text[64];
  Attribute *attrs[NullTag];
} Block;

Attribute *mkcopy(Attribute *);
Attribute *push(const char *, Extension, Attribute *);
Attribute *pop(Attribute *);
int parsetag(const char *, Tag *, Extension *, char *, int *);
void createblk(Block *, Attribute *[NullTag], const char *, int);

// public.
void freeblks(Block *, int);
int createblks(const char *, Block *);

#endif
