#ifndef H_RPMPS_PY
#define H_RPMPS_PY

#include <rpm/rpmps.h>

typedef struct rpmProblemObject_s rpmProblemObject;

extern PyTypeObject* rpmProblem_Type;
extern PyType_Spec rpmProblem_Type_Spec;

#define rpmProblemObject_Check(v)	((v)->ob_type == rpmProblem_Type)

PyObject * rpmprob_Wrap(PyTypeObject *subtype, rpmProblem prob);

PyObject *rpmps_AsList(rpmps ps);

#endif
