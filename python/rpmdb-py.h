#ifndef H_RPMDB_PY
#define H_RPMDB_PY

#include "rpmdb.h"

/** \ingroup py_c  
 * \file python/rpmdb-py.h
 */

/** \ingroup py_c  
 */
typedef struct rpmdbObject_s rpmdbObject;

/** \ingroup py_c  
 */
struct rpmdbObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmdb db;
    int offx;
    int noffs;
    int *offsets;
} ;

/*@unchecked@*/
extern PyTypeObject rpmdb_Type;

#ifdef  _LEGACY_BINDINGS_TOO
rpmdb dbFromDb(rpmdbObject * db)
	/*@*/;

rpmdbObject * rpmOpenDB(PyObject * self, PyObject * args)
	/*@*/;
PyObject * rebuildDB (PyObject * self, PyObject * args)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/;
#endif

#endif
