#ifndef H_RPMTS_PY
#define H_RPMTS_PY

#include "rpmts.h"

/** \ingroup python
 * \file python/rpmts-py.h
 */

typedef struct rpmtsObject_s {
    PyObject_HEAD
    rpmts	ts;
    PyObject * keyList;		/* keeps reference counts correct */
    FD_t scriptFd;
    rpmtsi tsi;
    rpmElementType tsiFilter;
    rpmprobFilterFlags ignoreSet;
} rpmtsObject;

/*@unchecked@*/
extern PyTypeObject rpmts_Type;

/* XXX These names/constants have been removed from the rpmlib API. */
enum {
   RPMDEP_SENSE_REQUIRES,		/*!< requirement not satisfied. */
   RPMDEP_SENSE_CONFLICTS		/*!< conflict was found. */
};

rpmtsObject * rpmts_Create(PyObject * s, PyObject * args)
	/*@*/;

#endif
