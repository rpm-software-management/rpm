/** \ingroup py_c
 * \file python/rpmmi-py.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmdb.h>

#include "rpmmi-py.h"
#include "header-py.h"

#include "debug.h"

/** \ingroup python
 * \class Rpmmi
 * \brief A python rpm.mi match iterator object represents the result of a
 *	database query.
 *
 * Instances of the rpm.mi object provide access to headers that match
 * certain criteria. Typically, a primary index is accessed to find
 * a set of headers that contain a key, and each header is returned
 * serially.
 *
 * The rpm.mi class conains the following methods:
 * - next() -> hdr		Return the next header that matches.
 *
 * - pattern(tag,mire,pattern) 	Specify secondary match criteria.
 *
 * To obtain a rpm.mi object to query the database used by a transaction,
 * the ts.match(tag,key,len) method is used.
 *
 * Here's an example that prints the name of all installed packages:
 * \code
 *	import rpm
 *	ts = rpm.TransactionSet()
 *	for h in ts.dbMatch():
 *	    print h['name']
 * \endcode
 *
 * Here's a more typical example that uses the Name index to retrieve
 * all installed kernel(s):
 * \code
 *	import rpm
 *	ts = rpm.TransactionSet()
 *	mi = ts.dbMatch('name', "kernel")
 *	for h in mi:
 *	    print "%s-%s-%s" % (h['name'], h['version'], h['release'])
 * \endcode
 *
 * Finally, here's an example that retrieves all packages whose name
 * matches the glob expression "XFree*":
 * \code
 *	import rpm
 *	ts = rpm.TransactionSet()
 *	mi = ts.dbMatch()
 *	mi.pattern('name', rpm.RPMMIRE_GLOB, "XFree*")
 *	for h in mi:
 *	    print "%s-%s-%s" % (h['name'], h['version'], h['release'])
 * \endcode
 *
 */

/** \ingroup python
 * \name Class: Rpmmi
 */
/*@{*/

/**
 */
static PyObject *
rpmmi_iter(rpmmiObject * s)
	/*@*/
{
    Py_INCREF(s);
    return (PyObject *)s;
}

/**
 */
/*@null@*/
static PyObject *
rpmmi_iternext(rpmmiObject * s)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies s, rpmGlobalMacroContext @*/
{
    Header h;

    if (s->mi == NULL || (h = rpmdbNextIterator(s->mi)) == NULL) {
	s->mi = rpmdbFreeIterator(s->mi);
	return NULL;
    }
    return (PyObject *) hdr_Wrap(h);
}

/**
 */
/*@null@*/
static PyObject *
rpmmi_Next(rpmmiObject * s)
	/*@globals rpmGlobalMacroContext, _Py_NoneStruct @*/
	/*@modifies s, rpmGlobalMacroContext, _Py_NoneStruct @*/
{
    PyObject * result;

    result = rpmmi_iternext(s);

    if (result == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return result;
}

/**
 */
/*@null@*/
static PyObject *
rpmmi_Instance(rpmmiObject * s)
	/*@*/
{
    int rc = 0;

    if (s->mi != NULL)
	rc = rpmdbGetIteratorOffset(s->mi);

    return Py_BuildValue("i", rc);
}

/**
 */
/*@null@*/
static PyObject *
rpmmi_Count(rpmmiObject * s)
	/*@*/
{
    int rc = 0;

    if (s->mi != NULL)
	rc = rpmdbGetIteratorCount(s->mi);

    return Py_BuildValue("i", rc);
}

/**
 */
/*@null@*/
static PyObject *
rpmmi_Pattern(rpmmiObject * s, PyObject * args, PyObject * kwds)
	/*@globals rpmGlobalMacroContext, _Py_NoneStruct @*/
	/*@modifies s, rpmGlobalMacroContext, _Py_NoneStruct @*/
{
    PyObject *TagN = NULL;
    int type;
    char * pattern;
    rpmTag tag;
    char * kwlist[] = {"tag", "type", "patern", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Ois:Pattern", kwlist,
	    &TagN, &type, &pattern))
	return NULL;

    if ((tag = tagNumFromPyObject (TagN)) == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    rpmdbSetIteratorRE(s->mi, tag, type, pattern);

    Py_INCREF (Py_None);
    return Py_None;

}

/** \ingroup py_c
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmmi_methods[] = {
    {"next",	    (PyCFunction) rpmmi_Next,		METH_NOARGS,
"mi.next() -> hdr\n\
- Retrieve next header that matches. Iterate directly in python if possible.\n" },
    {"instance",    (PyCFunction) rpmmi_Instance,	METH_NOARGS,
	NULL },
    {"count",       (PyCFunction) rpmmi_Count,		METH_NOARGS,
	NULL },
    {"pattern",	    (PyCFunction) rpmmi_Pattern,	METH_VARARGS|METH_KEYWORDS,
"mi.pattern(TagN, mire_type, pattern)\n\
- Set a secondary match pattern on tags from retrieved header.\n" },
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/** \ingroup py_c
 */
static void rpmmi_dealloc(/*@only@*/ /*@null@*/ rpmmiObject * s)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies s, rpmGlobalMacroContext @*/
{
    if (s) {
	s->mi = rpmdbFreeIterator(s->mi);
	Py_DECREF(s->ref);
	PyObject_Del(s);
    }
}

static PyObject * rpmmi_getattro(PyObject * o, PyObject * n)
	/*@*/
{
    return PyObject_GenericGetAttr(o, n);
}

static int rpmmi_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr(o, n, v);
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmmi_doc[] =
"";

/** \ingroup py_c
 */
/*@-fullinitblock@*/
PyTypeObject rpmmi_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.mi",			/* tp_name */
	sizeof(rpmmiObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmmi_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	(getattrofunc) rpmmi_getattro,	/* tp_getattro */
	(setattrofunc) rpmmi_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmmi_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc) rpmmi_iter,	/* tp_iter */
	(iternextfunc) rpmmi_iternext,	/* tp_iternext */
	rpmmi_methods,			/* tp_methods */
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
#endif
};
/*@=fullinitblock@*/

rpmmiObject * rpmmi_Wrap(rpmdbMatchIterator mi, PyObject *s)
{
    rpmmiObject * mio = (rpmmiObject *) PyObject_New(rpmmiObject, &rpmmi_Type);

    if (mio == NULL) {
        PyErr_SetString(pyrpmError, "out of memory creating rpmmiObject");
        return NULL;
    }
    mio->mi = mi;
    mio->ref = s;
    Py_INCREF(mio->ref);
    return mio;
}

/*@}*/
