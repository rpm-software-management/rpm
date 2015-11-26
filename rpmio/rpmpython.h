#ifndef RPMPYTHON_H
#define RPMPYTHON_H

/** \ingroup rpmio
 * \file rpmio/rpmpython.h
 */

#include <rpm/rpmtypes.h>
#include <rpm/rpmsw.h>
#include <rpm/rpmio.h>

typedef struct rpmpython_s * rpmpython;

extern int _rpmpython_debug;

extern rpmpython _rpmpythonI;
#if defined(_RPMPYTHON_INTERNAL)
struct rpmpython_s {
    void * I;			/* (unused) */
};
#endif /* _RPMPYTHON_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Destroy a python interpreter.
 * @param python	python interpreter
 * @return		NULL on last dereference
 */
rpmpython rpmpythonFree(rpmpython python);

/**
 * Create and load a python interpreter.
 * @param av		python interpreter args (or NULL)
 * @return		new python interpreter
 */
rpmpython rpmpythonNew(char ** av);

/**
 * Execute python from a file.
 * @param python	python interpreter (NULL uses global interpreter)
 * @param fn		python file to run (NULL returns RPMRC_FAIL)
 * @param *resultp	python exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmpythonRunFile(rpmpython python, const char * fn,
		char ** resultp);

/**
 * Execute python string.
 * @param python	python interpreter (NULL uses global interpreter)
 * @param str		python string to execute (NULL returns RPMRC_FAIL)
 * @param *resultp	python exec result
 * @return		RPMRC_OK on success
 */
rpmRC rpmpythonRun(rpmpython python, const char * str,
		char ** resultp);

#ifdef __cplusplus
}
#endif

#endif /* RPMPYTHON_H */
