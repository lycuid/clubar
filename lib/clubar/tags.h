#ifndef __CLUBAR__TAGS_H__
#define __CLUBAR__TAGS_H__

#include <stdint.h>

#define BLK_BUFFER_SIZE (1 << 10)

#define TagStart "<"
#define TagEnd   ">"

#define Enum(ident, ...) typedef enum { __VA_ARGS__, Null##ident } ident
Enum(TagName, Fn, Fg, Bg, Box, BtnL, BtnM, BtnR, ScrlU, ScrlD);
Enum(TagModifier, Shift, Ctrl, Super, Alt, Left, Right, Top, Bottom);
#undef Enum

typedef uint32_t TagModifierMask;

typedef struct Tag {
    TagModifierMask tmod_mask;
    char val[BLK_BUFFER_SIZE];
    struct Tag *previous;
} Tag;

static const TagModifierMask ValidTagModifiers[NullTagName] = {
    [Fn]    = 0,
    [Fg]    = 0,
    [Bg]    = 0,
    [Box]   = (1 << Left) | (1 << Right) | (1 << Top) | (1 << Bottom),
    [BtnL]  = (1 << Shift) | (1 << Ctrl) | (1 << Super) | (1 << Alt),
    [BtnM]  = (1 << Shift) | (1 << Ctrl) | (1 << Super) | (1 << Alt),
    [BtnR]  = (1 << Shift) | (1 << Ctrl) | (1 << Super) | (1 << Alt),
    [ScrlU] = (1 << Shift) | (1 << Ctrl) | (1 << Super) | (1 << Alt),
    [ScrlD] = (1 << Shift) | (1 << Ctrl) | (1 << Super) | (1 << Alt),
};

#define REPR(sym) [sym] = #sym
static const char *const TagNameRepr[NullTagName] = {
    REPR(Fn),   REPR(Fg),   REPR(Bg),    REPR(Box),   REPR(BtnL),
    REPR(BtnM), REPR(BtnR), REPR(ScrlU), REPR(ScrlD),
};
static const char *const TagModifierRepr[NullTagModifier] = {
    REPR(Shift), REPR(Ctrl),  REPR(Super), REPR(Alt),
    REPR(Left),  REPR(Right), REPR(Top),   REPR(Bottom),
};
#undef REPR

Tag *tag_create(Tag *, const char *, TagModifierMask);
Tag *tag_remove(Tag *);
Tag *tag_clone(const Tag *);

#endif
