#ifndef H_RPMFD_PY
#define H_RPMFD_PY

#include <rpm/rpmio.h>

typedef struct rpmfdObject_s rpmfdObject;
extern PyType_Spec rpmfd_Type_Spec;

static inline int rpmfdObject_Check(PyObject *v) {
	rpmmodule_state_t *modstate = rpmModState_FromObject(v);
    if (!modstate) {
        PyErr_Clear();
        return 0;
    }
    return (v)->ob_type == modstate->rpmfd_Type;
}

FD_t rpmfdGetFd(rpmfdObject *fdo);

int rpmfdFromPyObject(rpmmodule_state_t *modstate, PyObject *obj, rpmfdObject **fdop);


#endif
