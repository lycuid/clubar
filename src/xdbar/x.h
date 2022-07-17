#include <X11/Xft/Xft.h>
#include <stdbool.h>
#include <xdbar/core.h>
#include <xdbar/core/blocks.h>

void xsetup(const Config *);
void xcleanup(void);
void xrender(BlockType, const Block[MAX_BLKS], int);
BarEvent xnextevent(Block[2][MAX_BLKS], int[2], char[BLOCK_BUF_SIZE]);
void xclear(BlockType, int);

#define xdb_setup     xsetup
#define xdb_clear     xclear
#define xdb_render    xrender
#define xdb_nextevent xnextevent
#define xdb_cleanup   xcleanup
