#ifndef H_RPMTE_PY
#define H_RPMTE_PY

#include <rpm/rpmte.h>

typedef struct rpmteObject_s rpmteObject;
extern PyType_Spec rpmte_Type_Spec;

PyObject * rpmte_Wrap(PyTypeObject *subtype, rpmte te);

#endif
