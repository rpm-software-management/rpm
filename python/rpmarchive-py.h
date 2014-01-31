#ifndef H_RPMARCHIVE_PY
#define H_RPMARCHIVE_PY

#include <rpm/rpmarchive.h>

typedef struct rpmarchiveObject_s rpmarchiveObject;

extern PyTypeObject rpmarchive_Type;

#define rpmarchiveObject_Check(v)	((v)->ob_type == &rpmarchive_Type)

PyObject * rpmarchive_Wrap(PyTypeObject *subtype,
			   rpmfiles files, rpmfi archive);

#endif
