#include <xdbar/core.h>

#define UNIMPLEMENTED() die("Wayland currently not supported.\n")

void xdb_setup(void) { UNIMPLEMENTED(); }

void xdb_clear(BlockType blktype)
{
  (void)blktype;
  UNIMPLEMENTED();
}

void xdb_render(BlockType blktype)
{
  (void)blktype;
  UNIMPLEMENTED();
}

void xdb_toggle() { UNIMPLEMENTED(); }

XDBEvent xdb_nextevent(char name[BLK_BUFFER_SIZE])
{
  (void)name;
  UNIMPLEMENTED();
}

void xdb_cleanup(void) { UNIMPLEMENTED(); }
