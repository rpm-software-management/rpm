#ifndef RPMLUA_H
#define RPMLUA_H

#include <rpm/argv.h>

typedef struct rpmlua_s * rpmlua;
struct rpmhookArgs_s;

#ifdef __cplusplus
extern "C" {
#endif

/* Upstream Lua headers lack C++ protection, include them all centrally */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#ifdef __cplusplus
}
#endif

typedef char * (*rpmluarl)(const char *);

RPM_PRIVATE_API
rpmlua rpmluaNew(void);
RPM_PRIVATE_API
rpmlua rpmluaFree(rpmlua lua);
RPM_PRIVATE_API
rpmlua rpmluaGetGlobalState(void);
RPM_PRIVATE_API
void *rpmluaGetLua(rpmlua lua);

RPM_PRIVATE_API
int rpmluaCheckScript(rpmlua lua, const char *script,
		      const char *name);
RPM_PRIVATE_API
int rpmluaRunScript(rpmlua lua, const char *script,
		    const char *name, const char *opts, ARGV_t args);
RPM_PRIVATE_API
int rpmluaRunScriptFile(rpmlua lua, const char *filename);
RPM_PRIVATE_API
void rpmluaInteractive(rpmlua lua, rpmluarl rlcb);

RPM_PRIVATE_API
char *rpmluaPopPrintBuffer(rpmlua lua);
RPM_PRIVATE_API
void rpmluaPushPrintBuffer(rpmlua lua);

RPM_PRIVATE_API
void rpmluaPushOutstream(rpmlua lua, FILE *stream);
RPM_PRIVATE_API
void rpmluaPopOutstream(rpmlua lua);

RPM_PRIVATE_API
char *rpmluaCallStringFunction(rpmlua lua, const char *function, struct rpmhookArgs_s *args);

#endif /* RPMLUA_H */
