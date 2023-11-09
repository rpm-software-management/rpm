#ifndef H_RPMARCHIVE_PY
#define H_RPMARCHIVE_PY

#include <rpm/rpmarchive.h>

typedef struct rpmarchiveObject_s rpmarchiveObject;
extern PyType_Spec rpmarchive_Type_Spec;

PyObject * rpmarchive_Wrap(PyTypeObject *subtype,
			   rpmfiles files, rpmfi archive);

#endif
