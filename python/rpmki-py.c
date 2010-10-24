#include "rpmsystem-py.h"

#include <rpm/rpmdb.h>

#include "rpmki-py.h"
#include "header-py.h"

#include "debug.h"

/** \ingroup python
 * \class Rpmki
 * \brief A python rpm.ki key iterator object represents the keys of a
 *	database index.
 *
 * The rpm.ki class conains the following methods:
 * - next() -> key		Return the next key.
 *
 * To obtain a rpm.ki object to query the database used by a transaction,
 * the ts.dbKeys(tag) method is used.
 *
 * Here's an example that prints the name of all installed packages:
 * \code
 *	import rpm
 *	ts = rpm.TransactionSet()
 *	for name in ts.dbKeys("conflictname"):
 *	    print name
 * \endcode
 *
 * ts.dbMatch() can be used to get the packages containing the keys of interest
 */

/** \ingroup python
 * \name Class: Rpmki
 */

struct rpmkiObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    PyObject *ref;		/* for db/ts refcounting */
    rpmdbKeyIterator ki;
};

static PyObject *
rpmki_iternext(rpmkiObject * s)
{
    if (s->ki == NULL || (rpmdbKeyIteratorNext(s->ki)) != 0) {
	s->ki = rpmdbKeyIteratorFree(s->ki);
	return NULL;
    }
    return PyString_FromStringAndSize(rpmdbKeyIteratorKey(s->ki),
                                      rpmdbKeyIteratorKeySize(s->ki));
};

static PyObject *
rpmki_offsets(rpmkiObject * s)
{
    int entries = rpmdbKeyIteratorNumPkgs(s->ki);
    PyObject * list = PyList_New(0);
    PyObject * tuple;
    for (int i = 0; i < entries; i++) {
        tuple = PyTuple_New(2);
        PyTuple_SET_ITEM(tuple, 0,
                         PyInt_FromLong(rpmdbKeyIteratorPkgOffset(s->ki, i)));
        PyTuple_SET_ITEM(tuple, 1,
                         PyInt_FromLong(rpmdbKeyIteratorTagNum(s->ki, i)));
        PyList_Append(list, tuple);
    }
    return list;
}

static struct PyMethodDef rpmki_methods[] = {
    {"offsets",    (PyCFunction) rpmki_offsets,       METH_NOARGS,
     NULL },
    {NULL,		NULL}		/* sentinel */
};

static void rpmki_dealloc(rpmkiObject * s)
{
    s->ki = rpmdbKeyIteratorFree(s->ki);
    Py_DECREF(s->ref);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static int rpmki_bool(rpmkiObject *s)
{
    return (s->ki != NULL);
}

static PyNumberMethods rpmki_as_number = {
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
	(inquiry)rpmki_bool, /* nb_bool/nonzero */
};

static char rpmki_doc[] =
"";

PyTypeObject rpmki_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.ki",			/* tp_name */
	sizeof(rpmkiObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmki_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	&rpmki_as_number,		/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmki_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc) rpmki_iternext,	/* tp_iternext */
	rpmki_methods,			/* tp_methods */
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

PyObject * rpmki_Wrap(PyTypeObject *subtype, rpmdbKeyIterator ki, PyObject *s)
{
    rpmkiObject * kio = (rpmkiObject *)subtype->tp_alloc(subtype, 0);
    if (kio == NULL) return NULL;

    kio->ki = ki;
    kio->ref = s;
    Py_INCREF(kio->ref);
    return (PyObject *) kio;
}
