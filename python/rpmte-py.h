#ifndef H_RPMTE_PY
#define H_RPMTE_PY

#include <rpm/rpmte.h>

typedef struct rpmteObject_s rpmteObject;

extern PyTypeObject* rpmte_Type;
extern PyType_Spec rpmte_Type_Spec;

#define rpmteObject_Check(v)	((v)->ob_type == rpmte_Type)

PyObject * rpmte_Wrap(PyTypeObject *subtype, rpmte te);

#endif
