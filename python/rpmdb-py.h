#ifndef H_RPMDB_PY
#define H_RPMDB_PY

#include <rpm/rpmdb.h>

/** \ingroup py_c
 * \file python/rpmdb-py.h
 */

/** \ingroup py_c
 */
typedef struct rpmdbObject_s rpmdbObject;

/** \ingroup py_c
 */
struct rpmdbObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmdb db;
    int offx;
    int noffs;
    int *offsets;
} ;

extern PyTypeObject rpmdb_Type;

#endif
