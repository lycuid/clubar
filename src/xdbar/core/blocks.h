#ifndef __CORE__BLOCKS_H__
#define __CORE__BLOCKS_H__

#include <stdint.h>

/* clang-format off
 *
 * <Bg=#efefef>
 *    <Fg=#090909>
 *        <Box:Top|Bottom=#00ff00:1> One ring </Box> to rule them all!.
 *    </Fg>
 * </Bg>
 *
 * blocks = {
 *    {
 *       val:  " One ring ",
 *       tags: {
 *          [Bg]  => { val: "#efefef", tmod_mask: 0x0, previous: NULL },
 *          [Fg]  => { val: "#090909", tmod_mask: 0x0, previous: NULL },
 *          [Box] => { val: "#00ff00", tmod_mask: (1 << Top) | (1 << Bottom), previous: NULL },
 *       },
 *    },
 *    {
 *       val:  " to rule them all!.",
 *       tags: {
 *          [Bg] => { val: "#efefef", previous: NULL },
 *          [Fg] => { val: "#090909", previous: NULL },
 *       },
 *    },
 * };
 *
 * clang-format on */

#define TAG_START "<"
#define TAG_END   ">"

#define Enum(indent, ...) typedef enum { __VA_ARGS__, Null##indent } indent
Enum(TagName, Fn, Fg, Bg, Box, BtnL, BtnM, BtnR, ScrlU, ScrlD);
Enum(TagModifier, Shift, Ctrl, Super, Alt, Left, Right, Top, Bottom);
#undef Enum

typedef uint32_t TagModifierMask;

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

typedef struct _Tag {
  TagModifierMask tmod_mask;
  char val[64];
  struct _Tag *previous;
} Tag;

typedef struct {
  int ntext;
  char text[64];
  Tag *tags[NullTagName];
} Block;

int blks_create(const char *, Block *);
void blks_free(Block *, int);

#endif
