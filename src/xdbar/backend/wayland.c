#include <xdbar/core.h>

void xdb_setup(void) { die("Wayland currently not supported.\n"); }

void xdb_clear(BlockType blktype)
{
  (void)blktype;
  die("Wayland currently not supported.\n");
}

void xdb_render(BlockType blktype)
{
  (void)blktype;
  die("Wayland currently not supported.\n");
}

BarEvent xdb_nextevent(char name[BLK_BUFFER_SIZE])
{
  (void)name;
  die("Wayland currently not supported.\n");
}

void xdb_cleanup(void) { die("Wayland currently not supported.\n"); }
