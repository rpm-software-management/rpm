#include "system.h"
#include "rpmlib.h"
#include <rpmmacro.h>
#include <rpmurl.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lposix.h>
#include <lrexlib.h>

#include <unistd.h>

#define _RPMLUA_INTERNAL
#include "rpmlua.h"

static int luaopen_rpm(lua_State *L);

rpmlua rpmluaNew()
{
    rpmlua lua = (rpmlua) xcalloc(1, sizeof(struct rpmlua_s));
    lua_State *L = lua_open();
    const luaL_reg lualibs[] = {
	{"base", luaopen_base},
	{"table", luaopen_table},
	{"io", luaopen_io},
	{"string", luaopen_string},
	{"debug", luaopen_debug},
	{"loadlib", luaopen_loadlib},
	{"posix", luaopen_posix},
	{"rex", luaopen_rex},
	{"rpm", luaopen_rpm},
	{NULL, NULL},
    };
    const luaL_reg *lib = lualibs;
    for (; lib->name; lib++) {
	lib->func(L);
	lua_settop(L, 0);
    }
    lua_pushliteral(L, "LUA_PATH");
    lua_pushstring(L, RPMCONFIGDIR "/lua/?.lua");
    lua_rawset(L, LUA_GLOBALSINDEX);
    lua->L = L;
    return lua;
}

void *rpmluaFree(rpmlua lua)
{
    if (lua) {
	if (lua->L) lua_close(lua->L);
	free(lua);
    }
    return NULL;
}

void rpmluaSetTS(rpmlua lua, rpmts ts)
{
    lua->ts = ts;
}

rpmRC rpmluaCheckScript(rpmlua lua, const char *script, const char *name)
{
    lua_State *L = lua->L;
    rpmRC rc = RPMRC_OK;
    if (!name)
	name = "<lua>";
    if (luaL_loadbuffer(L, script, strlen(script), name) != 0) {
	rpmError(RPMERR_SCRIPT,
		_("invalid syntax in lua scriptlet: %s\n"),
		  lua_tostring(L, -1));
	rc = RPMRC_FAIL;
    }
    lua_pop(L, 1); /* Error or chunk. */
    return rc;
}

rpmRC rpmluaRunScript(rpmlua lua, Header h, const char *sln,
		      int progArgc, const char **progArgv,
		      const char *script, int arg1, int arg2)
{
    lua_State *L = lua->L;
    int rootFd = -1;
    const char *n, *v, *r;
    rpmRC rc = RPMRC_OK;
    int i;
    int xx;
    if (!L) {
	rpmError(RPMERR_SCRIPT,
		_("internal lua interpreter not available\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }
    xx = headerNVR(h, &n, &v, &r);
    if (luaL_loadbuffer(L, script, strlen(script), "<lua>") != 0) {
	rpmError(RPMERR_SCRIPT,
		_("%s(%s-%s-%s) lua scriptlet failed loading: %s\n"),
		 sln, n, v, r, lua_tostring(L, -1));
	lua_pop(L, 1);
	rc = RPMRC_FAIL;
	goto exit;
    }

    if (lua->ts && !rpmtsChrootDone(lua->ts)) {
	const char *rootDir = rpmtsRootDir(lua->ts);
	if (rootDir != NULL && !(rootDir[0] == '/' && rootDir[1] == '\0')) {
	    chdir("/");
	    rootFd = open(".", O_RDONLY, 0);
	    if (rootFd >= 0) {
		chroot(rootDir);
		rpmtsSetChrootDone(lua->ts, 1);
	    }
	}
    }

    /* Create arg variable */
    lua_pushliteral(L, "arg");
    lua_newtable(L);
    i = 0;
    if (progArgv) {
	while (i < progArgc && progArgv[i]) {
	    lua_pushstring(L, progArgv[i]);
	    lua_rawseti(L, -2, ++i);
	}
    }
    if (arg1 >= 0) {
	lua_pushnumber(L, arg1);
	lua_rawseti(L, -2, ++i);
    }
    if (arg2 >= 0) {
	lua_pushnumber(L, arg2);
	lua_rawseti(L, -2, ++i);
    }
    lua_rawset(L, LUA_GLOBALSINDEX);

    if (lua_pcall(L, 0, 0, 0) != 0) {
	rpmError(RPMERR_SCRIPT, _("%s(%s-%s-%s) lua scriptlet failed: %s\n"),
		 sln, n, v, r, lua_tostring(L, -1));
	lua_pop(L, 1);
	rc = RPMRC_FAIL;
    }

    /* Remove 'arg' global. */
    lua_pushliteral(L, "arg");
    lua_pushnil(L);
    lua_rawset(L, LUA_GLOBALSINDEX);

    if (rootFd >= 0) {
	fchdir(rootFd);
	close(rootFd);
	chroot(".");
	rpmtsSetChrootDone(lua->ts, 0);
    }

exit:
    return rc;
}

/* From lua.c */
static int rpmluaReadline(lua_State *l, const char *prompt) {
   static char buffer[1024];
   if (prompt) {
      fputs(prompt, stdout);
      fflush(stdout);
   }
   if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      return 0;  /* read fails */
   } else {
      lua_pushstring(l, buffer);
      return 1;
   }
}

/* Based on lua.c */
static void _rpmluaInteractive(lua_State *L)
{
   fputs("\n", stdout);
   printf("RPM Interactive %s Interpreter\n", LUA_VERSION);
   for (;;) {
      if (rpmluaReadline(L, "> ") == 0)
	 break;
      if (lua_tostring(L, -1)[0] == '=') {
	 lua_pushfstring(L, "print(%s)", lua_tostring(L, -1)+1);
	 lua_remove(L, -2);
      }
      int rc = 0;
      for (;;) {
	 rc = luaL_loadbuffer(L, lua_tostring(L, -1),
			      lua_strlen(L, -1), "<lua>");
	 if (rc == LUA_ERRSYNTAX &&
	     strstr(lua_tostring(L, -1), "near `<eof>'") != NULL) {
	    if (rpmluaReadline(L, ">> ") == 0)
	       break;
	    lua_remove(L, -2); // Remove error
	    lua_concat(L, 2);
	    continue;
	 }
	 break;
      }
      if (rc == 0)
	 rc = lua_pcall(L, 0, 0, 0);
      if (rc != 0) {
	 fprintf(stderr, "%s\n", lua_tostring(L, -1));
	 lua_pop(L, 1);
      }
      lua_pop(L, 1); // Remove line
   }
   fputs("\n", stdout);
}

void rpmluaInteractive(rpmlua lua)
{
    _rpmluaInteractive(lua->L);
}

/* ------------------------------------------------------------------ */
/* Lua API */

static int rpm_expand(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    lua_pushstring(L, rpmExpand(str, NULL));
    return 1;
}

static int rpm_define(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    rpmDefineMacro(NULL, str, 0);
    return 0;
}

static int rpm_interactive(lua_State *L)
{
    _rpmluaInteractive(L);
    return 0;
}

static const luaL_reg rpmlib[] = {
    {"expand", rpm_expand},
    {"define", rpm_define},
    {"interactive", rpm_interactive},
    {NULL, NULL}
};

static int luaopen_rpm(lua_State *L)
{
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    luaL_openlib(L, "rpm", rpmlib, 0);
    return 0;
}

/* vim:sts=4:sw=4
*/
