#ifndef H_RPMDS_PY
#define H_RPMDS_PY

/** \ingroup python
 * \file python/rpmds-py.h
 */

typedef struct rpmdsObject_s {
    PyObject_HEAD
    rpmds	ds;
} rpmdsObject;

extern PyTypeObject rpmds_Type;

rpmds dsFromDs(rpmdsObject * ds);

rpmdsObject * rpmds_New(rpmds ds);

rpmdsObject * hdr_dsFromHeader(PyObject * s, PyObject * args);

#endif
