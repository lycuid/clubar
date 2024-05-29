/*
 * <Bg=#efefef>
 *    <Fg=#090909>
 *        <Box:Top|Bottom=#00ff00:2>
 *            <Fg=white> One ring </Fg>
 *        </Box>
 *        to rule them all!.
 *    </Fg>
 * </Bg>
 *
 * ----------------------------------------------------------------------------
 *
 * blocks = {
 *    {
 *       text: " One ring ",
 *       tags: {
 *          [Bg]  => { val: "#efefef", tmod_mask: 0x0, previous: NULL },
 *          [Fg]  => {
 *              val:        "white",
 *              tmod_mask:  0x0,
 *              previous:   {
 *                  val: "#090909",
 *                  tmod_mask: 0x0,
 *                  previous: NULL
 *              },
 *          },
 *          [Box] => {
 *              val:        "#00ff00:2",
 *              tmod_mask:  (1 << Top) | (1 << Bottom),
 *              previous:   NULL,
 *          },
 *       },
 *    },
 *    {
 *       text: " to rule them all!.",
 *       tags: {
 *          [Bg] => { val: "#efefef", tmod_mask = 0x0, previous: NULL },
 *          [Fg] => { val: "#090909", tmod_mask = 0x0, previous: NULL },
 *       },
 *    },
 * };
 */

#ifndef __CLUBAR__BLOCKS_H__
#define __CLUBAR__BLOCKS_H__

#include <clubar/tags.h>

typedef struct {
    char text[BLK_BUFFER_SIZE];
    Tag *tags[NullTagName];
} Block;

int blks_create(Block *, const char *);
void blks_free(Block *, int);

#endif
