#ifndef H_RPMMPW_PY
#define H_RPMMPW_PY

#include "rpmio_internal.h"

/** \ingroup py_c  
 * \file python/rpmmpw-py.h
 */

/**
 */
typedef struct mpwObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    mpnumber n;
    mpw data[1];
} mpwObject;

/**
 */
/*@unchecked@*/
extern PyTypeObject mpw_Type;

#define is_mpw(o)	((o)->ob_type == &mpw_Type)

/**
 */
mpwObject * mpw_New(void)
	/*@*/;

#endif
