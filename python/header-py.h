#ifndef RPMPYTHON_HEADER
#define RPMPYTHON_HEADER

#include <rpm/rpmtypes.h>

typedef struct hdrObject_s hdrObject;

extern PyTypeObject hdr_Type;

#define hdrObject_Check(v)	((v)->ob_type == &hdr_Type)

extern PyObject * pyrpmError;

PyObject * hdr_Wrap(Header h);

Header hdrGetHeader(hdrObject * h);

rpmTag tagNumFromPyObject (PyObject *item);

PyObject * labelCompare (PyObject * self, PyObject * args);
PyObject * versionCompare (PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmMergeHeadersFromFD(PyObject * self, PyObject * args, PyObject * kwds);
int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag);
PyObject * rpmHeaderFromFile(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmHeaderFromFD(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmSingleHeaderFromFD(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmReadHeaders (FD_t fd);
PyObject * hdrLoad(PyObject * self, PyObject * args, PyObject * kwds);

#endif
