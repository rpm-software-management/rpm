#ifndef H_RPMFI_PY
#define H_RPMFI_PY

#include "rpmfi.h"

/** \ingroup py_c
 * \file python/rpmfi-py.h
 */

typedef struct rpmfiObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int active;
/*@null@*/
    rpmfi fi;
} rpmfiObject;

/*@unchecked@*/
extern PyTypeObject rpmfi_Type;

/*@null@*/
rpmfi fiFromFi(rpmfiObject * fi)
	/*@*/;

/*@null@*/
rpmfiObject * rpmfi_Wrap(rpmfi fi)
	/*@*/;

/*@null@*/
rpmfiObject * hdr_fiFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/;

#endif
