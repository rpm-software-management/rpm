#ifndef H_RPMDS_PY
#define H_RPMDS_PY

#include "rpmds.h"

/** \ingroup py_c
 * \file python/rpmds-py.h
 */

/**
 */
typedef struct rpmdsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int		active;
/*@null@*/
    rpmds	ds;
} rpmdsObject;

/**
 */
/*@unchecked@*/
extern PyTypeObject rpmds_Type;

/**
 */
/*@null@*/
rpmds dsFromDs(rpmdsObject * ds)
	/*@*/;

/**
 */
/*@null@*/
rpmdsObject * rpmds_Wrap(rpmds ds)
	/*@*/;

/**
 */
/*@null@*/
rpmdsObject * rpmds_Single(PyObject * s, PyObject * args, PyObject * kwds)
	/*@*/;

/**
 */
/*@null@*/
rpmdsObject * hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
	/*@*/;

/**
 */
/*@null@*/
rpmdsObject * hdr_dsOfHeader(PyObject * s)
	/*@*/;

#endif
