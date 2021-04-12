#include "system.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lposix.h>
#include <lrexlib.h>

#ifndef LUA_LOADED_TABLE
/* feature introduced in Lua 5.3.4 */
#define LUA_LOADED_TABLE "_LOADED"
#endif

#include <unistd.h>
#include <assert.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>

#include <rpm/rpmio.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmurl.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmbase64.h>
#include <rpm/rpmver.h>
#include "rpmio/rpmhook.h"

#include "rpmio/rpmlua.h"
#include "rpmio/rpmio_internal.h"
#include "rpmio/rpmmacro_internal.h"

#include "debug.h"

int _rpmlua_have_forked = 0;

typedef struct rpmluapb_s * rpmluapb;

struct rpmlua_s {
    lua_State *L;
    size_t pushsize;
    rpmluapb printbuf;
};

#define INITSTATE(_lua, lua) \
    rpmlua lua = _lua ? _lua : \
	    (globalLuaState ? globalLuaState : \
			\
			(globalLuaState = rpmluaNew()) \
			\
	    )

struct rpmluapb_s {
    size_t alloced;
    size_t used;
    char *buf;
    rpmluapb next;
};

static rpmlua globalLuaState = NULL;

static char *(*nextFileFunc)(void *) = NULL;
static void *nextFileFuncParam = NULL;

static int luaopen_rpm(lua_State *L);
static int rpm_print(lua_State *L);
static int rpm_exit(lua_State *L);
static int rpm_redirect2null(lua_State *L);

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

rpmlua rpmluaGetGlobalState(void)
{
    INITSTATE(NULL, lua);
    return lua;
}

static const luaL_Reg os_overrides[] =
{
    {"exit",    rpm_exit},
    {NULL,      NULL}
};

static const luaL_Reg posix_overrides[] =
{
    {"redirect2null",	rpm_redirect2null},
    {NULL,      NULL}
};

rpmlua rpmluaNew()
{
    rpmlua lua = NULL;
    struct stat st;
    const luaL_Reg *lib;
    char *initlua = NULL;

    static const luaL_Reg extlibs[] = {
	{"posix", luaopen_posix},
	{"rex", luaopen_rex},
	{"rpm", luaopen_rpm},
	{NULL, NULL},
    };

    lua_State *L = luaL_newstate();
    if (!L) return NULL;

    luaL_openlibs(L);

    lua = (rpmlua) xcalloc(1, sizeof(*lua));
    lua->L = L;

    for (lib = extlibs; lib->name; lib++) {
	luaL_requiref(L, lib->name, lib->func, 1);
    }
    lua_pushcfunction(L, rpm_print);
    lua_setglobal(L, "print");

    lua_getglobal(L, "os");
    luaL_setfuncs(L, os_overrides, 0);
    lua_getglobal(L, "posix");
    luaL_setfuncs(L, posix_overrides, 0);

    lua_getglobal(L, "package");
    lua_pushfstring(L, "%s/%s", rpmConfigDir(), "/lua/?.lua");
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);

    rpmluaSetData(lua, "lua", lua);

    initlua = rpmGenPath(rpmConfigDir(), "init.lua", NULL);
    if (stat(initlua, &st) != -1)
	(void)rpmluaRunScriptFile(lua, initlua);
    free(initlua);
    return lua;
}

rpmlua rpmluaFree(rpmlua lua)
{
    if (lua) {
	if (lua->L) lua_close(lua->L);
	free(lua->printbuf);
	free(lua);
	if (lua == globalLuaState) globalLuaState = NULL;
    }
    return NULL;
}

void * rpmluaGetLua(rpmlua _lua)
{
    INITSTATE(_lua, lua);
    return lua->L;
}

void rpmluaRegister(rpmlua lua, const void *regfuncs, const char *lib)
{
    const luaL_Reg *funcs = regfuncs;
    lua_getfield(lua->L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getfield(lua->L, -1, lib);
    luaL_setfuncs(lua->L, funcs, 0);
    lua_pop(lua->L, 2);
}

void rpmluaSetData(rpmlua _lua, const char *key, const void *data)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    lua_pushliteral(L, "rpm_");
    lua_pushstring(L, key);
    lua_concat(L, 2);
    if (data == NULL)
	lua_pushnil(L);
    else
	lua_pushlightuserdata(L, (void *)data);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

static void *getdata(lua_State *L, const char *key)
{
    void *ret = NULL;
    lua_pushliteral(L, "rpm_");
    lua_pushstring(L, key);
    lua_concat(L, 2);
    lua_rawget(L, LUA_REGISTRYINDEX);
    if (lua_islightuserdata(L, -1))
	ret = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return ret;
}

void *rpmluaGetData(rpmlua _lua, const char *key)
{
    INITSTATE(_lua, lua);
    return getdata(lua->L, key);
}

void rpmluaPushPrintBuffer(rpmlua _lua)
{
    INITSTATE(_lua, lua);
    rpmluapb prbuf = xcalloc(1, sizeof(*prbuf));
    prbuf->buf = NULL;
    prbuf->alloced = 0;
    prbuf->used = 0;
    prbuf->next = lua->printbuf;

    lua->printbuf = prbuf;
}

char *rpmluaPopPrintBuffer(rpmlua _lua)
{
    INITSTATE(_lua, lua);
    rpmluapb prbuf = lua->printbuf;
    char *ret = NULL;

    if (prbuf) {
	ret = prbuf->buf;
	lua->printbuf = prbuf->next;
	free(prbuf);
    }

    return ret;
}

void rpmluaSetNextFileFunc(char *(*func)(void *), void *funcParam)
{
    nextFileFunc = func;
    nextFileFuncParam = funcParam;
}

int rpmluaCheckScript(rpmlua _lua, const char *script, const char *name)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    int ret = 0;
    if (name == NULL)
	name = "<lua>";
    if (luaL_loadbuffer(L, script, strlen(script), name) != 0) {
	rpmlog(RPMLOG_ERR,
		_("invalid syntax in lua scriptlet: %s\n"),
		  lua_tostring(L, -1));
	ret = -1;
    }
    lua_pop(L, 1); /* Error or chunk. */
    return ret;
}

static int luaopt(int c, const char *oarg, int oint, void *data)
{
    lua_State *L = data;
    char key[2] = { c, '\0' };

    lua_pushstring(L, oarg ? oarg : "");
    lua_setfield(L, -2, key);
    return 0;
}

int rpmluaRunScript(rpmlua _lua, const char *script, const char *name,
		    const char *opts, ARGV_t args)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    int ret = -1;
    int oind = 0;
    static const char *lualocal =
	"local opt = select(1, ...); local arg = select(2, ...);";

    if (name == NULL)
	name = "<lua>";
    if (script == NULL)
	script = "";

    char *buf = rstrscat(NULL, lualocal, script, NULL);

    if (luaL_loadbuffer(L, buf, strlen(buf), name) != 0) {
	rpmlog(RPMLOG_ERR, _("invalid syntax in lua script: %s\n"),
		 lua_tostring(L, -1));
	lua_pop(L, 1);
	goto exit;
    }

    lua_newtable(L);
    if (opts) {
	int argc = argvCount(args);

	oind = rgetopt(argc, args, opts, luaopt, L);

	if (oind < 0) {
	    rpmlog(RPMLOG_ERR, _("Unknown option %c in %s(%s)\n"),
		    -oind, name, opts);
	    lua_pop(L, 2);
	    goto exit;
	}
    }

    lua_newtable(L);
    if (args) {
	int i = 1;
	for (ARGV_const_t arg = args + oind; arg && *arg; arg++) {
	    lua_pushstring(L, *arg);
	    lua_rawseti(L, -2, i++);
	}
    }

    if (lua_pcall(L, 2, 0, 0) != 0) {
	rpmlog(RPMLOG_ERR, _("lua script failed: %s\n"),
		 lua_tostring(L, -1));
	lua_pop(L, 1);
	goto exit;
    }
    ret = 0;

exit:
    free(buf);
    return ret;
}

int rpmluaRunScriptFile(rpmlua _lua, const char *filename)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    int ret = 0;
    if (luaL_loadfile(L, filename) != 0) {
	rpmlog(RPMLOG_ERR, _("invalid syntax in lua file: %s\n"),
		 lua_tostring(L, -1));
	lua_pop(L, 1);
	ret = -1;
    } else if (lua_pcall(L, 0, 0, 0) != 0) {
	rpmlog(RPMLOG_ERR, _("lua script failed: %s\n"),
		 lua_tostring(L, -1));
	lua_pop(L, 1);
	ret = -1;
    }
    return ret;
}

/* From lua.c */
static int rpmluaReadline(lua_State *L, const char *prompt)
{
   static char buffer[1024];
   if (prompt) {
      (void) fputs(prompt, stdout);
      (void) fflush(stdout);
   }
   if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      return 0;  /* read fails */
   } else {
      lua_pushstring(L, buffer);
      return 1;
   }
}

/* Based on lua.c */
static void _rpmluaInteractive(lua_State *L)
{
   rpmlua lua = getdata(L, "lua");
   (void) fputs("\n", stdout);
   printf("RPM Interactive %s Interpreter\n", LUA_VERSION);
   for (;;) {
      int rc = 0;

      if (rpmluaReadline(L, "> ") == 0)
	 break;
      if (lua_tostring(L, -1)[0] == '=') {
	 (void) lua_pushfstring(L, "print(%s)", lua_tostring(L, -1)+1);
	 lua_remove(L, -2);
      }
      for (;;) {
         size_t len;
	 const char *code = lua_tolstring(L, -1, &len);
	 rc = luaL_loadbuffer(L, code, len, "<lua>");
	 if (rc == LUA_ERRSYNTAX &&
	     strstr(lua_tostring(L, -1), "near `<eof>'") != NULL) {
	    if (rpmluaReadline(L, ">> ") == 0)
	       break;
	    lua_remove(L, -2); /* Remove error */
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
      } else {
	 char *s = rpmluaPopPrintBuffer(lua);
	 if (s) {
	    fprintf(stdout, "%s\n", s);
	    free(s);
	 }
      }
      lua_pop(L, 1); /* Remove line */
   }
   (void) fputs("\n", stdout);
}

void rpmluaInteractive(rpmlua _lua)
{
    INITSTATE(_lua, lua);
    _rpmluaInteractive(lua->L);
}

/* ------------------------------------------------------------------ */
/* Lua API */

static int rpm_vercmp(lua_State *L)
{
    const char *s1 = luaL_checkstring(L, 1);
    const char *s2 = luaL_checkstring(L, 2);
    int rc = 0;

    if (s1 && s2) {
	rpmver v1 = rpmverParse(luaL_checkstring(L, 1));
	rpmver v2 = rpmverParse(luaL_checkstring(L, 2));
	if (v1 && v2) {
	    lua_pushinteger(L, rpmverCmp(v1, v2));
	    rc = 1;
	} else {
	    if (v1 == NULL)
		luaL_argerror(L, 1, "invalid version ");
	    if (v1 == NULL)
		luaL_argerror(L, 2, "invalid version ");
	}
	rpmverFree(v1);
	rpmverFree(v2);
    }
    return rc;
}

static int rpm_b64encode(lua_State *L)
{
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    int linelen = -1;
    if (lua_gettop(L) == 2)
	linelen = luaL_checkinteger(L, 2);
    if (str && len) {
	char *data = rpmBase64Encode(str, len, linelen);
	lua_pushstring(L, data);
	free(data);
    }
    return 1;
}

static int rpm_b64decode(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    if (str) {
	void *data = NULL;
	size_t len = 0;
	if (rpmBase64Decode(str, &data, &len) == 0) {
	    lua_pushlstring(L, data, len);
	} else {
	    lua_pushnil(L);
	}
	free(data);
    }
    return 1;
}

static int rpm_isdefined(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    int parametric = 0;
    int defined = rpmMacroIsDefined(NULL, str);
    if (defined)
	parametric = rpmMacroIsParametric(NULL, str);
    lua_pushboolean(L, defined);
    lua_pushboolean(L, parametric);
    return 2;
}

static int rpm_expand(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    char *val = NULL;
    if (rpmExpandMacros(NULL, str, &val, 0) < 0)
	return luaL_error(L, "error expanding macro");
    lua_pushstring(L, val);
    free(val);
    return 1;
}

static int rpm_define(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    if (rpmDefineMacro(NULL, str, 0))
	return luaL_error(L, "error defining macro");
    return 0;
}

static int rpm_undefine(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    rpmPopMacro(NULL, str);
    return 0;
}

static int rpm_load(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    int rc = rpmLoadMacroFile(NULL, str);
    lua_pushnumber(L, rc);
    return 1;
}

static int rpm_interactive(lua_State *L)
{
    if (!(isatty(STDOUT_FILENO) && isatty(STDIN_FILENO)))
	return luaL_error(L, "not a tty");

    _rpmluaInteractive(L);
    return 0;
}

static int rpm_next_file(lua_State *L)
{
    if (nextFileFunc)
	lua_pushstring(L, nextFileFunc(nextFileFuncParam));
    else
	lua_pushstring(L, NULL);

    return 1;
}

typedef struct rpmluaHookData_s {
    lua_State *L;
    int funcRef;
    int dataRef;
} * rpmluaHookData;

static int rpmluaHookWrapper(rpmhookArgs args, void *data)
{
    rpmluaHookData hookdata = (rpmluaHookData)data;
    lua_State *L = hookdata->L;
    int ret = 0;
    int i;
    lua_rawgeti(L, LUA_REGISTRYINDEX, hookdata->funcRef);
    lua_newtable(L);
    for (i = 0; i != args->argc; i++) {
	switch (args->argt[i]) {
	    case 's':
		lua_pushstring(L, args->argv[i].s);
		lua_rawseti(L, -2, i+1);
		break;
	    case 'i':
		lua_pushnumber(L, (lua_Number)args->argv[i].i);
		lua_rawseti(L, -2, i+1);
		break;
	    case 'f':
		lua_pushnumber(L, (lua_Number)args->argv[i].f);
		lua_rawseti(L, -2, i+1);
		break;
	    case 'p':
		lua_pushlightuserdata(L, args->argv[i].p);
		lua_rawseti(L, -2, i+1);
		break;
	    default:
                (void) luaL_error(L, "unsupported type '%c' as "
                              "a hook argument\n", args->argt[i]);
		break;
	}
    }
    if (lua_pcall(L, 1, 1, 0) != 0) {
	rpmlog(RPMLOG_ERR, _("lua hook failed: %s\n"),
		 lua_tostring(L, -1));
	lua_pop(L, 1);
    } else {
	if (lua_isnumber(L, -1))
	    ret = (int)lua_tonumber(L, -1);
	lua_pop(L, 1);
    }
    return ret;
}

static int rpm_register(lua_State *L)
{
    if (!lua_isstring(L, 1)) {
	(void) luaL_argerror(L, 1, "hook name expected");
    } else if (!lua_isfunction(L, 2)) {
	(void) luaL_argerror(L, 2, "function expected");
    } else {
	rpmluaHookData hookdata =
	    lua_newuserdata(L, sizeof(struct rpmluaHookData_s));
	lua_pushvalue(L, -1);
	hookdata->dataRef = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushvalue(L, 2);
	hookdata->funcRef = luaL_ref(L, LUA_REGISTRYINDEX);
	hookdata->L = L;
	rpmhookRegister(lua_tostring(L, 1), rpmluaHookWrapper, hookdata);
	return 1;
    }
    return 0;
}

static int rpm_unregister(lua_State *L)
{
    if (!lua_isstring(L, 1)) {
	(void) luaL_argerror(L, 1, "hook name expected");
    } else if (!lua_isuserdata(L, 2)) {
	(void) luaL_argerror(L, 2, "hook information expected");
    } else {
	rpmluaHookData hookdata = (rpmluaHookData)lua_touserdata(L, 2);
	luaL_unref(L, LUA_REGISTRYINDEX, hookdata->funcRef);
	luaL_unref(L, LUA_REGISTRYINDEX, hookdata->dataRef);
	rpmhookUnregister(lua_tostring(L, 1), rpmluaHookWrapper, hookdata);
    }
    return 0;
}

static int rpm_call(lua_State *L)
{
    if (!lua_isstring(L, 1)) {
	(void) luaL_argerror(L, 1, "hook name expected");
    } else {
	rpmhookArgs args = rpmhookArgsNew(lua_gettop(L)-1);
	const char *name = lua_tostring(L, 1);
	char *argt = (char *)xmalloc(args->argc+1);
	int i;
	for (i = 0; i != args->argc; i++) {
	    switch (lua_type(L, i+1)) {
		case LUA_TNIL:
		    argt[i] = 'p';
		    args->argv[i].p = NULL;
		    break;
		case LUA_TNUMBER: {
		    float f = (float)lua_tonumber(L, i+1);
		    if (f == (int)f) {
			argt[i] = 'i';
			args->argv[i].i = (int)f;
		    } else {
			argt[i] = 'f';
			args->argv[i].f = f;
		    }
		}   break;
		case LUA_TSTRING:
		    argt[i] = 's';
		    args->argv[i].s = lua_tostring(L, i+1);
		    break;
		case LUA_TUSERDATA:
		case LUA_TLIGHTUSERDATA:
		    argt[i] = 'p';
		    args->argv[i].p = lua_touserdata(L, i+1);
		    break;
		default:
		    (void) luaL_error(L, "unsupported Lua type passed to hook");
		    argt[i] = 'p';
		    args->argv[i].p = NULL;
		    break;
	    }
	}
	args->argt = argt;
	rpmhookCallArgs(name, args);
	free(argt);
	(void) rpmhookArgsFree(args);
    }
    return 0;
}

/* Based on luaB_print. */
static int rpm_print (lua_State *L)
{
    rpmlua lua = (rpmlua)getdata(L, "lua");
    int n = lua_gettop(L);  /* number of arguments */
    int i;
    if (!lua) return 0;
    lua_getglobal(L, "tostring");
    for (i = 1; i <= n; i++) {
	const char *s;
	size_t sl;
	lua_pushvalue(L, -1);  /* function to be called */
	lua_pushvalue(L, i);   /* value to print */
	lua_call(L, 1, 1);
	s = lua_tolstring(L, -1, &sl);  /* get result */
	if (s == NULL)
	    return luaL_error(L, "`tostring' must return a string to `print'");
	if (lua->printbuf) {
	    rpmluapb prbuf = lua->printbuf;
	    if (prbuf->used+sl+1 > prbuf->alloced) {
		prbuf->alloced += sl+512;
		prbuf->buf = xrealloc(prbuf->buf, prbuf->alloced);
	    }
	    if (i > 1)
		prbuf->buf[prbuf->used++] = '\t';
	    memcpy(prbuf->buf+prbuf->used, s, sl+1);
	    prbuf->used += sl;
	} else {
	    if (i > 1)
		(void) fputs("\t", stdout);
	    (void) fputs(s, stdout);
	}
	lua_pop(L, 1);  /* pop result */
    }
    if (!lua->printbuf) {
	(void) fputs("\n", stdout);
    } else {
	rpmluapb prbuf = lua->printbuf;
	if (prbuf->used+1 > prbuf->alloced) {
	    prbuf->alloced += 512;
	    prbuf->buf = xrealloc(prbuf->buf, prbuf->alloced);
	}
	prbuf->buf[prbuf->used] = '\0';
    }
    return 0;
}

static int rpm_redirect2null(lua_State *L)
{
    int target_fd, fd, r, e;

    if (!_rpmlua_have_forked)
	return luaL_error(L, "redirect2null not permitted in this context");

    target_fd = luaL_checkinteger(L, 1);

    r = fd = open("/dev/null", O_WRONLY);
    if (fd >= 0 && fd != target_fd) {
	r = dup2(fd, target_fd);
	e = errno;
	(void) close(fd);
	errno = e;
    }
    return pushresult(L, r, NULL);
}

static int rpm_exit(lua_State *L)
{
    if (!_rpmlua_have_forked)
	return luaL_error(L, "exit not permitted in this context");

    exit(luaL_optinteger(L, 1, EXIT_SUCCESS));
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

static int newinstance(lua_State *L, const char *name, void *p)
{
    if (p != NULL) {
	intptr_t **pp = lua_newuserdata(L, sizeof(*pp));
	*pp = p;
	luaL_getmetatable(L, name);
	lua_setmetatable(L, -2);
    }
    return (p != NULL) ? 1 : 0;
}

static int createclass(lua_State *L, const char *name, const luaL_Reg *methods)
{
    luaL_newmetatable(L, name);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, methods, 0);
    return 0;
}

static rpmver * checkver(lua_State *L, int ix)
{
    rpmver *vp = lua_touserdata(L, ix);
    luaL_checkudata(L, ix, "rpm.ver");
    return vp;
}

static int ver_index(lua_State *L)
{
    rpmver *vp = checkver(L, 1);
    const char *key = luaL_checkstring(L, 2);
    const char *s = NULL;

    if (rstreq(key, "e"))
	s = rpmverE(*vp);
    else if (rstreq(key, "v"))
	s = rpmverV(*vp);
    else if (rstreq(key, "r"))
	s = rpmverR(*vp);
    else
	return luaL_error(L, "invalid attribute: %s", key);

    lua_pushstring(L, s);
    return 1;
}

static int ver_tostring(lua_State *L)
{
    rpmver *vp = checkver(L, 1);
    char *evr = rpmverEVR(*vp);
    lua_pushstring(L, evr);
    free(evr);
    return 1;
}

static int ver_gc(lua_State *L)
{
    rpmver *vp = checkver(L, 1);
    *vp = rpmverFree(*vp);
    return 0;
}

static int ver_cmp(lua_State *L, int expect)
{
    rpmver *vp1 = checkver(L, 1);
    rpmver *vp2 = checkver(L, 2);
    return (rpmverCmp(*vp1, *vp2) == expect);
}

static int ver_eq(lua_State *L)
{
    return ver_cmp(L, 0);
}

static int ver_lt(lua_State *L)
{
    return ver_cmp(L, -1);
}

static int ver_le(lua_State *L)
{
    return ver_eq(L) || ver_lt(L);
}

static int rpm_ver_new(lua_State *L)
{
    int nargs = lua_gettop(L);
    rpmver ver = NULL;

    if (nargs == 1) {
	const char *evr = lua_tostring(L, 1);
	ver = rpmverParse(evr);
    } else if (nargs == 3) {
	const char *e = lua_tostring(L, 1);
	const char *v = lua_tostring(L, 2);
	const char *r = lua_tostring(L, 3);
	ver = rpmverNew(e, v, r);
    } else {
	luaL_error(L, "invalid number of arguments: %d", nargs);
    }

    return newinstance(L, "rpm.ver", ver);
}

static const luaL_Reg ver_m[] = {
    {"__index", ver_index},
    {"__tostring", ver_tostring},
    {"__eq", ver_eq},
    {"__le", ver_le},
    {"__lt", ver_lt},
    {"__gc", ver_gc},
    {NULL, NULL}
};

static FD_t * checkfd(lua_State *L, int ix)
{
    FD_t *fdp = lua_touserdata(L, ix);
    luaL_checkudata(L, ix, "rpm.fd");
    return fdp;
}

static int fd_tostring(lua_State *L)
{
    FD_t *fdp = checkfd(L, 1);
    lua_pushstring(L, Fdescr(*fdp));
    return 1;
}

static int fd_reopen(lua_State *L)
{
    FD_t *ofdp = checkfd(L, 1);
    const char *mode = luaL_checkstring(L, 2);

    FD_t fd = Fdopen(*ofdp, mode);

    if (fd == NULL) {
	return luaL_error(L, "%s stream reopen failed (invalid mode?)",
			Fdescr(*ofdp));
    }
    *ofdp = fd;
    lua_pushvalue(L, 1);

    return 1;
}

static int fd_close(lua_State *L)
{
    FD_t *fdp = checkfd(L, 1);
    int rc = -1;
    if (*fdp) {
	rc = Fclose(*fdp);
	*fdp = NULL;
    }
    lua_pushinteger(L, rc);
    return 1;
}

static int fd_read(lua_State *L)
{
    FD_t *fdp = checkfd(L, 1);
    char buf[BUFSIZ];
    ssize_t chunksize = sizeof(buf);
    ssize_t left = luaL_optinteger(L, 2, -1);
    ssize_t nb = 0;

    lua_pushstring(L, "");

    do {
	if (left >= 0 && left < chunksize)
	    chunksize = left;
	nb = Fread(buf, 1, chunksize, *fdp);

	if (Ferror(*fdp)) {
	    return luaL_error(L, "error reading %s: %s",
				Fdescr(*fdp), Fstrerror(*fdp));
	}

	if (nb > 0) {
	    lua_pushlstring(L, buf, nb);
	    lua_concat(L, 2);
	    left -= nb;
	}
    } while (nb > 0);

    return 1;
}

static int fd_write(lua_State *L)
{
    FD_t *fdp = checkfd(L, 1);
    size_t len;
    const char *buf = luaL_checklstring(L, 2, &len);
    ssize_t size = luaL_optinteger(L, 3, len);

    ssize_t wrote = Fwrite(buf, 1, size, *fdp);

    if (Ferror(*fdp) || (size != wrote))
	return luaL_error(L, "error writing %s: %s",
			Fdescr(*fdp), Fstrerror(*fdp));

    lua_pushinteger(L, wrote);
    return 1;
}

static int fd_flush(lua_State *L)
{
    FD_t *fdp = checkfd(L, 1);
    int rc = Fflush(*fdp);
    lua_pushinteger(L, rc);
    return 1;
}

static int fd_seek(lua_State *L)
{
    static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
    static const char *const modenames[] = {"set", "cur", "end", NULL};
    FD_t *fdp = checkfd(L, 1);
    int op = luaL_checkoption(L, 2, "cur", modenames);
    off_t offset = luaL_optinteger(L, 3, 0);

    op = Fseek(*fdp, offset, mode[op]);

    if (op < 0 || Ferror(*fdp)) {
	return luaL_error(L, "%s: seek failed: %s",
			Fdescr(*fdp), Fstrerror(*fdp));
    }

    offset = Ftell(*fdp);
    lua_pushinteger(L, offset);

    return 1;
}

static int fd_gc(lua_State *L)
{
    fd_close(L);
    lua_pop(L, 1);
    return 0;
}

static int rpm_open(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    const char *mode = luaL_optlstring(L, 2, "r", NULL);

    FD_t fd = Fopen(path, mode);

    if (fd == NULL) {
	return luaL_error(L, "%s open failed: %s", path, strerror(errno));
    }

    return newinstance(L, "rpm.fd", fd);
}

static const luaL_Reg fd_m[] = {
    {"read", fd_read},
    {"write", fd_write},
    {"flush", fd_flush},
    {"close", fd_close},
    {"seek", fd_seek},
    {"reopen", fd_reopen},
    {"__tostring", fd_tostring},
    {"__gc", fd_gc},
    {NULL, NULL}
};

static rpmMacroContext *checkmc(lua_State *L, int ix)
{
    rpmMacroContext *mc = lua_touserdata(L, ix);
    luaL_checkudata(L, ix, "rpm.mc");
    return mc;
}

static int mc_call(lua_State *L)
{
    rpmMacroContext *mc = checkmc(L, lua_upvalueindex(1));
    const char *name = lua_tostring(L, lua_upvalueindex(2));
    int rc = 0;

    if (lua_gettop(L) > 1)
	luaL_error(L, "too many arguments");

    if (lua_isstring(L, 1)) {
	lua_pushfstring(L, "%%{%s %s}", name, lua_tostring(L, 1));
	/* throw out previous args and call expand() with our result string */
	lua_rotate(L, 1, 1);
	lua_settop(L, 1);
	rc = rpm_expand(L);
    } else if (lua_istable(L, 1)) {
	ARGV_t argv = NULL;
	char *buf = NULL;
	int nitem = lua_rawlen(L, 1);

	for (int i = 1; i <= nitem; i++) {
	    lua_rawgeti(L, 1, i);
	    argvAdd(&argv, lua_tostring(L, -1));
	    lua_pop(L, 1);
	}

	if (rpmExpandThisMacro(*mc, name, argv, &buf, 0) >= 0) {
	    rc = 1;
	    lua_pushstring(L, buf);
	    free(buf);
	}
	argvFree(argv);
    } else {
	luaL_argerror(L, 1, "string or table expected");
    }

    return rc;
}

static int mc_index(lua_State *L)
{
    rpmMacroContext *mc = checkmc(L, 1);
    const char *a = luaL_checkstring(L, 2);
    int rc = 0;

    if (rpmMacroIsDefined(*mc, a)) {
	if (rpmMacroIsParametric(*mc, a)) {;
	    /* closure with the macro context and the name */
	    lua_pushcclosure(L, &mc_call, 2);
	    rc = 1;
	} else {
	    lua_pushfstring(L, "%%{%s}", a);
	    lua_rotate(L, 1, 1);
	    lua_settop(L, 1);
	    rc = rpm_expand(L);
	}
    }
    return rc;
}

static int mc_newindex(lua_State *L)
{
    rpmMacroContext *mc = checkmc(L, 1);
    const char *name = luaL_checkstring(L, 2);
    if (lua_isnil(L, 3)) {
	if (rpmPopMacro(*mc, name))
	    luaL_error(L, "error undefining macro %s", name);
    } else {
	const char *body = luaL_checkstring(L, 3);
	char *s = rstrscat(NULL, name, " ", body, NULL);
	if (rpmDefineMacro(*mc, s, 0))
	    luaL_error(L, "error defining macro %s", name);
	free(s);
    }
    return 0;
}

static const luaL_Reg mc_m[] = {
    {"__index", mc_index},
    {"__newindex", mc_newindex},
    {NULL, NULL}
};

static void createmt(lua_State *L, const char *name, rpmMacroContext mc)
{
    lua_pushglobaltable(L);
    newinstance(L, "rpm.mc", mc);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

static const luaL_Reg rpmlib[] = {
    {"b64encode", rpm_b64encode},
    {"b64decode", rpm_b64decode},
    {"expand", rpm_expand},
    {"define", rpm_define},
    {"undefine", rpm_undefine},
    {"isdefined", rpm_isdefined},
    {"load", rpm_load},
    {"register", rpm_register},
    {"unregister", rpm_unregister},
    {"call", rpm_call},
    {"interactive", rpm_interactive},
    {"next_file", rpm_next_file},
    {"execute", rpm_execute},
    {"redirect2null", rpm_redirect2null},
    {"vercmp", rpm_vercmp},
    {"ver", rpm_ver_new},
    {"open", rpm_open},
    {NULL, NULL}
};

static int luaopen_rpm(lua_State *L)
{
    createclass(L, "rpm.ver", ver_m);
    createclass(L, "rpm.fd", fd_m);
    createclass(L, "rpm.mc", mc_m);

    createmt(L, "macros", rpmGlobalMacroContext);

    luaL_newlib(L, rpmlib);
    return 1;
}
