#ifndef RPMLUA_H
#define RPMLUA_H

#ifdef WITH_LUA
#include <lauxlib.h>
#endif

typedef enum rpmluavType_e {
    RPMLUAV_NIL		= 0,
    RPMLUAV_STRING	= 1,
    RPMLUAV_NUMBER	= 2
} rpmluavType;


typedef struct rpmlua_s * rpmlua;
typedef struct rpmluav_s * rpmluav;

#ifdef __cplusplus
extern "C" {
#endif

rpmlua rpmluaNew(void);
rpmlua rpmluaFree(rpmlua lua);
rpmlua rpmluaGetGlobalState(void);

#ifdef WITH_LUA
void rpmluaRegister(rpmlua lua, const luaL_Reg *funcs, const char *lib);
#endif

int rpmluaCheckScript(rpmlua lua, const char *script,
		      const char *name);
int rpmluaRunScript(rpmlua lua, const char *script,
		    const char *name);
int rpmluaRunScriptFile(rpmlua lua, const char *filename);
void rpmluaInteractive(rpmlua lua);

void *rpmluaGetData(rpmlua lua, const char *key);
void rpmluaSetData(rpmlua lua, const char *key, const void *data);

char *rpmluaPopPrintBuffer(rpmlua lua);
void rpmluaPushPrintBuffer(rpmlua lua);

void rpmluaSetNextFileFunc(char *(*func)(void *), void *funcParam);

void rpmluaGetVar(rpmlua lua, rpmluav var);
void rpmluaSetVar(rpmlua lua, rpmluav var);
void rpmluaDelVar(rpmlua lua, const char *key, ...);
int rpmluaVarExists(rpmlua lua, const char *key, ...);
void rpmluaPushTable(rpmlua lua, const char *key, ...);
void rpmluaPop(rpmlua lua);

rpmluav rpmluavNew(void);
rpmluav rpmluavFree(rpmluav var);
void rpmluavSetListMode(rpmluav var, int flag);
void rpmluavSetKey(rpmluav var, rpmluavType type, const void *value);
void rpmluavSetValue(rpmluav var, rpmluavType type, const void *value);
void rpmluavGetKey(rpmluav var, rpmluavType *type, void **value);
void rpmluavGetValue(rpmluav var, rpmluavType *type, void **value);

/* Optional helpers for numbers. */
void rpmluavSetKeyNum(rpmluav var, double value);
void rpmluavSetValueNum(rpmluav var, double value);
double rpmluavGetKeyNum(rpmluav var);
double rpmluavGetValueNum(rpmluav var);
int rpmluavKeyIsNum(rpmluav var);
int rpmluavValueIsNum(rpmluav var);

#ifdef __cplusplus
}
#endif

#endif /* RPMLUA_H */
