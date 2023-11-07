#ifndef H_RPMPS_PY
#define H_RPMPS_PY

#include <rpm/rpmps.h>

typedef struct rpmProblemObject_s rpmProblemObject;
extern PyType_Spec rpmProblem_Type_Spec;

PyObject * rpmprob_Wrap(PyTypeObject *subtype, rpmProblem prob);

PyObject *rpmps_AsList(rpmmodule_state_t *modstate, rpmps ps);

#endif
