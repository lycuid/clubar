#include "blocks.h"
#include "utils.h"

void xsetup(const Config *);
void clearblks(BlockType, int);
void renderblks(BlockType, const Block[MAX_BLKS], int);
BarEvent handle_xevent(const Block[2][MAX_BLKS], int[2], char[BLOCK_BUF_SIZE]);
void xcleanup(void);

#define Bar_Setup xsetup
#define Bar_ClearBlks clearblks
#define Bar_RenderBlks renderblks
#define Bar_HandleEvent handle_xevent
#define Bar_Cleanup xcleanup
