#include "system.h"

#include <lua.h>
#include <lauxlib.h>

#include <rpm/rpmlib.h>

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
    rpmluaRegister(lua, luarpmlib_f, "rpm");
    return;
}

void rpmLuaFree(void)
{
    rpmlua lua = rpmluaGetGlobalState();
    rpmluaFree(lua);
}
