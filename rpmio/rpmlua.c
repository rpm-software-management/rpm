#include "system.h"
#include <rpmio.h>
#include <rpmmacro.h>
#include <rpmerr.h>
#include <rpmurl.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lposix.h>
#include <lrexlib.h>

#include <unistd.h>
#include <assert.h>

#define _RPMLUA_INTERNAL
#include "rpmlua.h"

#if !defined(HAVE_VSNPRINTF)
static inline int vsnprintf(char * buf, /*@unused@*/ int nb,
			    const char * fmt, va_list ap)
{
    return vsprintf(buf, fmt, ap);
}
#endif

static int luaopen_rpm(lua_State *L);
static int rpm_print(lua_State *L);

rpmlua rpmluaNew()
{
    rpmlua lua = (rpmlua) xcalloc(1, sizeof(struct rpmlua_s));
    lua_State *L = lua_open();
    lua->L = L;
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
    lua_pushliteral(L, "print");
    lua_pushcfunction(L, rpm_print);
    lua_rawset(L, LUA_GLOBALSINDEX);
    rpmluaSetData(lua, "lua", lua);
    return lua;
}

void *rpmluaFree(rpmlua lua)
{
    if (lua) {
	if (lua->L) lua_close(lua->L);
	free(lua->printbuf);
	free(lua);
    }
    return NULL;
}

void rpmluaSetData(rpmlua lua, const char *key, const void *data)
{
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

void *rpmluaGetData(rpmlua lua, const char *key)
{
    return getdata(lua->L, key);
}

void rpmluaSetPrintBuffer(rpmlua lua, int flag)
{
    lua->storeprint = flag;
    free(lua->printbuf);
    lua->printbuf = NULL;
    lua->printbufsize = 0;
}

const char *rpmluaGetPrintBuffer(rpmlua lua)
{
    return lua->printbuf;
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

void rpmluaSetVar(rpmlua lua, rpmluav var)
{
    lua_State *L = lua->L;
    if (var->listmode && lua->pushsize > 0) {
	if (var->keyType != RPMLUAV_NUMBER || var->key.num == 0) {
	    var->keyType = RPMLUAV_NUMBER;
	    var->key.num = luaL_getn(L, -1);
	}
	var->key.num++;
    }
    if (!var->listmode || lua->pushsize > 0) {
	if (lua->pushsize == 0)
	    lua_pushvalue(L, LUA_GLOBALSINDEX);
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

void rpmluaGetVar(rpmlua lua, rpmluav var)
{
    lua_State *L = lua->L;
    if (!var->listmode) {
	if (lua->pushsize == 0)
	    lua_pushvalue(L, LUA_GLOBALSINDEX);
	if (pushvar(L, var->keyType, &var->key) != -1) {
	    lua_rawget(L, -2);
	    popvar(L, &var->valueType, &var->value);
	}
	if (lua->pushsize == 0)
	    lua_pop(L, 1);
    } else if (lua->pushsize > 0) {
	pushvar(L, var->keyType, &var->key);
	if (lua_next(L, -2) != 0)
	    popvar(L, &var->valueType, &var->value);
    }
}

#define FINDKEY_RETURN 0
#define FINDKEY_CREATE 1
#define FINDKEY_REMOVE 2
static int findkey(lua_State *L, int oper, const char *key, va_list va)
{
    char buf[BUFSIZ];
    const char *s, *e;
    int ret = 0;
    vsnprintf(buf, BUFSIZ, key, va);
    s = e = buf;
    lua_pushvalue(L, LUA_GLOBALSINDEX);
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
			/* @fallthrough@ */
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

    return ret;
}

void rpmluaDelVar(rpmlua lua, const char *key, ...)
{
    va_list va;
    va_start(va, key);
    findkey(lua->L, FINDKEY_REMOVE, key, va);
    va_end(va);
}

int rpmluaVarExists(rpmlua lua, const char *key, ...)
{
    int ret = 0;
    va_list va;
    va_start(va, key);
    if (findkey(lua->L, FINDKEY_RETURN, key, va) == 0) {
	if (!lua_isnil(lua->L, -1))
	    ret = 1;
	lua_pop(lua->L, 1);
    }
    va_end(va);
    return ret;
}

void rpmluaPushTable(rpmlua lua, const char *key, ...)
{
    va_list va;
    va_start(va, key);
    findkey(lua->L, FINDKEY_CREATE, key, va);
    lua->pushsize++;
    va_end(va);
}

void rpmluaPop(rpmlua lua)
{
    assert(lua->pushsize > 0);
    lua->pushsize--;
    lua_pop(lua->L, 1);
}

rpmluav rpmluavNew(void)
{
    rpmluav var = (rpmluav) xcalloc(1, sizeof(struct rpmluav_s));
    return var;
}

void *rpmluavFree(rpmluav var)
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
    return 0;
}

double rpmluavGetValueNum(rpmluav var)
{
    rpmluavType type;
    void *value;
    rpmluavGetValue(var, &type, &value);
    if (type == RPMLUAV_NUMBER)
	return *((double *)value);
    return 0;
}

int rpmluavKeyIsNum(rpmluav var)
{
    return (var->keyType == RPMLUAV_NUMBER) ? 1 : 0;
}

int rpmluavValueIsNum(rpmluav var)
{
    return (var->valueType == RPMLUAV_NUMBER) ? 1 : 0;
}

int rpmluaCheckScript(rpmlua lua, const char *script, const char *name)
{
    lua_State *L = lua->L;
    int ret = 0;
    if (!name)
	name = "<lua>";
    if (luaL_loadbuffer(L, script, strlen(script), name) != 0) {
	rpmError(RPMERR_SCRIPT,
		_("invalid syntax in lua scriptlet: %s\n"),
		  lua_tostring(L, -1));
	ret = -1;
    }
    lua_pop(L, 1); /* Error or chunk. */
    return ret;
}

int rpmluaRunScript(rpmlua lua, const char *script, const char *name)
{
    lua_State *L = lua->L;
    int ret = 0;
    if (!name)
	name = "<lua>";
    if (luaL_loadbuffer(L, script, strlen(script), name) != 0) {
	rpmError(RPMERR_SCRIPT, _("invalid syntax in lua script: %s\n"),
		 lua_tostring(L, -1));
	lua_pop(L, 1);
	ret = -1;
    } else if (lua_pcall(L, 0, 0, 0) != 0) {
	rpmError(RPMERR_SCRIPT, _("lua script failed: %s\n"),
		 lua_tostring(L, -1));
	lua_pop(L, 1);
	ret = -1;
    }
    return ret;
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

/* Based on luaB_print. */
static int rpm_print (lua_State *L)
{
    rpmlua lua = (rpmlua)getdata(L, "lua");;
    int n = lua_gettop(L);  /* number of arguments */
    int i;
    if (!lua) return 0;
    lua_getglobal(L, "tostring");
    for (i = 1; i <= n; i++) {
	const char *s;
	lua_pushvalue(L, -1);  /* function to be called */
	lua_pushvalue(L, i);   /* value to print */
	lua_call(L, 1, 1);
	s = lua_tostring(L, -1);  /* get result */
	if (s == NULL)
	    return luaL_error(L, "`tostring' must return a string to `print'");
	if (lua->storeprint) {
	    int sl = lua_strlen(L, -1);
	    if (lua->printbufused+sl+1 > lua->printbufsize) {
		lua->printbufsize += sl+512;
		lua->printbuf = xrealloc(lua->printbuf, lua->printbufsize);
	    }
	    if (i > 1)
		lua->printbuf[lua->printbufused++] = '\t';
	    memcpy(lua->printbuf+lua->printbufused, s, sl+1);
	    lua->printbufused += sl;
	} else {
	    if (i > 1)
		fputs("\t", stdout);
	    fputs(s, stdout);
	}
	lua_pop(L, 1);  /* pop result */
    }
    lua_pop(L, 1);
    if (!lua->storeprint) {
	fputs("\n", stdout);
    } else {
	if (lua->printbufused+1 >= lua->printbufsize) {
	    lua->printbufsize += 512;
	    lua->printbuf = xrealloc(lua->printbuf, lua->printbufsize);
	}
	lua->printbuf[lua->printbufused++] = '\n';
	lua->printbuf[lua->printbufused] = '\0';
    }
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
