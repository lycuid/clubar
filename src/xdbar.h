#include <xdbar/core.h>
#include <xdbar/core/blocks.h>

// These are supposed to be implemented by whatever 'frontend' thats being used.
void xdb_setup(const Config *);
void xdb_clear(BlockType);
void xdb_render(BlockType);
BarEvent xdb_nextevent(char[BLOCK_BUF_SIZE]);
void xdb_cleanup(void);
