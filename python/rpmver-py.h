#ifndef H_RPMVER_PY
#define H_RPMVER_PY

#include <rpm/rpmtypes.h>

typedef struct rpmverObject_s rpmverObject;

extern PyTypeObject rpmver_Type;

#define verObject_Check(v)	(((PyObject*)v)->ob_type == &rpmver_Type)

int verFromPyObject(PyObject *item, rpmver *ver);
PyObject * rpmver_Wrap(PyTypeObject *subtype, rpmver ver);

#endif
