#ifndef RPMPYTHON_HEADER
#define RPMPYTHON_HEADER

/** \ingroup py_c
 * \file python/header-py.h
 */

/** \ingroup py_c
 */
typedef struct hdrObject_s hdrObject;

/*@unchecked@*/
extern PyTypeObject hdr_Type;

/** \ingroup py_c
 */
PyObject * pyrpmError;

hdrObject * hdr_Wrap(Header h)
	/*@*/;

Header hdrGetHeader(hdrObject * h)
	/*@*/;

long tagNumFromPyObject (PyObject *item)
	/*@*/;

PyObject * labelCompare (PyObject * self, PyObject * args)
	/*@*/;
PyObject * versionCompare (PyObject * self, PyObject * args)
	/*@*/;
PyObject * rpmMergeHeadersFromFD(PyObject * self, PyObject * args)
	/*@*/;
int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag)
	/*@*/;
PyObject * rpmHeaderFromFile(PyObject * self, PyObject * args)
	/*@*/;
PyObject * rpmHeaderFromFD(PyObject * self, PyObject * args)
	/*@*/;
PyObject * rpmSingleHeaderFromFD(PyObject * self, PyObject * args)
	/*@*/;
PyObject * rpmReadHeaders (FD_t fd)
	/*@*/;
PyObject * rhnLoad(PyObject * self, PyObject * args)
	/*@*/;
PyObject * hdrLoad(PyObject * self, PyObject * args)
	/*@*/;

#endif
