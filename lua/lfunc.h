/*
** $Id: lfunc.h,v 1.2 2004/03/23 05:09:14 jbj Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


/*@null@*/
Proto *luaF_newproto (lua_State *L)
	/*@modifies L @*/;
/*@null@*/
Closure *luaF_newCclosure (lua_State *L, int nelems)
	/*@modifies L @*/;
/*@null@*/
Closure *luaF_newLclosure (lua_State *L, int nelems, TObject *e)
	/*@modifies L @*/;
/*@null@*/
UpVal *luaF_findupval (lua_State *L, StkId level)
	/*@modifies L @*/;
void luaF_close (lua_State *L, StkId level)
	/*@modifies L @*/;
void luaF_freeproto (lua_State *L, Proto *f)
	/*@modifies L, f @*/;
void luaF_freeclosure (lua_State *L, Closure *c)
	/*@modifies L, c @*/;

/*@observer@*/ /*@null@*/
const char *luaF_getlocalname (const Proto *func, int local_number, int pc)
	/*@*/;


#endif
