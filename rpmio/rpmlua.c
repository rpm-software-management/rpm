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

#include "debug.h"

int _rpmlua_have_forked = 0;

typedef struct rpmluapb_s * rpmluapb;

struct rpmlua_s {
    lua_State *L;
    size_t pushsize;
    rpmluapb printbuf;
};

struct rpmluav_s {
    rpmluavType keyType;
    rpmluavType valueType;
    union {
	const char *str;
	const void *ptr;
	double num;
    } key;
    union {
	const char *str;
	const void *ptr;
	double num;
    } value;
    int listmode;
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

static int pushvar(lua_State *L, rpmluavType type, void *value)
{
    int ret = 0;
    switch (type) {
	case RPMLUAV_NIL:
	    lua_pushnil(L);
	    break;
	case RPMLUAV_STRING:
	    lua_pushstring(L, *((char **)value));
	    break;
	case RPMLUAV_NUMBER:
	    lua_pushnumber(L, *((double *)value));
	    break;
	default:
	    ret = -1;
	    break;
    }
    return ret;
}

void rpmluaSetNextFileFunc(char *(*func)(void *), void *funcParam)
{
    nextFileFunc = func;
    nextFileFuncParam = funcParam;
}

void rpmluaSetVar(rpmlua _lua, rpmluav var)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    if (var->listmode && lua->pushsize > 0) {
	if (var->keyType != RPMLUAV_NUMBER || var->key.num == (double)0) {
	    var->keyType = RPMLUAV_NUMBER;
	    var->key.num = (double)lua_rawlen(L, -1);
	}
	var->key.num++;
    }
    if (!var->listmode || lua->pushsize > 0) {
	if (lua->pushsize == 0)
	    lua_pushglobaltable(L);
	if (pushvar(L, var->keyType, &var->key) != -1) {
	    if (pushvar(L, var->valueType, &var->value) != -1)
		lua_rawset(L, -3);
	    else
		lua_pop(L, 1);
	}
	if (lua->pushsize == 0)
	    lua_pop(L, 1);
    }
}

static void popvar(lua_State *L, rpmluavType *type, void *value)
{
    switch (lua_type(L, -1)) {
    case LUA_TSTRING:
	*type = RPMLUAV_STRING;
	*((const char **)value) = lua_tostring(L, -1);
	break;
    case LUA_TNUMBER:
	*type = RPMLUAV_NUMBER;
	*((double *)value) = lua_tonumber(L, -1);
	break;
    default:
	*type = RPMLUAV_NIL;
	*((void **)value) = NULL;
	break;
    }
    lua_pop(L, 1);
}

void rpmluaGetVar(rpmlua _lua, rpmluav var)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    if (!var->listmode) {
	if (lua->pushsize == 0)
	    lua_pushglobaltable(L);
	if (pushvar(L, var->keyType, &var->key) != -1) {
	    lua_rawget(L, -2);
	    popvar(L, &var->valueType, &var->value);
	}
	if (lua->pushsize == 0)
	    lua_pop(L, 1);
    } else if (lua->pushsize > 0) {
	(void) pushvar(L, var->keyType, &var->key);
	if (lua_next(L, -2) != 0)
	    popvar(L, &var->valueType, &var->value);
    }
}

#define FINDKEY_RETURN 0
#define FINDKEY_CREATE 1
#define FINDKEY_REMOVE 2
static int findkey(lua_State *L, int oper, const char *key, va_list va)
{
    char *buf;
    const char *s, *e;
    int ret = 0;
    int blen;

    blen = vsnprintf(NULL, 0, key, va);
    if (blen <= 0) {
	return -1;
    }

    buf = xmalloc(blen + 1);
    vsnprintf(buf, blen + 1, key, va);

    s = e = buf;
    lua_pushglobaltable(L);
    for (;;) {
	if (*e == '\0' || *e == '.') {
	    if (e != s) {
		lua_pushlstring(L, s, e-s);
		switch (oper) {
		case FINDKEY_REMOVE:
		    if (*e == '\0') {
			lua_pushnil(L);
			lua_rawset(L, -3);
			lua_pop(L, 1);
			break;
		    }
		case FINDKEY_RETURN:
		    lua_rawget(L, -2);
		    lua_remove(L, -2);
		    break;
		case FINDKEY_CREATE:
		    lua_rawget(L, -2);
		    if (!lua_istable(L, -1)) {
			lua_pop(L, 1);
			lua_newtable(L);
			lua_pushlstring(L, s, e-s);
			lua_pushvalue(L, -2);
			lua_rawset(L, -4);
		    }
		    lua_remove(L, -2);
		    break;
		}
	    }
	    if (*e == '\0')
		break;
	    if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		ret = -1;
		break;
	    }
	    s = e+1;
	}
	e++;
    }
    free(buf);

    return ret;
}

void rpmluaDelVar(rpmlua _lua, const char *key, ...)
{
    INITSTATE(_lua, lua);
    va_list va;
    va_start(va, key);
    (void) findkey(lua->L, FINDKEY_REMOVE, key, va);
    va_end(va);
}

int rpmluaVarExists(rpmlua _lua, const char *key, ...)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    int ret = 0;
    va_list va;
    va_start(va, key);
    if (findkey(L, FINDKEY_RETURN, key, va) == 0) {
	if (!lua_isnil(L, -1))
	    ret = 1;
	lua_pop(L, 1);
    }
    va_end(va);
    return ret;
}

void rpmluaPushTable(rpmlua _lua, const char *key, ...)
{
    INITSTATE(_lua, lua);
    va_list va;
    va_start(va, key);
    (void) findkey(lua->L, FINDKEY_CREATE, key, va);
    lua->pushsize++;
    va_end(va);
}

void rpmluaPop(rpmlua _lua)
{
    INITSTATE(_lua, lua);
    assert(lua->pushsize > 0);
    lua->pushsize--;
    lua_pop(lua->L, 1);
}

rpmluav rpmluavNew(void)
{
    rpmluav var = (rpmluav) xcalloc(1, sizeof(*var));
    return var;
}

rpmluav rpmluavFree(rpmluav var)
{
    free(var);
    return NULL;
}

void rpmluavSetListMode(rpmluav var, int flag)
{
    var->listmode = flag;
    var->keyType = RPMLUAV_NIL;
}

void rpmluavSetKey(rpmluav var, rpmluavType type, const void *value)
{
    var->keyType = type;
    switch (type) {
	case RPMLUAV_NUMBER:
	    var->key.num = *((double *)value);
	    break;
	case RPMLUAV_STRING:
	    var->key.str = (char *)value;
	    break;
	default:
	    break;
    }
}

void rpmluavSetValue(rpmluav var, rpmluavType type, const void *value)
{
    var->valueType = type;
    switch (type) {
	case RPMLUAV_NUMBER:
	    var->value.num = *((const double *)value);
	    break;
	case RPMLUAV_STRING:
	    var->value.str = (const char *)value;
	    break;
	default:
	    break;
    }
}

void rpmluavGetKey(rpmluav var, rpmluavType *type, void **value)
{
    *type = var->keyType;
    switch (var->keyType) {
	case RPMLUAV_NUMBER:
	    *((double **)value) = &var->key.num;
	    break;
	case RPMLUAV_STRING:
	    *((const char **)value) = var->key.str;
	    break;
	default:
	    break;
    }
}

void rpmluavGetValue(rpmluav var, rpmluavType *type, void **value)
{
    *type = var->valueType;
    switch (var->valueType) {
	case RPMLUAV_NUMBER:
	    *((double **)value) = &var->value.num;
	    break;
	case RPMLUAV_STRING:
	    *((const char **)value) = var->value.str;
	    break;
	default:
	    break;
    }
}

void rpmluavSetKeyNum(rpmluav var, double value)
{
    rpmluavSetKey(var, RPMLUAV_NUMBER, &value);
}

void rpmluavSetValueNum(rpmluav var, double value)
{
    rpmluavSetValue(var, RPMLUAV_NUMBER, &value);
}

double rpmluavGetKeyNum(rpmluav var)
{
    rpmluavType type;
    void *value;
    rpmluavGetKey(var, &type, &value);
    if (type == RPMLUAV_NUMBER)
	return *((double *)value);
    return (double) 0;
}

double rpmluavGetValueNum(rpmluav var)
{
    rpmluavType type;
    void *value;
    rpmluavGetValue(var, &type, &value);
    if (type == RPMLUAV_NUMBER)
	return *((double *)value);
    return (double) 0;
}

int rpmluavKeyIsNum(rpmluav var)
{
    return (var->keyType == RPMLUAV_NUMBER) ? 1 : 0;
}

int rpmluavValueIsNum(rpmluav var)
{
    return (var->valueType == RPMLUAV_NUMBER) ? 1 : 0;
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

int rpmluaRunScript(rpmlua _lua, const char *script, const char *name)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    int ret = 0;
    if (name == NULL)
	name = "<lua>";
    if (script == NULL)
	script = "";
    if (luaL_loadbuffer(L, script, strlen(script), name) != 0) {
	rpmlog(RPMLOG_ERR, _("invalid syntax in lua script: %s\n"),
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

static const luaL_Reg rpmlib[] = {
    {"b64encode", rpm_b64encode},
    {"b64decode", rpm_b64decode},
    {"expand", rpm_expand},
    {"define", rpm_define},
    {"undefine", rpm_undefine},
    {"load", rpm_load},
    {"register", rpm_register},
    {"unregister", rpm_unregister},
    {"call", rpm_call},
    {"interactive", rpm_interactive},
    {"next_file", rpm_next_file},
    {"execute", rpm_execute},
    {"redirect2null", rpm_redirect2null},
    {"vercmp", rpm_vercmp},
    {NULL, NULL}
};

static int luaopen_rpm(lua_State *L)
{
    luaL_newlib(L, rpmlib);
    return 1;
}
