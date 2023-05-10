#ifndef __CORE__BLKS__ALLOCATOR_H__
#define __CORE__BLKS__ALLOCATOR_H__

#include <stdint.h>
#include <stdlib.h>

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

static const TagModifier ValidTagModifiers[NullTagName][NullTagModifier] = {
    [Fn]    = {NullTagModifier},
    [Fg]    = {NullTagModifier},
    [Bg]    = {NullTagModifier},
    [Box]   = {Left, Right, Top, Bottom, NullTagModifier},
    [BtnL]  = {Shift, Ctrl, Super, Alt, NullTagModifier},
    [BtnM]  = {Shift, Ctrl, Super, Alt, NullTagModifier},
    [BtnR]  = {Shift, Ctrl, Super, Alt, NullTagModifier},
    [ScrlU] = {Shift, Ctrl, Super, Alt, NullTagModifier},
    [ScrlD] = {Shift, Ctrl, Super, Alt, NullTagModifier}};

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
