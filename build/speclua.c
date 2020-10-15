
#include "system.h"

#include <lua.h>

#include "rpmio/rpmlua.h"
#include "build/rpmbuild_internal.h"

#include "debug.h"

static const char * luavars[] = { "patches", "sources",
			       "patch_nums", "source_nums", NULL, };

void * specLuaInit(rpmSpec spec)
{
    rpmlua lua = rpmluaGetGlobalState();
    for (const char **vp = luavars; vp && *vp; vp++) {
	rpmluaDelVar(lua, *vp);
	rpmluaPushTable(lua, *vp);
	rpmluaPop(lua);
    }

    return lua;
}

void * specLuaFini(rpmSpec spec)
{
    rpmlua lua = spec->lua;
    for (const char **vp = luavars; vp && *vp; vp++) {
	rpmluaDelVar(lua, *vp);
    }
    return NULL;
}

void addLuaSource(rpmlua lua, const struct Source *p)
{
    const char * what = (p->flags & RPMBUILD_ISPATCH) ? "patches" : "sources";
    rpmluaPushTable(lua, what);
    rpmluav var = rpmluavNew();
    rpmluavSetListMode(var, 1);
    rpmluavSetValue(var, RPMLUAV_STRING, p->path);
    rpmluaSetVar(lua, var);
    rpmluavFree(var);
    rpmluaPop(lua);

    what = (p->flags & RPMBUILD_ISPATCH) ? "patch_nums" : "source_nums";
    rpmluaPushTable(lua, what);
    var = rpmluavNew();
    rpmluavSetListMode(var, 1);
    rpmluavSetValueNum(var, p->num);
    rpmluaSetVar(lua, var);
    rpmluavFree(var);
    rpmluaPop(lua);
}

