#ifndef H_RPMDS_PY
#define H_RPMDS_PY

#include <rpm/rpmds.h>

typedef struct rpmdsObject_s rpmdsObject;

extern PyTypeObject rpmds_Type;

#define rpmdsObject_Check(v)	((v)->ob_type == &rpmds_Type)

rpmds dsFromDs(rpmdsObject * ds);

PyObject * rpmds_Wrap(PyTypeObject *subtype, rpmds ds);

#endif
