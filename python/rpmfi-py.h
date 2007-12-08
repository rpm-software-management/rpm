#ifndef H_RPMFI_PY
#define H_RPMFI_PY

#include <rpm/rpmfi.h>

/** \ingroup py_c
 * \file python/rpmfi-py.h
 */

typedef struct rpmfiObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int active;
    rpmfi fi;
} rpmfiObject;

extern PyTypeObject rpmfi_Type;

rpmfi fiFromFi(rpmfiObject * fi);

rpmfiObject * rpmfi_Wrap(rpmfi fi);

rpmfiObject * hdr_fiFromHeader(PyObject * s, PyObject * args, PyObject * kwds);

#endif
