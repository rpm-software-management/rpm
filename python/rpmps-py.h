#ifndef H_RPMPS_PY
#define H_RPMPS_PY

#include "rpmps.h"

/** \ingroup py_c
 * \file python/rpmps-py.h
 */

/**
 */
typedef struct rpmpsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int		active;
    int		ix;
/*@relnull@*/
    rpmps	ps;
} rpmpsObject;

/**
 */
/*@unchecked@*/
extern PyTypeObject rpmps_Type;

/**
 */
/*@null@*/
rpmps psFromPs(rpmpsObject * ps)
	/*@*/;

/**
 */
/*@null@*/
rpmpsObject * rpmps_Wrap(rpmps ps)
	/*@*/;

#endif
