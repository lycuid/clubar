#include "blocks.h"
#include "utils.h"
#include <X11/Xft/Xft.h>

XftColor *get_cached_color(const char *);
int parse_box_string(const char *, char[32]);
void createdrw(const Config *);
void createbar(const BarConfig *);
void xsetup(const Config *);
void xsetatoms(const BarConfig *);
void xcleanup(void);
void xrenderblks(BlockType, const Block[MAX_BLKS], int);
__inline__ void prepare_stdinblks(const Block[MAX_BLKS], int);
__inline__ void prepare_customblks(const Block[MAX_BLKS], int);
void clearblks(BlockType, int);
void renderblks(BlockType, const Block[MAX_BLKS], int);
int onExpose(Block[2][MAX_BLKS], int[2]);
void onButtonPress(const XEvent *, Block[2][MAX_BLKS], int[2]);
int onPropertyNotify(const XEvent *, char *);
BarEvent handle_xevent(Block[2][MAX_BLKS], int[2], char[BLOCK_BUF_SIZE]);

#define xdb_setup xsetup
#define xdb_clearblks clearblks
#define xdb_renderblks renderblks
#define xdb_nextevent handle_xevent
#define xdb_cleanup xcleanup
