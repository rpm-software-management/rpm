/** \ingroup python
 * \file python/rpmmi-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

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
static PyObject *
rpmmi_iternext(rpmmiObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
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
static PyObject *
rpmmi_Next(rpmmiObject * s, PyObject *args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result;
    
    if (!PyArg_ParseTuple(args, ":Next"))
	return NULL;

    result = rpmmi_iternext(s);

    if (result == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return result;
}

/**
 */
static PyObject *
rpmmi_Pattern(rpmmiObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject *TagN = NULL;
    int type;
    char * pattern;
    rpmTag tag;
    
    if (!PyArg_ParseTuple(args, "Ois:Pattern", &TagN, &type, &pattern))
	return NULL;

    if ((tag = tagNumFromPyObject (TagN)) == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    rpmdbSetIteratorRE(s->mi, tag, type, pattern);

    Py_INCREF (Py_None);
    return Py_None;
    
}

/** \ingroup python
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmmi_methods[] = {
    {"next",	    (PyCFunction) rpmmi_Next,		METH_VARARGS,
"mi.next() -> hdr\n\
- Retrieve next header that matches. Iterate directly in python if possible.\n" },
    {"pattern",	    (PyCFunction) rpmmi_Pattern,	METH_VARARGS,
"mi.pattern(TagN, mire_type, pattern)\n\
- Set a secondary match pattern on tags from retrieved header.\n" },
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/** \ingroup python
 */
static void rpmmi_dealloc(/*@only@*/ /*@null@*/ rpmmiObject * s)
	/*@modifies s @*/
{
    if (s) {
	if (s->mi) s->mi = rpmdbFreeIterator(s->mi);
	PyMem_DEL(s);
    }
}

/** \ingroup python
 */
static PyObject * rpmmi_getattr (rpmmiObject *s, char *name)
	/*@*/
{
    return Py_FindMethod (rpmmi_methods, (PyObject *) s, name);
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmmi_doc[] =
"";

/** \ingroup python
 */
/*@-fullinitblock@*/
PyTypeObject rpmmi_Type = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"rpm.mi",			/* tp_name */
	sizeof(rpmmiObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmmi_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmmi_getattr, 	/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
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

rpmmiObject * rpmmi_Wrap(rpmdbMatchIterator mi)
{
    rpmmiObject * mio = (rpmmiObject *) PyObject_NEW(rpmmiObject, &rpmmi_Type);

    if (mio == NULL) {
        PyErr_SetString(pyrpmError, "out of memory creating rpmmiObject");
        return NULL;
    }
    mio->mi = mi;
    return mio;
}

/*@}*/
