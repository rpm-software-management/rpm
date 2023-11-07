#ifndef H_RPMDS_PY
#define H_RPMDS_PY

#include <rpm/rpmds.h>

typedef struct rpmdsObject_s rpmdsObject;
extern PyType_Spec rpmds_Type_Spec;

rpmds dsFromDs(rpmdsObject * ds);

PyObject * rpmds_Wrap(PyTypeObject *subtype, rpmds ds);

#endif
