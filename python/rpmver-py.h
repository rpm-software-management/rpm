#ifndef H_RPMVER_PY
#define H_RPMVER_PY

#include <rpm/rpmtypes.h>

typedef struct rpmverObject_s rpmverObject;
extern PyType_Spec rpmver_Type_Spec;

static inline int verObject_Check(PyObject *v) {
	rpmmodule_state_t *modstate = rpmModState_FromObject(v);
    if (!modstate) {
	PyErr_Clear();
	return 0;
    }
    return PyObject_TypeCheck(v, modstate->rpmver_Type);
}

int verFromPyObject(PyObject *item, rpmver *ver);
PyObject * rpmver_Wrap(PyTypeObject *subtype, rpmver ver);

#endif
