#ifndef H_RPMTE_PY
#define H_RPMTE_PY

/** \ingroup python
 * \file python/rpmte-py.h
 */

typedef struct rpmteObject_s {
    PyObject_HEAD
    rpmte	te;
} rpmteObject;

extern PyTypeObject rpmte_Type;

rpmteObject * rpmte_Wrap(rpmte te);

#endif
