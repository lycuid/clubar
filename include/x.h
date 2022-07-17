#include "blocks.h"
#include "utils.h"
#include <X11/Xft/Xft.h>
#include <stdbool.h>

void xsetup(const Config *);
void xcleanup(void);
void render(BlockType, const Block[MAX_BLKS], int);
BarEvent handle_xevent(Block[2][MAX_BLKS], int[2], char[BLOCK_BUF_SIZE]);
void clear(BlockType, int);

#define xdb_setup xsetup
#define xdb_clear clear
#define xdb_render render
#define xdb_nextevent handle_xevent
#define xdb_cleanup xcleanup
