#ifndef H_RPMFI_PY
#define H_RPMFI_PY

#include "rpmfi.h"

/** \ingroup python
 * \file python/rpmfi-py.h
 */

typedef struct rpmfiObject_s {
    PyObject_HEAD
    int active;
/*@null@*/
    rpmfi fi;
} rpmfiObject;

/*@unchecked@*/
extern PyTypeObject rpmfi_Type;

rpmfi fiFromFi(rpmfiObject * fi)
	/*@*/;

rpmfiObject * rpmfi_Wrap(rpmfi fi)
	/*@*/;

rpmfiObject * hdr_fiFromHeader(PyObject * s, PyObject * args)
	/*@*/;

#endif
