#include "rpmsystem-py.h"

#include <rpm/rpmdb.h>

#include "rpmmi-py.h"
#include "header-py.h"

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

struct rpmmiObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    PyObject *ref;		/* for db/ts refcounting */
    rpmdbMatchIterator mi;
} ;

static PyObject *
rpmmi_iternext(rpmmiObject * s)
{
    Header h;

    if (s->mi == NULL || (h = rpmdbNextIterator(s->mi)) == NULL) {
	s->mi = rpmdbFreeIterator(s->mi);
	return NULL;
    }
    return hdr_Wrap(&hdr_Type, h);
}

static PyObject *
rpmmi_Instance(rpmmiObject * s, PyObject * unused)
{
    int rc = 0;

    if (s->mi != NULL)
	rc = rpmdbGetIteratorOffset(s->mi);

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmmi_Count(rpmmiObject * s, PyObject * unused)
{
    DEPRECATED_METHOD("use len(mi) instead");
    return Py_BuildValue("i", PyMapping_Size((PyObject *)s));
}

static PyObject *
rpmmi_Pattern(rpmmiObject * s, PyObject * args, PyObject * kwds)
{
    int type;
    const char * pattern;
    rpmTagVal tag;
    char * kwlist[] = {"tag", "type", "patern", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&is:Pattern", kwlist,
	    tagNumFromPyObject, &tag, &type, &pattern))
	return NULL;

    rpmdbSetIteratorRE(s->mi, tag, type, pattern);

    Py_RETURN_NONE;
}

static struct PyMethodDef rpmmi_methods[] = {
    {"instance",    (PyCFunction) rpmmi_Instance,	METH_NOARGS,
	NULL },
    {"count",       (PyCFunction) rpmmi_Count,		METH_NOARGS,
"Deprecated, use len(mi) instead.\n" },
    {"pattern",	    (PyCFunction) rpmmi_Pattern,	METH_VARARGS|METH_KEYWORDS,
"mi.pattern(TagN, mire_type, pattern)\n\
- Set a secondary match pattern on tags from retrieved header.\n" },
    {NULL,		NULL}		/* sentinel */
};

static void rpmmi_dealloc(rpmmiObject * s)
{
    s->mi = rpmdbFreeIterator(s->mi);
    Py_DECREF(s->ref);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static Py_ssize_t rpmmi_length(rpmmiObject * s)
{
    return s->mi ? rpmdbGetIteratorCount(s->mi) : 0;
}

static int rpmmi_bool(rpmmiObject *s)
{
    return (s->mi != NULL);
}

PyMappingMethods rpmmi_as_mapping = {
    (lenfunc) rpmmi_length,		/* mp_length */
    0,
};

static PyNumberMethods rpmmi_as_number = {
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
	(inquiry)rpmmi_bool, /* nb_bool/nonzero */
};

static char rpmmi_doc[] =
"";

PyTypeObject rpmmi_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.mi",			/* tp_name */
	sizeof(rpmmiObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmmi_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	&rpmmi_as_number,		/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmmi_as_mapping,		/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmmi_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
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
};

PyObject * rpmmi_Wrap(PyTypeObject *subtype, rpmdbMatchIterator mi, PyObject *s)
{
    rpmmiObject * mio = (rpmmiObject *)subtype->tp_alloc(subtype, 0);
    if (mio == NULL) return NULL;

    mio->mi = mi;
    mio->ref = s;
    Py_INCREF(mio->ref);
    return (PyObject *) mio;
}

