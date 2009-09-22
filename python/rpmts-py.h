#ifndef H_RPMTS_PY
#define H_RPMTS_PY

#include <rpm/rpmts.h>

/** \ingroup py_c
 * \file python/rpmts-py.h
 */

typedef struct rpmtsObject_s rpmtsObject;

extern PyTypeObject rpmts_Type;

/* XXX These names/constants have been removed from the rpmlib API. */
enum {
   RPMDEP_SENSE_REQUIRES,		/*!< requirement not satisfied. */
   RPMDEP_SENSE_CONFLICTS		/*!< conflict was found. */
};

PyObject * rpmts_Create(PyObject * s, PyObject * args, PyObject * kwds);

#endif
