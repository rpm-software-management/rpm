/*
** $Id: ltable.h,v 1.2 2004/03/23 05:09:14 jbj Exp $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gkey(n)		(&(n)->i_key)
#define gval(n)		(&(n)->i_val)


/*@observer@*/
const TObject *luaH_getnum (Table *t, int key)
	/*@*/;
TObject *luaH_setnum (lua_State *L, Table *t, int key)
	/*@modifies L, t @*/;
/*@observer@*/
const TObject *luaH_getstr (Table *t, TString *key)
	/*@*/;
/*@observer@*/
const TObject *luaH_get (Table *t, const TObject *key)
	/*@*/;
TObject *luaH_set (lua_State *L, Table *t, const TObject *key)
	/*@modifies L, t @*/;
/*@null@*/
Table *luaH_new (lua_State *L, int narray, int lnhash)
	/*@modifies L @*/;
void luaH_free (lua_State *L, Table *t)
	/*@modifies L, t @*/;
int luaH_next (lua_State *L, Table *t, StkId key)
	/*@modifies L, key @*/;

/* exported only for debugging */
Node *luaH_mainposition (const Table *t, const TObject *key)
	/*@*/;


#endif
