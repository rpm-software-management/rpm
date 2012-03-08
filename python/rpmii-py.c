#include "rpmsystem-py.h"

#include <rpm/rpmdb.h>

#include "rpmii-py.h"
#include "header-py.h"

/** \ingroup python
 * \class Rpmii
 * \brief A python rpm.ii key iterator object represents the keys of a
 *	database index.
 *
 * The rpm.ii class conains the following methods:
 * - next() -> key		Return the next key.
 *
 * To obtain a rpm.ii object to query the database used by a transaction,
 * the ts.dbIndex(tag) method is used.
 *
 * Here's an example that prints the name of all installed packages:
 * \code
 *	import rpm
 *	ts = rpm.TransactionSet()
 *	for name in ts.dbIndex("conflictname"):
 *	    print name
 * \endcode
 *
 * ts.dbIndex() can be used to get the packages containing the keys of interest
 */

/** \ingroup python
 * \name Class: Rpmii
 */

struct rpmiiObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    PyObject *ref;		/* for db/ts refcounting */
    rpmdbIndexIterator ii;
};

static PyObject *
rpmii_iternext(rpmiiObject * s)
{
    char * key;
    size_t keylen;
    if (s->ii == NULL || (rpmdbIndexIteratorNext(s->ii, (const void**)&key, &keylen)) != 0) {
	s->ii = rpmdbIndexIteratorFree(s->ii);
	return NULL;
    }
    return PyBytes_FromStringAndSize(key, keylen);
};

static PyObject *
rpmii_instances(rpmiiObject * s)
{
    int entries = rpmdbIndexIteratorNumPkgs(s->ii);
    PyObject * list = PyList_New(entries);
    PyObject * tuple;
    for (int i = 0; i < entries; i++) {
        tuple = PyTuple_New(2);
        PyTuple_SET_ITEM(tuple, 0,
                         PyInt_FromLong(rpmdbIndexIteratorPkgOffset(s->ii, i)));
        PyTuple_SET_ITEM(tuple, 1,
                         PyInt_FromLong(rpmdbIndexIteratorTagNum(s->ii, i)));
	PyList_SET_ITEM(list, i, tuple);
    }
    return list;
}

static struct PyMethodDef rpmii_methods[] = {
    {"instances", (PyCFunction) rpmii_instances, METH_NOARGS, NULL},
    {NULL,		NULL}		/* sentinel */
};

static void rpmii_dealloc(rpmiiObject * s)
{
    s->ii = rpmdbIndexIteratorFree(s->ii);
    Py_DECREF(s->ref);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static int rpmii_bool(rpmiiObject *s)
{
    return (s->ii != NULL);
}

static PyNumberMethods rpmii_as_number = {
	0, /* nb_add */
	0, /* nb_subtract */
	0, /* nb_multiply */
	0, /* nb_divide */
	0, /* nb_remainder */
	0, /* nb_divmod */
	0, /* nb_power */
	0, /* nb_negative */
	0, /* nb_positive */
	0, /* nb_absolute */
	(inquiry)rpmii_bool, /* nb_bool/nonzero */
};

static char rpmii_doc[] =
"";

PyTypeObject rpmii_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.ii",			/* tp_name */
	sizeof(rpmiiObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmii_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	&rpmii_as_number,		/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmii_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc) rpmii_iternext,	/* tp_iternext */
	rpmii_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

PyObject * rpmii_Wrap(PyTypeObject *subtype, rpmdbIndexIterator ii, PyObject *s)
{
    rpmiiObject * iio = (rpmiiObject *)subtype->tp_alloc(subtype, 0);
    if (iio == NULL) return NULL;

    iio->ii = ii;
    iio->ref = s;
    Py_INCREF(iio->ref);
    return (PyObject *) iio;
}
