
#include "config.h"

#include "lposix.h"
#include "lrexlib.h"

#define LUA_EXTRALIBS \
	{"posix", luaopen_posix}, \
	{"rex", luaopen_rex}, \
	{"luapath", luapath},

#if 0

#define lua_readline	myreadline
#define lua_saveline	mysaveline

#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

static int myreadline (lua_State *L, const char *prompt) {
  char *s=readline(prompt);
  if (s==NULL)
    return 0;
  else {
    lua_pushstring(L,s);
    lua_pushliteral(L,"\n");
    lua_concat(L,2);
    free(s);
    return 1;
  }
}

static void mysaveline (lua_State *L, const char *s) {
  const char *p;
  for (p=s; isspace(*p); p++)
    ;
  if (*p!=0) {
    size_t n=strlen(s)-1;
    if (s[n]!='\n')
      add_history(s);
    else {
      lua_pushlstring(L,s,n);
      s=lua_tostring(L,-1);
      add_history(s);
      lua_remove(L,-1);
    }
  }
}
#endif

static int luapath(lua_State *L)
{
	lua_pushstring(L, "LUA_PATH");
	lua_pushstring(L, RPMCONFIGDIR "/lua/?.lua;?.lua");
	lua_rawset(L, LUA_GLOBALSINDEX);
	return 0;
}
