#ifndef RPMPYTHON_HEADER
#define RPMPYTHON_HEADER

#include <rpm/rpmtypes.h>

typedef struct hdrObject_s hdrObject;

extern PyTypeObject hdr_Type;

#define hdrObject_Check(v)	((v)->ob_type == &hdr_Type)

#define DEPRECATED_METHOD(_msg) \
    PyErr_WarnEx(PyExc_PendingDeprecationWarning, (_msg), 2);

extern PyObject * pyrpmError;

PyObject * hdr_Wrap(PyTypeObject *subtype, Header h);

int hdrFromPyObject(PyObject *item, Header *h);

int tagNumFromPyObject (PyObject *item, rpmTag *tagp);

PyObject * labelCompare (PyObject * self, PyObject * args);
PyObject * versionCompare (PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmMergeHeadersFromFD(PyObject * self, PyObject * args, PyObject * kwds);
int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag);
PyObject * rpmSingleHeaderFromFD(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * hdrLoad(PyObject * self, PyObject * args, PyObject * kwds);

#endif
