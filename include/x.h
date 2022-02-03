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
#if __has_attribute(always_inline)
__attribute__((always_inline)) void prepare_stdinblks(const Block[MAX_BLKS],
                                                      int);
__attribute__((always_inline)) void prepare_customblks(const Block[MAX_BLKS],
                                                       int);
#else
inline void prepare_stdinblks(const Block[MAX_BLKS], int);
inline void prepare_customblks(const Block[MAX_BLKS], int);
#endif
void clearblks(BlockType, int);
void renderblks(BlockType, const Block[MAX_BLKS], int);
void handle_xbuttonpress(XButtonEvent *, Block[2][MAX_BLKS], int[2]);
BarEvent handle_xevent(Block[2][MAX_BLKS], int[2], char[BLOCK_BUF_SIZE]);

#define XDBSetup xsetup
#define XDBClearBlks clearblks
#define XDBRenderBlks renderblks
#define XDBHandleEvent handle_xevent
#define XDBCleanup xcleanup
