/*
** $Id: lvm.h,v 1.2 2004/03/23 05:09:14 jbj Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#define tostring(L,o) ((ttype(o) == LUA_TSTRING) || (luaV_tostring(L, o)))

#define tonumber(o,n)	(ttype(o) == LUA_TNUMBER || \
                         (((o) = luaV_tonumber(o,n)) != NULL))

#define equalobj(L,o1,o2) \
	(ttype(o1) == ttype(o2) && luaV_equalval(L, o1, o2))


int luaV_lessthan (lua_State *L, const TObject *l, const TObject *r)
	/*@modifies L @*/;
int luaV_equalval (lua_State *L, const TObject *t1, const TObject *t2)
	/*@modifies L, t1, t2 @*/;
/*@observer@*/ /*@null@*/
const TObject *luaV_tonumber (const TObject *obj, TObject *n)
	/*@modifies n @*/;
int luaV_tostring (lua_State *L, StkId obj)
	/*@modifies L, obj @*/;
/*@observer@*/
const TObject *luaV_gettable (lua_State *L, const TObject *t, TObject *key,
                              int loop)
	/*@modifies L, t @*/;
void luaV_settable (lua_State *L, const TObject *t, TObject *key, StkId val)
	/*@modifies L, t @*/;
/*@null@*/
StkId luaV_execute (lua_State *L)
	/*@modifies L @*/;
void luaV_concat (lua_State *L, int total, int last)
	/*@modifies L @*/;

#endif
