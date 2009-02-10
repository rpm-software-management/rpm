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

static const luaL_reg luarpmlib_f[] = {
    {"vercmp", rpm_vercmp},
    {NULL, NULL}
};

void rpmLuaInit(void)
{
    rpmlua lua = rpmluaGetGlobalState();
    lua_pushvalue(lua->L, LUA_GLOBALSINDEX); 
    luaL_register(lua->L, "rpm", luarpmlib_f);
    return;
}

void rpmLuaFree(void)
{
    rpmlua lua = rpmluaGetGlobalState();
    rpmluaFree(lua);
}

#endif /* WITH_LUA */
