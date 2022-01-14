#include "blocks.h"
#include "utils.h"

void xsetup(const Config *const);
void renderblks(BlockType, Block[2][MAX_BLKS], int[2]);
int handle_xevent(Block[2][MAX_BLKS], int[2], char *, int *);
void xcleanup();

#define b_Setup xsetup
#define b_RenderBlks renderblks
#define b_HandleEvent handle_xevent
#define b_Cleanup xcleanup
