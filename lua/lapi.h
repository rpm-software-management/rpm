/*
** $Id: lapi.h,v 1.2 2004/03/23 05:09:14 jbj Exp $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


void luaA_pushobject (lua_State *L, const TObject *o)
	/*@modifies L @*/;

#endif
