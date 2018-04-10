#include "system.h"

#ifdef WITH_LUA
#include <lua.h>
#include <lauxlib.h>
#include <rpm/rpmlib.h>

#define _RPMLUA_INTERNAL
#include "rpmio/rpmlua.h"
#include "lib/rpmliblua.h"

static int rpm_vercmp(lua_State *L)
{
    const char *v1, *v2;
    int rc = 0;

    v1 = luaL_checkstring(L, 1);
    v2 = luaL_checkstring(L, 2);
    if (v1 && v2) {
	lua_pushinteger(L, rpmvercmp(v1, v2));
	rc = 1;
    }
    return rc;
}

static const luaL_Reg luarpmlib_f[] = {
    {"vercmp", rpm_vercmp},
    {NULL, NULL}
};

void rpmLuaInit(void)
{
    rpmlua lua = rpmluaGetGlobalState();
#if (LUA_VERSION_NUM < 502) || defined(LUA_COMPAT_MODULE)
    lua_getfield(lua->L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(lua->L, -1, "rpm");
    luaL_register(lua->L, 0, luarpmlib_f);
#else
    lua_getfield(lua->L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getfield(lua->L, -1, "rpm");
    luaL_setfuncs(lua->L, luarpmlib_f, 0);
#endif
    lua_pop(lua->L, 2);
    return;
}

void rpmLuaFree(void)
{
    rpmlua lua = rpmluaGetGlobalState();
    rpmluaFree(lua);
}

#endif /* WITH_LUA */
