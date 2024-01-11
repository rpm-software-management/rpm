#ifndef H_RPMVER_PY
#define H_RPMVER_PY

#include <rpm/rpmtypes.h>

typedef struct rpmverObject_s rpmverObject;

extern PyTypeObject* rpmver_Type;
extern PyType_Spec rpmver_Type_Spec;

#define verObject_Check(v)	PyObject_TypeCheck(v, rpmver_Type)

int verFromPyObject(PyObject *item, rpmver *ver);
PyObject * rpmver_Wrap(PyTypeObject *subtype, rpmver ver);

#endif
