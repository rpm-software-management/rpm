/** \ingroup python
 * \file python/rpmmi-py.c
 */

#include "system.h"

#include "Python.h"
#include <rpmlib.h>

#include "rpmdb-py.h"
#include "rpmmi-py.h"
#include "header-py.h"

#include "debug.h"

/** \ingroup python
 * \class rpm.mi
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
 *	mi = ts.dbMatch()
 *	while mi:
 *	    h = mi.next()
 *	    if not h:
 *		break
 *	    print h['name']
 * \endcode
 *
 * Here's a more typical example that uses the Name index to retrieve
 * all installed kernel(s):
 * \code
 *	import rpm
 *	ts = rpm.TransactionSet()
 *	mi = ts.dbMatch('name', "kernel")
 *	while mi:
 *	    h = mi.next()
 *	    if not h:
 *		break;
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
 *	while mi:
 *	    h = mi.next{}
 *	    if not h:
 *		break;
 *	    print "%s-%s-%s" % (h['name'], h['version'], h['release'])
 * \endcode
 *
 */

/** \ingroup python
 * \name Class: rpm.mi
 */
/*@{*/

#if Py_TPFLAGS_HAVE_ITER
static PyObject *
rpmmi_Iter(rpmmiObject * s)
{
assert(s->mi);
    Py_INCREF(s);
    return (PyObject *)s;
}

/** \ingroup python
 */
static PyObject *
rpmmi_Next(rpmmiObject * s)
{
    Header h;
    
    if (s->mi == NULL || (h = rpmdbNextIterator(s->mi)) == NULL) {
	if (s->mi) s->mi = rpmdbFreeIterator(s->mi);
	Py_INCREF(Py_None);
	return Py_None;
    }
    return (PyObject *) hdr_Wrap(h);
}
#endif

/** \ingroup python
 */
static PyObject *
rpmmi_Pattern(rpmmiObject * s, PyObject * args)
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
static struct PyMethodDef rpmmi_methods[] = {
    {"next",	    (PyCFunction) rpmmi_Next,		METH_VARARGS,
"mi.next() -> hdr\n\
- Retrieve next header that matches.\n" },
    {"pattern",	    (PyCFunction) rpmmi_Pattern,	METH_VARARGS,
"mi.pattern(TagN, mire_type, pattern)\n\
- Set a secondary match pattern on tags from retrieved header.\n" },
    {NULL,		NULL}		/* sentinel */
};

/** \ingroup python
 */
static void rpmmi_dealloc(rpmmiObject * s)
{
    if (s) {
	if (s->mi) s->mi = rpmdbFreeIterator(s->mi);
	PyMem_DEL(s);
    }
}

/** \ingroup python
 */
static PyObject * rpmmi_getattr (rpmdbObject *s, char *name)
{
    return Py_FindMethod (rpmmi_methods, (PyObject *) s, name);
}

/**
 */
static char rpmmi_doc[] =
"";

/** \ingroup python
 */
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
	(getiterfunc) rpmmi_Iter,	/* tp_iter */
	(iternextfunc) rpmmi_Next,	/* tp_iternext */
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

rpmmiObject * rpmmi_Wrap(rpmdbMatchIterator mi)
{
    rpmmiObject * mio;
    if (mi == NULL) {
	Py_INCREF(Py_None);
        return (rpmmiObject *) Py_None;
    }
    mio = (rpmmiObject *) PyObject_NEW(rpmmiObject, &rpmmi_Type);
    if (mio == NULL) {
        PyErr_SetString(pyrpmError, "out of memory creating rpmmiObject");
        return NULL;
    }
    mio->mi = mi;
    return mio;
}

/*@}*/
