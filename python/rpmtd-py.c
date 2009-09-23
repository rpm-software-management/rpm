/** \ingroup py_c
 * \file python/rpmtd-py.c
 */

#include "rpmsystem-py.h"
#include <rpm/rpmtd.h>
#include "rpmtd-py.h"

/*
 * Convert single tag data item to python object of suitable type
 */
static PyObject * rpmtd_ItemAsPyobj(rpmtd td, rpmTagClass class)
{
    PyObject *res = NULL;
    char *str = NULL;
    const char *errmsg = NULL;

    switch (class) {
    case RPM_STRING_CLASS:
	res = PyString_FromString(rpmtdGetString(td));
	break;
    case RPM_NUMERIC_CLASS:
	res = PyLong_FromLongLong(rpmtdGetNumber(td));
	break;
    case RPM_BINARY_CLASS:
	str = rpmtdFormat(td, RPMTD_FORMAT_STRING, &errmsg);
	if (errmsg) {
	    PyErr_SetString(PyExc_ValueError, errmsg);
	} else {
	    res = PyString_FromString(str);
	} 
	free(str);
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
    rpmTagType type = rpmTagGetType(td->tag);
    int array = ((type & RPM_MASK_RETURN_TYPE) == RPM_ARRAY_RETURN_TYPE);
    rpmTagClass class = rpmtdClass(td);

    if (!array && rpmtdCount(td) < 1) {
	Py_RETURN_NONE;
    }
    
    if (array) {
	res = PyList_New(0);
	while (rpmtdNext(td) >= 0) {
	    PyList_Append(res, rpmtd_ItemAsPyobj(td, class));
	}
    } else {
	res = rpmtd_ItemAsPyobj(td, class);
    }
    return res;
}
