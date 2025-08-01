#ifndef RPMPYTHON_HEADER
#define RPMPYTHON_HEADER

#include <rpm/rpmtypes.h>

typedef struct hdrObject_s hdrObject;
extern PyType_Spec hdr_Type_Spec;

static inline int hdrObject_Check(PyObject *v) {
	rpmmodule_state_t *modstate = rpmModState_FromObject(v);
    if (!modstate) {
        PyErr_Clear();
        return 0;
    }
    return (v)->ob_type == modstate->hdr_Type;
}

#define DEPRECATED_METHOD(_msg) \
    PyErr_WarnEx(PyExc_PendingDeprecationWarning, (_msg), 2);

PyObject * hdr_Wrap(PyTypeObject *subtype, Header h);

int hdrFromPyObject(PyObject *item, Header *h);
int utf8FromPyObject(PyObject *item, PyObject **str);
int tagNumFromPyObject (PyObject *item, rpmTagVal *tagp);

PyObject * labelCompare (PyObject * self, PyObject * args);
PyObject * versionCompare (PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmMergeHeadersFromFD(PyObject * self, PyObject * args, PyObject * kwds);
#endif
