#ifndef H_RPMTS_PY
#define H_RPMTS_PY

#include <rpm/rpmts.h>

typedef struct rpmtsObject_s rpmtsObject;

extern PyTypeObject* rpmts_Type;
extern PyType_Spec rpmts_Type_Spec;

#define rpmtsObject_Check(v)	((v)->ob_type == rpmts_Type)

int rpmtsFromPyObject(PyObject *item, rpmts *ts);

/* XXX These names/constants have been removed from the rpmlib API. */
enum {
   RPMDEP_SENSE_REQUIRES,		/*!< requirement not satisfied. */
   RPMDEP_SENSE_CONFLICTS		/*!< conflict was found. */
};

#endif
