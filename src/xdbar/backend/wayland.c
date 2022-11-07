#include <xdbar/core.h>

#define UNIMPLEMENTED die("Wayland currently not supported.\n")

void xdb_setup(void) { UNIMPLEMENTED; }
void xdb_clear(__attribute__((unused)) BlockType _) { UNIMPLEMENTED; }
void xdb_render(__attribute__((unused)) BlockType _) { UNIMPLEMENTED; }
void xdb_toggle_visibility() { UNIMPLEMENTED; }
xdb_event_t xdb_nextevent(__attribute__((unused)) char _[]) { UNIMPLEMENTED; }
void xdb_cleanup(void) { UNIMPLEMENTED; }
