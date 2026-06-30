#ifndef LPOSIX_H
#define LPOSIX_H

#include <rpm/rpmutil.h>

void check_deprecated(lua_State *L, const char *func,
		      const char *deprecated_in, const char *removed_in);

int luaopen_posix (lua_State *L);

#endif
