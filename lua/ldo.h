/*
** $Id: ldo.h,v 1.2 2004/03/23 05:09:14 jbj Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** macro to control inclusion of some hard tests on stack reallocation
*/ 
#ifndef HARDSTACKTESTS
#define condhardstacktests(x)	{ /* empty */ }
#else
#define condhardstacktests(x)	x
#endif


#define luaD_checkstack(L,n)	\
  if ((char *)L->stack_last - (char *)L->top <= (n)*(int)sizeof(TObject)) \
    luaD_growstack(L, n); \
  else condhardstacktests(luaD_reallocstack(L, L->stacksize));


#define incr_top(L) {luaD_checkstack(L,1); L->top++;}

#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((TObject *)((char *)L->stack + (n)))

#define saveci(L,p)		((char *)(p) - (char *)L->base_ci)
#define restoreci(L,n)		((CallInfo *)((char *)L->base_ci + (n)))


/* type of protected functions, to be ran by `runprotected' */
typedef void (*Pfunc) (lua_State *L, void *ud);

/*@unused@*/
void luaD_resetprotection (lua_State *L)
	/*@modifies L @*/;
int luaD_protectedparser (lua_State *L, ZIO *z, int bin)
	/*@modifies L, z @*/;
void luaD_callhook (lua_State *L, int event, int line)
	/*@modifies L @*/;
/*@null@*/
StkId luaD_precall (lua_State *L, StkId func)
	/*@modifies L @*/;
void luaD_call (lua_State *L, StkId func, int nResults)
	/*@modifies L @*/;
int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t oldtop, ptrdiff_t ef)
	/*@modifies L @*/;
void luaD_poscall (lua_State *L, int wanted, StkId firstResult)
	/*@modifies L @*/;
void luaD_reallocCI (lua_State *L, int newsize)
	/*@modifies L @*/;
void luaD_reallocstack (lua_State *L, int newsize)
	/*@modifies L @*/;
void luaD_growstack (lua_State *L, int n)
	/*@modifies L @*/;

void luaD_throw (lua_State *L, int errcode)
	/*@modifies L @*/;
int luaD_rawrunprotected (lua_State *L, Pfunc f, /*@null@*/ void *ud)
	/*@modifies L @*/;


#endif
