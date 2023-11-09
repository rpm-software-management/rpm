#ifndef H_RPMTS_PY
#define H_RPMTS_PY

#include <rpm/rpmts.h>

typedef struct rpmtsObject_s rpmtsObject;
extern PyType_Spec rpmts_Type_Spec;

static inline int rpmtsObject_Check(PyObject *v) {
	rpmmodule_state_t *modstate = rpmModState_FromObject(v);
    if (!modstate) {
        PyErr_Clear();
        return 0;
    }
    return (v)->ob_type == modstate->rpmts_Type;
}

int rpmtsFromPyObject(PyObject *item, rpmts *ts);

/* XXX These names/constants have been removed from the rpmlib API. */
enum {
   RPMDEP_SENSE_REQUIRES,		/*!< requirement not satisfied. */
   RPMDEP_SENSE_CONFLICTS		/*!< conflict was found. */
};

#endif
