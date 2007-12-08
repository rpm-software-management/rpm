#ifndef H_RPMDS_PY
#define H_RPMDS_PY

#include <rpm/rpmds.h>

/** \ingroup py_c
 * \file python/rpmds-py.h
 */

/**
 */
typedef struct rpmdsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int		active;
    rpmds	ds;
} rpmdsObject;

/**
 */
extern PyTypeObject rpmds_Type;

/**
 */
rpmds dsFromDs(rpmdsObject * ds);

/**
 */
rpmdsObject * rpmds_Wrap(rpmds ds);

/**
 */
rpmdsObject * rpmds_Single(PyObject * s, PyObject * args, PyObject * kwds);

/**
 */
rpmdsObject * hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds);

/**
 */
rpmdsObject * hdr_dsOfHeader(PyObject * s);

#endif
