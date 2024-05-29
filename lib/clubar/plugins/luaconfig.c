/* Enable Plugin with: `make PLUGINS=luaconfig`
 * This plugin enables runtime configuration support using lua source code.
 * check 'examples' directory for sample configs.
 */
#include "luaconfig.h"
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>

#define GetField(L, index, field, result, lua_f)                               \
    {                                                                          \
        lua_getfield(L, (index), field);                                       \
        if (!lua_isnil(L, -1))                                                 \
            *(result) = lua_f(L, -1);                                          \
        lua_pop(L, 1);                                                         \
    }

static inline int GetTableAsIntArray(lua_State *L, int index, const char *table,
                                     const char **repr, int *ret, int size)
{
    memset(ret, 0, size);
    lua_getfield(L, index, table);
    int found = !lua_isnil(L, index + 1);
    if (found)
        for (int i = 0; i < size; ++i)
            GetField(L, index + 1, repr[i], &ret[i], lua_tointeger);
    lua_pop(L, 1);
    return found;
}

static inline void GetString(lua_State *L, int index, const char *f, char *res)
{
    lua_getfield(L, index, f);
    if (!lua_isnil(L, -1))
        strcpy(res, lua_tostring(L, -1));
    lua_pop(L, 1);
}

static inline void load_fonts(lua_State *L, Config *c)
{
    c->nfonts = 0;
    lua_getfield(L, 1, "fonts");
    if (lua_isnil(L, 2))
        return;

    lua_pushnil(L);
    for (int i = 0; i < c->nfonts; ++i)
        free(c->fonts[i]);
    free(c->fonts);
    c->fonts = NULL, c->nfonts = 0;
    while (lua_next(L, -2)) {
        lua_pushvalue(L, -2);
        c->fonts = realloc(c->fonts, (c->nfonts + 1) * sizeof(char *));
        c->fonts[c->nfonts++] = (char *)lua_tostring(L, -2);
        lua_pop(L, 2);
    }
}

void luaconfig_merge(const char *luafile, Config *config)
{
    if (!luafile || !strlen(luafile))
        return;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    if (luaL_dofile(L, luafile))
        die("Unable to load lua file: '%s'.\n", luafile);

    lua_getglobal(L, "Config");
    if (lua_isnil(L, 1))
        die("Invalid 'Config' table.\n");

    int ret[4];
    const char *g_members[] = {"x", "y", "w", "h"};
    const char *members[]   = {"left", "right", "top", "bottom"};

    if (GetTableAsIntArray(L, 1, "geometry", g_members, ret, 4))
        memcpy(&config->geometry, ret, sizeof(ret));

    if (GetTableAsIntArray(L, 1, "padding", members, ret, 4))
        memcpy(&config->padding, ret, sizeof(ret));

    if (GetTableAsIntArray(L, 1, "margin", members, ret, 4))
        memcpy(&config->margin, ret, sizeof(ret));

    GetField(L, 1, "topbar", &config->topbar, lua_toboolean);
    GetString(L, 1, "foreground", (char *)config->foreground);
    GetString(L, 1, "background", (char *)config->background);

    load_fonts(L, config);
}
