#ifndef RPMLUA_H
#define RPMLUA_H

#if defined(_RPMLUA_INTERNAL)

#include "rpmts.h"

#include <lua.h>

struct rpmlua_s {
    lua_State *L;
    rpmts ts;
};

#endif /* _RPMLUA_INTERNAL */

typedef struct rpmlua_s * rpmlua;

rpmlua rpmluaNew(void);
void *rpmluaFree(rpmlua lua);
void rpmluaSetTS(rpmlua lua, rpmts ts);
rpmRC rpmluaCheckScript(rpmlua lua, const char *script, const char *name);
rpmRC rpmluaRunScript(rpmlua lua, Header h, const char *sln,
		      int progArgc, const char **progArgv,
		      const char *script, int arg1, int arg2);
void rpmluaInteractive(rpmlua lua);

#endif /* RPMLUA_H */

/* vim:sts=4:sw=4
*/
