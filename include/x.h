#include "blocks.h"
#include "utils.h"

void xsetup(const Config *const);
void renderblks(BlockType, Block[2][MAX_BLKS], int[2]);
int handle_xevent(Block[2][MAX_BLKS], int[2], char *, int *);
void xcleanup();

#define Bar_Setup xsetup
#define Bar_RenderBlks renderblks
#define Bar_HandleEvent handle_xevent
#define Bar_Cleanup xcleanup
