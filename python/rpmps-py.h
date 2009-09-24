#ifndef H_RPMPS_PY
#define H_RPMPS_PY

#include <rpm/rpmps.h>

typedef struct rpmpsObject_s rpmpsObject;

extern PyTypeObject rpmps_Type;

#define rpmpsObject_Check(v)	((v)->ob_type == &rpmps_Type)

rpmps psFromPs(rpmpsObject * ps);

PyObject * rpmps_Wrap(PyTypeObject *subtype, rpmps ps);

#endif
