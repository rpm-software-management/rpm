/** \ingroup py_c
 * \file python/rpmtd-py.c
 */

#include "rpmsystem-py.h"
#include <rpm/rpmtd.h>
#include <rpm/header.h>
#include "rpmtd-py.h"
#include "header-py.h"

/*
 * Convert single tag data item to python object of suitable type
 */
PyObject * rpmtd_ItemAsPyobj(rpmtd td, rpmTagClass tclass)
{
    PyObject *res = NULL;

    switch (tclass) {
    case RPM_STRING_CLASS:
	res = utf8FromString(rpmtdGetString(td));
	break;
    case RPM_NUMERIC_CLASS:
	res = PyLong_FromLongLong(rpmtdGetNumber(td));
	break;
    case RPM_BINARY_CLASS:
	res = PyBytes_FromStringAndSize(td->data, td->count);
	break;
    default:
	PyErr_SetString(PyExc_KeyError, "unknown data type");
	break;
    }
    return res;
}

PyObject *rpmtd_AsPyobj(rpmtd td)
{
    PyObject *res = NULL;
    int array = (rpmTagGetReturnType(td->tag) == RPM_ARRAY_RETURN_TYPE);
    rpmTagClass tclass = rpmtdClass(td);

    if (!array && rpmtdCount(td) < 1) {
	Py_RETURN_NONE;
    }
    
    if (array) {
	int ix;
	res = PyList_New(rpmtdCount(td));
        if (!res) {
            return NULL;
        }
	while ((ix = rpmtdNext(td)) >= 0) {
	    PyObject *item = rpmtd_ItemAsPyobj(td, tclass);
            if (!item) {
                Py_DECREF(res);
                return NULL;
            }
	    PyList_SET_ITEM(res, ix, item);
	}
    } else {
	res = rpmtd_ItemAsPyobj(td, tclass);
    }
    return res;
}
