#ifndef H_RPMBC_PY
#define H_RPMBC_PY

#include "rpmio_internal.h"

/** \ingroup python
 * \file python/rpmbc-py.h
 */

/**
 */
typedef struct rpmbcObject_s {
    PyObject_HEAD
    mp32number n;
} rpmbcObject;

/**
 */
/*@unchecked@*/
extern PyTypeObject rpmbc_Type;

#endif
