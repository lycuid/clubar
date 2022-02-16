#ifndef __BLOCKS_H__
#define __BLOCKS_H__

#define TAG_START "<"
#define TAG_END ">"

typedef enum {
  Fn,
  Fg,
  Bg,
  Box,
  BtnL,
  BtnM,
  BtnR,
  ScrlU,
  ScrlD,
  NullKey
} TagKey;
static const char *const TagKeyRepr[NullKey] = {
    [Fn] = "Fn",     [Fg] = "Fg",       [Bg] = "Bg",
    [Box] = "Box",   [BtnL] = "BtnL",   [BtnM] = "BtnM",
    [BtnR] = "BtnR", [ScrlU] = "ScrlU", [ScrlD] = "ScrlD"};

typedef unsigned int TagModifierMask;
typedef enum {
  Shift,
  Ctrl,
  Super,
  Alt,
  Left,
  Right,
  Top,
  Bottom,
  NullModifier
} TagModifier;
static const char *const TagModifierRepr[NullModifier] = {
    [Shift] = "Shift", [Ctrl] = "Ctrl",    [Super] = "Super",
    [Alt] = "Alt",     [Left] = "Left",    [Right] = "Right",
    [Top] = "Top",     [Bottom] = "Bottom"};

static const TagModifier ValidTagModifiers[NullKey][NullModifier] = {
    [Fn] = {NullModifier},
    [Fg] = {NullModifier},
    [Bg] = {NullModifier},
    [Box] = {Left, Right, Top, Bottom, NullModifier},
    [BtnL] = {Shift, Ctrl, Super, Alt, NullModifier},
    [BtnM] = {Shift, Ctrl, Super, Alt, NullModifier},
    [BtnR] = {Shift, Ctrl, Super, Alt, NullModifier},
    [ScrlU] = {Shift, Ctrl, Super, Alt, NullModifier},
    [ScrlD] = {Shift, Ctrl, Super, Alt, NullModifier}};

typedef struct _Tag {
  TagModifierMask tmod_mask;
  char val[64];
  struct _Tag *previous;
} Tag;

typedef struct {
  int ntext;
  char text[64];
  Tag *tags[NullKey];
} Block;

Tag *mkcopy(Tag *);
Tag *push(const char *, TagModifierMask, Tag *);
Tag *pop(Tag *);
int parsetag(const char *, TagKey *, TagModifierMask *, char *, int *);
void createblk(Block *, Tag *[NullKey], const char *, int);

// public.
void freeblks(Block *, int);
int createblks(const char *, Block *);

#endif
