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

typedef /*@abstract@*/ struct rpmlua_s * rpmlua;
typedef /*@abstract@*/ struct rpmluav_s * rpmluav;

/*@only@*/
rpmlua rpmluaNew(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
void *rpmluaFree(/*@only@*/ rpmlua lua)
	/*@modifies lua @*/;

int rpmluaCheckScript(/*@null@*/ rpmlua lua, const char *script,
		      /*@null@*/ const char *name)
	/*@modifies lua @*/;
int rpmluaRunScript(/*@null@*/ rpmlua lua, const char *script,
		    /*@null@*/ const char *name)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
int rpmluaRunScriptFile(/*@null@*/ rpmlua lua, const char *filename)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;
void rpmluaInteractive(/*@null@*/ rpmlua lua)
	/*@globals fileSystem @*/
	/*@modifies lua, fileSystem @*/;

void rpmluaSetData(/*@null@*/ rpmlua lua, const char *key, const void *data)
	/*@modifies lua @*/;
void *rpmluaGetData(/*@null@*/ rpmlua lua, const char *key)
	/*@modifies lua @*/;

void rpmluaSetPrintBuffer(/*@null@*/ rpmlua lua, int flag)
	/*@modifies lua @*/;
/*@exposed@*/
const char *rpmluaGetPrintBuffer(/*@null@*/ rpmlua lua)
	/*@modifies lua @*/;

void rpmluaSetVar(/*@null@*/ rpmlua lua, rpmluav var)
	/*@modifies lua, var @*/;
void rpmluaGetVar(/*@null@*/ rpmlua lua, rpmluav var)
	/*@modifies lua, var @*/;
void rpmluaDelVar(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@modifies lua @*/;
int rpmluaVarExists(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@modifies lua @*/;
void rpmluaPushTable(/*@null@*/ rpmlua lua, const char *key, ...)
	/*@modifies lua @*/;
void rpmluaPop(/*@null@*/ rpmlua lua)
	/*@modifies lua @*/;

/*@only@*/
rpmluav rpmluavNew(void)
	/*@*/;
void *rpmluavFree(/*@only@*/ rpmluav var)
	/*@modifes var @*/;
void rpmluavSetListMode(rpmluav var, int flag)
	/*@modifies var @*/;
void rpmluavSetKey(rpmluav var, rpmluavType type, const void *value)
	/*@modifies var @*/;
void rpmluavSetValue(rpmluav var, rpmluavType type, const void *value)
	/*@modifies var @*/;
void rpmluavGetKey(rpmluav var, /*@out@*/ rpmluavType *type, /*@out@*/ void **value)
	/*@modifies *type, *value @*/;
void rpmluavGetValue(rpmluav var, /*@out@*/ rpmluavType *type, /*@out@*/ void **value)
	/*@modifies *type, *value @*/;

/* Optional helpers for numbers. */
void rpmluavSetKeyNum(rpmluav var, double value)
	/*@modifies var @*/;
void rpmluavSetValueNum(rpmluav var, double value)
	/*@modifies var @*/;
double rpmluavGetKeyNum(rpmluav var)
	/*@*/;
double rpmluavGetValueNum(rpmluav var)
	/*@*/;
int rpmluavKeyIsNum(rpmluav var)
	/*@*/;
int rpmluavValueIsNum(rpmluav var)
	/*@*/;

#endif /* RPMLUA_H */
