#ifndef H_RPMFILES_PY
#define H_RPMFILES_PY

#include <rpm/rpmfiles.h>

typedef struct rpmfileObject_s rpmfileObject;
typedef struct rpmfilesObject_s rpmfilesObject;
extern PyType_Spec rpmfile_Type_Spec;
extern PyType_Spec rpmfiles_Type_Spec;

static inline int rpmfileObject_Check(PyObject *v) {
	rpmmodule_state_t *modstate = rpmModState_FromObject(v);
    if (!modstate) {
        PyErr_Clear();
        return 0;
    }
    return (v)->ob_type == modstate->rpmfile_Type;
}

static inline int rpmfilesObject_Check(PyObject *v) {
	rpmmodule_state_t *modstate = rpmModState_FromObject(v);
    if (!modstate) {
        PyErr_Clear();
        return 0;
    }
    return (v)->ob_type == modstate->rpmfiles_Type;
}

PyObject * rpmfile_Wrap(rpmmodule_state_t *modstate, rpmfiles files, int ix);
PyObject * rpmfiles_Wrap(PyTypeObject *subtype, rpmfiles files);

#endif
