#ifndef __CORE__BLOCKS_H__
#define __CORE__BLOCKS_H__

#include <stdint.h>

#define TAG_START "<"
#define TAG_END   ">"

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

typedef uint32_t TagModifierMask;
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

static const TagModifier ValidTagModifiers[NullKey][NullModifier] = {
    [Fn]    = {NullModifier},
    [Fg]    = {NullModifier},
    [Bg]    = {NullModifier},
    [Box]   = {Left, Right, Top, Bottom, NullModifier},
    [BtnL]  = {Shift, Ctrl, Super, Alt, NullModifier},
    [BtnM]  = {Shift, Ctrl, Super, Alt, NullModifier},
    [BtnR]  = {Shift, Ctrl, Super, Alt, NullModifier},
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

int blks_create(const char *, Block *);
void blks_free(Block *, int);

#endif
