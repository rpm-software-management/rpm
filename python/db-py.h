#ifndef RPMPYTHON_DB
#define RPMPYTHON_DB

/** \ingroup python
 * \file python/db-py.h
 */

/** \ingroup python
 */
struct rpmdbObject_s {
    PyObject_HEAD;
    rpmdb db;
    int offx;
    int noffs;
    int *offsets;
} ;

/** \ingroup python
 */
typedef struct rpmdbObject_s rpmdbObject;

extern PyTypeObject rpmdbType;
PyTypeObject rpmdbMIType;

rpmdb dbFromDb(rpmdbObject * db);
rpmdbObject * rpmOpenDB(PyObject * self, PyObject * args);
PyObject * rebuildDB (PyObject * self, PyObject * args);

#endif
