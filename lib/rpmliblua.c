#include "system.h"

#ifdef WITH_LUA
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <lua.h>
#include <lauxlib.h>
#include <rpm/rpmlib.h>

#define _RPMLUA_INTERNAL
#include "rpmio/rpmlua.h"
#include "lib/rpmliblua.h"
#include "rpmio/rpmio_internal.h"

static int pusherror(lua_State *L, int code, const char *info)
{
    lua_pushnil(L);
    if (info == NULL)
	lua_pushstring(L, strerror(code));
    else
	lua_pushfstring(L, "%s: %s", info, strerror(code));
    lua_pushnumber(L, code);
    return 3;
}

static int pushresult(lua_State *L, int result, const char *info)
{
    if (result == 0) {
	lua_pushnumber(L, result);
	return 1;
    }

    return pusherror(L, result, info);
}

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

static int rpm_execute(lua_State *L)
{
    const char *file = luaL_checkstring(L, 1);
    int i, n = lua_gettop(L);
    int status;
    pid_t pid;

    char **argv = malloc((n + 1) * sizeof(char *));
    if (argv == NULL)
	return luaL_error(L, "not enough memory");
    argv[0] = (char *)file;
    for (i = 1; i < n; i++)
	argv[i] = (char *)luaL_checkstring(L, i + 1);
    argv[i] = NULL;
    rpmSetCloseOnExec();
    status = posix_spawnp(&pid, file, NULL, NULL, argv, environ);
    free(argv);
    if (status != 0)
	return pusherror(L, status, "posix_spawnp");
    if (waitpid(pid, &status, 0) == -1)
	return pusherror(L, 0, "waitpid");
    else
	return pushresult(L, status, NULL);
}

static const luaL_Reg luarpmlib_f[] = {
    {"vercmp", rpm_vercmp},
    {"execute", rpm_execute},
    {NULL, NULL}
};

#ifndef lua_pushglobaltable
#define lua_pushglobaltable(L) lua_pushvalue(L, LUA_GLOBALSINDEX)
#endif

void rpmLuaInit(void)
{
    rpmlua lua = rpmluaGetGlobalState();
    lua_pushglobaltable(lua->L);
#if (LUA_VERSION_NUM < 502) || defined(LUA_COMPAT_MODULE)
    luaL_register(lua->L, "rpm", luarpmlib_f);
#else
    luaL_pushmodule(lua->L, "rpm", 1);
    lua_insert(lua->L, -1);
    luaL_setfuncs(lua->L, luarpmlib_f, 0);
#endif
    return;
}

void rpmLuaFree(void)
{
    rpmlua lua = rpmluaGetGlobalState();
    rpmluaFree(lua);
}

#endif /* WITH_LUA */
