#ifndef H_RPMMPW_PY
#define H_RPMMPW_PY

#include "rpmio_internal.h"

/** \ingroup py_c  
 * \file python/rpmbc-py.h
 */

/**
 */
typedef struct mpwObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    mpnumber n;
} mpwObject;

/**
 */
/*@unchecked@*/
extern PyTypeObject mpw_Type;

#endif
