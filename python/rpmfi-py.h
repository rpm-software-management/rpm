#ifndef H_RPMFI_PY
#define H_RPMFI_PY

#include <rpm/rpmfi.h>

/** \ingroup py_c
 * \file python/rpmfi-py.h
 */

typedef struct rpmfiObject_s rpmfiObject;

extern PyTypeObject rpmfi_Type;

rpmfi fiFromFi(rpmfiObject * fi);

PyObject * rpmfi_Wrap(rpmfi fi);

PyObject * hdr_fiFromHeader(PyObject * s, PyObject * args, PyObject * kwds);

#endif
