#ifndef H_RPMDB_PY
#define H_RPMDB_PY

#include "rpmdb.h"

/** \ingroup python
 * \file python/rpmdb-py.h
 */

/** \ingroup python
 */
typedef struct rpmdbObject_s rpmdbObject;

/** \ingroup python
 */
struct rpmdbObject_s {
    PyObject_HEAD;
    rpmdb db;
    int offx;
    int noffs;
    int *offsets;
} ;

extern PyTypeObject rpmdb_Type;

rpmdb dbFromDb(rpmdbObject * db);

rpmdbObject * rpmOpenDB(PyObject * self, PyObject * args);
PyObject * rebuildDB (PyObject * self, PyObject * args);

#endif
