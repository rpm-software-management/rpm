#ifndef RPMLUA_H
#define RPMLUA_H

typedef enum rpmluavType_e {
    RPMLUAV_NIL		= 0,
    RPMLUAV_STRING	= 1,
    RPMLUAV_NUMBER	= 2
} rpmluavType;

#if defined(_RPMLUA_INTERNAL)

#include <stdarg.h>
#include <lua.h>

struct rpmlua_s {
    lua_State *L;
    int pushsize;
    int storeprint;
    int printbufsize;
    int printbufused;
    char *printbuf;
};

struct rpmluav_s {
    rpmluavType keyType;
    rpmluavType valueType;
    union {
	const char *str;
	const void *ptr;
	double num;
    } key;
    union {
	const char *str;
	const void *ptr;
	double num;
    } value;
    int listmode;
};

#endif /* _RPMLUA_INTERNAL */

typedef struct rpmlua_s * rpmlua;
typedef struct rpmluav_s * rpmluav;

rpmlua rpmluaNew(void);
void *rpmluaFree(rpmlua lua);

int rpmluaCheckScript(rpmlua lua, const char *script, const char *name);
int rpmluaRunScript(rpmlua lua, const char *script, const char *name);
void rpmluaInteractive(rpmlua lua);

void rpmluaSetData(rpmlua lua, const char *key, const void *data);
void *rpmluaGetData(rpmlua lua, const char *key);

void rpmluaSetPrintBuffer(rpmlua lua, int flag);
const char *rpmluaGetPrintBuffer(rpmlua lua);

void rpmluaSetVar(rpmlua lua, rpmluav var);
void rpmluaGetVar(rpmlua lua, rpmluav var);
void rpmluaDelVar(rpmlua lua, const char *key, ...);
int rpmluaVarExists(rpmlua lua, const char *key, ...);
void rpmluaPushTable(rpmlua lua, const char *key, ...);
void rpmluaPop(rpmlua lua);

rpmluav rpmluavNew(void);
void *rpmluavFree(rpmluav var);
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

#endif /* RPMLUA_H */

/* vim:sts=4:sw=4
*/
