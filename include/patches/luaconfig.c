/* Enable Patch with: `make PATCHES=luaconfig`
 * This patch enables runtime configuration support using lua source code.
 * check 'examples' directory for sample configs.
 */
#include "luaconfig.h"
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>

#define GetField(L, index, field, result, lua_f)                               \
  {                                                                            \
    lua_getfield(L, (index), field);                                           \
    if (!lua_isnil(L, -1))                                                     \
      *result = lua_f(L, -1);                                                  \
    lua_pop(L, 1);                                                             \
  }

inline int GetTableAsIntArray(lua_State *L, int index, const char *table,
                              const char **repr, int *result, int size) {
  memset(result, 0, size * sizeof(int));
  lua_getfield(L, (index), table);
  int found = !lua_isnil(L, index + 1);
  if (found)
    for (int i = 0; i < size; ++i)
      GetField(L, index + 1, repr[i], &result[i], lua_tointeger);
  lua_pop(L, 1);
  return found;
}

inline void GetString(lua_State *L, int index, const char *f, char *res) {
  lua_getfield(L, index, f);
  if (!lua_isnil(L, -1))
    strcpy(res, lua_tostring(L, -1));
  lua_pop(L, 1);
}

void load_bar_config(lua_State *L, BarConfig *barConfig) {
  lua_getfield(L, 1, "barConfig");
  int barindex = 2;
  if (lua_isnil(L, barindex))
    return;

  GetString(L, barindex, "foreground", (char *)barConfig->foreground);
  GetString(L, barindex, "background", (char *)barConfig->background);
  GetField(L, barindex, "topbar", &barConfig->topbar, lua_toboolean);

  int size = 4;
  int ints[size];
  const char *g_members[4] = {"x", "y", "w", "h"};
  const char *members[4] = {"left", "right", "top", "bottom"};

  // @TODO: Make sure there are no struct padding bugs during all
  // the memcpy int_array -> struct.
  // currently, don't know how struct padding works...but this code works, gl.
  if (GetTableAsIntArray(L, barindex, "geometry", g_members, ints, size))
    memcpy(&barConfig->geometry, ints, size * sizeof(int));

  if (GetTableAsIntArray(L, barindex, "padding", members, ints, size))
    memcpy(&barConfig->padding, ints, size * sizeof(int));

  if (GetTableAsIntArray(L, barindex, "margin", members, ints, size))
    memcpy(&barConfig->margin, ints, size * sizeof(int));
}

void load_font_config(lua_State *L, Config *luaConfig) {
  luaConfig->nfonts = 0;
  lua_getfield(L, 1, "fonts");

  if (lua_isnil(L, 2))
    return;

  lua_pushnil(L);
  luaConfig->fonts = NULL;
  while (lua_next(L, -2)) {
    lua_pushvalue(L, -2);
    luaConfig->fonts =
        realloc(luaConfig->fonts, (luaConfig->nfonts + 1) * sizeof(char *));
    luaConfig->fonts[luaConfig->nfonts++] = (char *)lua_tostring(L, -2);
    lua_pop(L, 2);
  }
}

void merge_lua_config(const char *luafile, Config *luaConfig) {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  if (luaL_dofile(L, luafile))
    die("Unable to load lua file: '%s'.\n", luafile);

  lua_getglobal(L, "Config");
  if (lua_isnil(L, 1))
    die("Invalid 'Config' table.\n");
  load_bar_config(L, &luaConfig->barConfig);
  load_font_config(L, luaConfig);
}
