#ifndef RPMPYTHON_DB
#define RPMPYTHON_DB

/** \ingroup python
 * \file python/db-py.h
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

/** \ingroup python
 */
typedef struct rpmmiObject_s rpmmiObject;

/** \ingroup python
 */
struct rpmmiObject_s {
    PyObject_HEAD;
    rpmdbMatchIterator mi;
    rpmdbObject *db;
} ;

extern PyTypeObject rpmmi_Type;

rpmdb dbFromDb(rpmdbObject * db);

rpmdbObject * rpmOpenDB(PyObject * self, PyObject * args);
PyObject * rebuildDB (PyObject * self, PyObject * args);

#endif
