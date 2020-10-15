#ifndef RPMLUA_H
#define RPMLUA_H

#include <rpm/argv.h>

typedef struct rpmlua_s * rpmlua;

#ifdef __cplusplus
extern "C" {
#endif

rpmlua rpmluaNew(void);
rpmlua rpmluaFree(rpmlua lua);
rpmlua rpmluaGetGlobalState(void);
void *rpmluaGetLua(rpmlua lua);

void rpmluaRegister(rpmlua lua, const void *regfuncs, const char *lib);

int rpmluaCheckScript(rpmlua lua, const char *script,
		      const char *name);
int rpmluaRunScript(rpmlua lua, const char *script,
		    const char *name, const char *opts, ARGV_t args);
int rpmluaRunScriptFile(rpmlua lua, const char *filename);
void rpmluaInteractive(rpmlua lua);

void *rpmluaGetData(rpmlua lua, const char *key);
void rpmluaSetData(rpmlua lua, const char *key, const void *data);

char *rpmluaPopPrintBuffer(rpmlua lua);
void rpmluaPushPrintBuffer(rpmlua lua);

void rpmluaSetNextFileFunc(char *(*func)(void *), void *funcParam);

#ifdef __cplusplus
}
#endif

#endif /* RPMLUA_H */
