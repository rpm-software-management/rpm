#include "rpmsystem-py.h"

#include <rpm/rpmdb.h>
#include <rpm/rpmtd.h>

#include "rpmtd-py.h"
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
 *	    print(name)
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
    rpmtd keytd;
};

static PyObject *
rpmii_iternext(rpmiiObject * s)
{
    PyObject *keyo = NULL;

    if (s->ii != NULL) {
	if (rpmdbIndexIteratorNextTd(s->ii, s->keytd) == 0) {
	    /* The keys must never be arrays so rpmtd_AsPyObj() wont work */
	    keyo = rpmtd_ItemAsPyobj(s->keytd, rpmtdClass(s->keytd));
	    rpmtdFreeData(s->keytd);
	} else {
	    s->ii = rpmdbIndexIteratorFree(s->ii);
	}
    }

    return keyo;
};

static PyObject *
rpmii_instances(rpmiiObject * s)
{
    int entries = rpmdbIndexIteratorNumPkgs(s->ii);
    PyObject * list = PyList_New(entries);
    PyObject * tuple;
    PyObject * item;
    int res;
    for (int i = 0; i < entries; i++) {
        tuple = PyTuple_New(2);
        item = PyLong_FromLong(rpmdbIndexIteratorPkgOffset(s->ii, i));
        if (!item) {
            Py_DECREF(list);
            Py_DECREF(tuple);
            return NULL;
        }
        res = PyTuple_SetItem(tuple, 0, item);
        if (res < 0) {
            Py_DECREF(list);
            Py_DECREF(tuple);
            return NULL;
        }
        item = PyLong_FromLong(rpmdbIndexIteratorTagNum(s->ii, i));
        if (!item) {
            Py_DECREF(list);
            Py_DECREF(tuple);
            return NULL;
        }
        PyTuple_SetItem(tuple, 1, item);
        if (res < 0) {
            Py_DECREF(list);
            Py_DECREF(tuple);
            return NULL;
        }
	PyList_SetItem(list, i, tuple);
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
    rpmtdFree(s->keytd);
    Py_DECREF(s->ref);
    freefunc free = PyType_GetSlot(Py_TYPE(s), Py_tp_free);
    free(s);
}

static int rpmii_bool(rpmiiObject *s)
{
    return (s->ii != NULL);
}


static char rpmii_doc[] =
"";

static PyObject *disabled_new(PyTypeObject *type,
                              PyObject *args, PyObject *kwds)
{
    PyErr_SetString(PyExc_TypeError,
                    "TypeError: cannot create 'rpm.ii' instances");
    return NULL;
}

static PyType_Slot rpmii_Type_Slots[] = {
    {Py_tp_new, disabled_new},
    {Py_tp_dealloc, rpmii_dealloc},
    {Py_nb_bool, rpmii_bool},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, rpmii_doc},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, rpmii_iternext},
    {Py_tp_methods, rpmii_methods},
    {0, NULL},
};

PyTypeObject* rpmii_Type;
PyType_Spec rpmii_Type_Spec = {
    .name = "rpm.ii",
    .basicsize = sizeof(rpmiiObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmii_Type_Slots,
};

PyObject * rpmii_Wrap(PyTypeObject *subtype, rpmdbIndexIterator ii, PyObject *s)
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    rpmiiObject *iio = (rpmiiObject *)subtype_alloc(subtype, 0);
    if (iio == NULL) return NULL;

    iio->ii = ii;
    iio->ref = s;
    iio->keytd = rpmtdNew();
    Py_INCREF(iio->ref);
    return (PyObject *) iio;
}
