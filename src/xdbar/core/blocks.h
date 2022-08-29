/*
 * <Bg=#efefef>
 *    <Fg=#090909>
 *        <Box:Top|Bottom=#00ff00:2> One ring </Box> to rule them all!.
 *    </Fg>
 * </Bg>
 *
 * ----------------------------------------------------------------------------
 *
 * blocks = {
 *    {
 *       val:  " One ring ",
 *       tags: {
 *          [Bg]  => { val: "#efefef", tmod_mask: 0x0, previous: NULL },
 *          [Fg]  => { val: "#090909", tmod_mask: 0x0, previous: NULL },
 *          [Box] => {
 *              val: "#00ff00:2",
 *              tmod_mask: (1 << Top) | (1 << Bottom),
 *              previous: NULL
 *          },
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
 */
#ifndef __CORE__BLOCKS_H__
#define __CORE__BLOCKS_H__

#include <stdint.h>

#define BLK_BUFFER_SIZE /* minimum can be 'unsigned char' size. */ (1 << 10)

#define TagStart "<"
#define TagEnd   ">"

#define Enum(ident, ...) typedef enum { __VA_ARGS__, Null##ident } ident
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

typedef struct Tag {
  TagModifierMask tmod_mask;
  char val[BLK_BUFFER_SIZE];
  struct Tag *previous;
} Tag;

typedef struct {
  int ntext;
  char text[BLK_BUFFER_SIZE];
  Tag *tags[NullTagName];
} Block;

int blks_create(const char *, Block *);
void blks_free(Block *, int);

#endif
