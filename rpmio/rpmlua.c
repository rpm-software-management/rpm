/*@-bounds -realcompare -sizeoftype -protoparammatch @*/
#include "system.h"
#include <rpmio.h>
#include <rpmmacro.h>
#include <rpmerr.h>
#include <rpmurl.h>
#include <rpmhook.h>

#include "../lua/include/lua.h"
#include "../lua/include/lualib.h"
#include "../lua/include/lauxlib.h"
#include "../lua/local/lposix.h"
#include "../lua/local/lrexlib.h"

#include <unistd.h>
#include <assert.h>

#define _RPMLUA_INTERNAL
#include "rpmlua.h"

#include "debug.h"

#if !defined(HAVE_VSNPRINTF)
static inline int vsnprintf(char * buf, /*@unused@*/ int nb,
			    const char * fmt, va_list ap)
{
    return vsprintf(buf, fmt, ap);
}
#endif

#define INITSTATE(_lua, lua) \
    rpmlua lua = _lua ? _lua : \
	    (globalLuaState ? globalLuaState : \
			/*@-mods@*/ \
			(globalLuaState = rpmluaNew()) \
			/*@=mods@*/ \
	    )

/*@only@*/ /*@unchecked@*/ /*@null@*/
static rpmlua globalLuaState = NULL;

static int luaopen_rpm(lua_State *L)
	/*@modifies L @*/;
static int rpm_print(lua_State *L)
	/*@globals fileSystem @*/
	/*@modifies L, fileSystem @*/;

rpmlua rpmluaNew()
{
    rpmlua lua = (rpmlua) xcalloc(1, sizeof(*lua));
    lua_State *L = lua_open();
    struct stat st;
    /*@-readonlytrans@*/
    /*@observer@*/ /*@unchecked@*/
    static const luaL_reg lualibs[] = {
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
    /*@observer@*/ /*@unchecked@*/
    const luaL_reg *lib = lualibs;
    /*@=readonlytrans@*/

    lua->L = L;
    for (; lib->name; lib++) {
/*@-noeffectuncon@*/
	(void) lib->func(L);
/*@=noeffectuncon@*/
	lua_settop(L, 0);
    }
    lua_pushliteral(L, "LUA_PATH");
    lua_pushstring(L, RPMCONFIGDIR "/lua/?.lua");
    lua_rawset(L, LUA_GLOBALSINDEX);
    lua_pushliteral(L, "print");
    lua_pushcfunction(L, rpm_print);
    lua_rawset(L, LUA_GLOBALSINDEX);
    rpmluaSetData(lua, "lua", lua);
    if (stat(RPMCONFIGDIR "/init.lua", &st) != -1)
	(void)rpmluaRunScriptFile(lua, RPMCONFIGDIR "/init.lua");
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
	/*@modifies L @*/
{
    void *ret = NULL;
    lua_pushliteral(L, "rpm_");
    lua_pushstring(L, key);
    lua_concat(L, 2);
    lua_rawget(L, LUA_REGISTRYINDEX);
/*@-branchstate@*/
    if (lua_islightuserdata(L, -1))
	ret = lua_touserdata(L, -1);
/*@=branchstate@*/
    lua_pop(L, 1);
    return ret;
}

void *rpmluaGetData(rpmlua _lua, const char *key)
{
    INITSTATE(_lua, lua);
    return getdata(lua->L, key);
}

void rpmluaSetPrintBuffer(rpmlua _lua, int flag)
{
    INITSTATE(_lua, lua);
    lua->storeprint = flag;
    free(lua->printbuf);
    lua->printbuf = NULL;
    lua->printbufsize = 0;
}

const char *rpmluaGetPrintBuffer(rpmlua _lua)
{
    INITSTATE(_lua, lua);
    return lua->printbuf;
}

static int pushvar(lua_State *L, rpmluavType type, void *value)
	/*@modifies L @*/
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

void rpmluaSetVar(rpmlua _lua, rpmluav var)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    if (var->listmode && lua->pushsize > 0) {
	if (var->keyType != RPMLUAV_NUMBER || var->key.num == (double)0) {
	    var->keyType = RPMLUAV_NUMBER;
	    var->key.num = (double) luaL_getn(L, -1);
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
	/*@modifies L, *type, *value @*/
{
    switch (lua_type(L, -1)) {
    case LUA_TSTRING:
	*type = RPMLUAV_STRING;
/*@-observertrans -dependenttrans @*/
	*((const char **)value) = lua_tostring(L, -1);
/*@=observertrans =dependenttrans @*/
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
	    lua_pushvalue(L, LUA_GLOBALSINDEX);
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
	/*@modifies L @*/
{
    char buf[BUFSIZ];
    const char *s, *e;
    int ret = 0;
    (void) vsnprintf(buf, BUFSIZ, key, va);
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
			/*@switchbreak@*/ break;
		    }
		    /*@fallthrough@*/
		case FINDKEY_RETURN:
		    lua_rawget(L, -2);
		    lua_remove(L, -2);
		    /*@switchbreak@*/ break;
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
		    /*@switchbreak@*/ break;
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
/*@-assignexpose -branchstate -temptrans @*/
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
/*@=assignexpose =branchstate =temptrans @*/
}

void rpmluavSetValue(rpmluav var, rpmluavType type, const void *value)
{
    var->valueType = type;
/*@-assignexpose -branchstate -temptrans @*/
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
/*@=assignexpose =branchstate =temptrans @*/
}

void rpmluavGetKey(rpmluav var, rpmluavType *type, void **value)
{
    *type = var->keyType;
/*@-onlytrans@*/
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
/*@=onlytrans@*/
}

void rpmluavGetValue(rpmluav var, rpmluavType *type, void **value)
{
    *type = var->valueType;
/*@-onlytrans@*/
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
/*@=onlytrans@*/
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
/*@-branchstate@*/
    if (name == NULL)
	name = "<lua>";
/*@=branchstate@*/
    if (luaL_loadbuffer(L, script, strlen(script), name) != 0) {
	rpmError(RPMERR_SCRIPT,
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
/*@-branchstate@*/
    if (name == NULL)
	name = "<lua>";
/*@=branchstate@*/
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

int rpmluaRunScriptFile(rpmlua _lua, const char *filename)
{
    INITSTATE(_lua, lua);
    lua_State *L = lua->L;
    int ret = 0;
    if (luaL_loadfile(L, filename) != 0) {
	rpmError(RPMERR_SCRIPT, _("invalid syntax in lua file: %s\n"),
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
static int rpmluaReadline(lua_State *L, const char *prompt)
	/*@globals fileSystem @*/
	/*@modifies L, fileSystem @*/
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
	/*@globals fileSystem @*/
	/*@modifies L, fileSystem @*/
{
   (void) fputs("\n", stdout);
   printf("RPM Interactive %s Interpreter\n", LUA_VERSION);
   for (;;) {
      int rc = 0;

      if (rpmluaReadline(L, "> ") == 0)
	 break;
      if (lua_tostring(L, -1)[0] == '=') {
/*@-evalorder@*/
	 (void) lua_pushfstring(L, "print(%s)", lua_tostring(L, -1)+1);
/*@=evalorder@*/
	 lua_remove(L, -2);
      }
      for (;;) {
/*@-evalorder@*/
	 rc = luaL_loadbuffer(L, lua_tostring(L, -1),
			      lua_strlen(L, -1), "<lua>");
/*@=evalorder@*/
	 if (rc == LUA_ERRSYNTAX &&
	     strstr(lua_tostring(L, -1), "near `<eof>'") != NULL) {
	    if (rpmluaReadline(L, ">> ") == 0)
	       /*@innerbreak@*/ break;
	    lua_remove(L, -2); /* Remove error */
	    lua_concat(L, 2);
	    /*@innercontinue@*/ continue;
	 }
	 /*@innerbreak@*/ break;
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

/*@-mods@*/
void rpmluaInteractive(rpmlua _lua)
{
    INITSTATE(_lua, lua);
    _rpmluaInteractive(lua->L);
}
/*@=mods@*/

/* ------------------------------------------------------------------ */
/* Lua API */

static int rpm_expand(lua_State *L)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies L, rpmGlobalMacroContext @*/
{
    const char *str = luaL_checkstring(L, 1);
    lua_pushstring(L, rpmExpand(str, NULL));
    return 1;
}

static int rpm_define(lua_State *L)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies L, rpmGlobalMacroContext @*/
{
    const char *str = luaL_checkstring(L, 1);
    (void) rpmDefineMacro(NULL, str, 0);
    return 0;
}

static int rpm_interactive(lua_State *L)
	/*@globals fileSystem @*/
	/*@modifies L, fileSystem @*/
{
    _rpmluaInteractive(L);
    return 0;
}

typedef struct rpmluaHookData_s {
/*@shared@*/
    lua_State *L;
    int funcRef;
    int dataRef;
} * rpmluaHookData;

static int rpmluaHookWrapper(rpmhookArgs args, void *data)
    /*@*/
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
		/*@switchbreak@*/ break;
	    case 'i':
		lua_pushnumber(L, (lua_Number)args->argv[i].i);
		lua_rawseti(L, -2, i+1);
		/*@switchbreak@*/ break;
	    case 'f':
		lua_pushnumber(L, (lua_Number)args->argv[i].f);
		lua_rawseti(L, -2, i+1);
		/*@switchbreak@*/ break;
	    case 'p':
		lua_pushlightuserdata(L, args->argv[i].p);
		lua_rawseti(L, -2, i+1);
		/*@switchbreak@*/ break;
	    default:
                (void) luaL_error(L, "unsupported type '%c' as "
                              "a hook argument\n", args->argt[i]);
		/*@switchbreak@*/ break;
	}
    }
    if (lua_pcall(L, 1, 1, 0) != 0) {
	rpmError(RPMERR_SCRIPT, _("lua hook failed: %s\n"),
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
	/*@globals internalState @*/
	/*@modifies L, internalState @*/
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
/*@-temptrans@*/
	hookdata->L = L;
/*@=temptrans@*/
	rpmhookRegister(lua_tostring(L, 1), rpmluaHookWrapper, hookdata);
	return 1;
    }
    return 0;
}

static int rpm_unregister(lua_State *L)
	/*@modifies L @*/
{
    if (!lua_isstring(L, 1)) {
	(void) luaL_argerror(L, 1, "hook name expected");
    } else if (!lua_islightuserdata(L, 2)) {
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
	/*@globals internalState @*/
	/*@modifies L, internalState @*/
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
		    /*@switchbreak@*/ break;
		case LUA_TNUMBER: {
		    float f = (float)lua_tonumber(L, i+1);
/*@+relaxtypes@*/
		    if (f == (int)f) {
			argt[i] = 'i';
			args->argv[i].i = (int)f;
		    } else {
			argt[i] = 'f';
			args->argv[i].f = f;
		    }
/*@=relaxtypes@*/
		}   /*@switchbreak@*/ break;
		case LUA_TSTRING:
		    argt[i] = 's';
		    args->argv[i].s = lua_tostring(L, i+1);
		    /*@switchbreak@*/ break;
		case LUA_TUSERDATA:
		case LUA_TLIGHTUSERDATA:
		    argt[i] = 'p';
		    args->argv[i].p = lua_touserdata(L, i+1);
		    /*@switchbreak@*/ break;
		default:
		    (void) luaL_error(L, "unsupported Lua type passed to hook");
		    argt[i] = 'p';
		    args->argv[i].p = NULL;
		    /*@switchbreak@*/ break;
	    }
	}
/*@-compdef -kepttrans -usereleased @*/
	args->argt = argt;
	rpmhookCallArgs(name, args);
	free(argt);
	(void) rpmhookArgsFree(args);
/*@=compdef =kepttrans =usereleased @*/
    }
    return 0;
}

/* Based on luaB_print. */
static int rpm_print (lua_State *L)
	/*@globals fileSystem @*/
	/*@modifies L, fileSystem @*/
{
    rpmlua lua = (rpmlua)getdata(L, "lua");
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
		(void) fputs("\t", stdout);
	    (void) fputs(s, stdout);
	}
	lua_pop(L, 1);  /* pop result */
    }
    lua_pop(L, 1);
    if (!lua->storeprint) {
	(void) fputs("\n", stdout);
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

/*@-readonlytrans@*/
/*@observer@*/ /*@unchecked@*/
static const luaL_reg rpmlib[] = {
    {"expand", rpm_expand},
    {"define", rpm_define},
    {"register", rpm_register},
    {"unregister", rpm_unregister},
    {"call", rpm_call},
    {"interactive", rpm_interactive},
    {NULL, NULL}
};
/*@=readonlytrans@*/

static int luaopen_rpm(lua_State *L)
	/*@modifies L @*/
{
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    luaL_openlib(L, "rpm", rpmlib, 0);
    return 0;
}

/*@=bounds =realcompare =sizeoftype =protoparammatch @*/
