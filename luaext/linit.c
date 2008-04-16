#include "lua.h"
#include "lauxlib.h"
#include "linit.h"

LUA_API int luaopen_init(lua_State *L)
{
#include "linit.lch"
    return 0;
}
