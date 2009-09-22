#ifndef H_RPMDS_PY
#define H_RPMDS_PY

#include <rpm/rpmds.h>

/** \ingroup py_c
 * \file python/rpmds-py.h
 */

typedef struct rpmdsObject_s rpmdsObject;

/**
 */
extern PyTypeObject rpmds_Type;

/**
 */
rpmds dsFromDs(rpmdsObject * ds);

/**
 */
PyObject * rpmds_Wrap(rpmds ds);

/**
 */
PyObject * rpmds_Single(PyObject * s, PyObject * args, PyObject * kwds);

/**
 */
PyObject * hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds);

/**
 */
PyObject * hdr_dsOfHeader(PyObject * s);

#endif
