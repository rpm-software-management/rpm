#include "lua.h"
#include "lauxlib.h"

LUA_API int luaopen_init(lua_State *L)
{
#include "linit.lch"
}
