#include "rpmsystem-py.h"

#include <rpm/rpmdb.h>
#include <rpm/header.h>

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
 *	    print(h['name'])
 * \endcode
 *
 * Here's a more typical example that uses the Name index to retrieve
 * all installed kernel(s):
 * \code
 *	import rpm
 *	ts = rpm.TransactionSet()
 *	mi = ts.dbMatch('name', 'kernel')
 *	for h in mi:
 *	    print('%s-%s-%s' % (h['name'], h['version'], h['release']))
 * \endcode
 *
 * Finally, here's an example that retrieves all packages whose name
 * matches the glob expression 'XFree*':
 * \code
 *	import rpm
 *	ts = rpm.TransactionSet()
 *	mi = ts.dbMatch()
 *	mi.pattern('name', rpm.RPMMIRE_GLOB, 'XFree*')
 *	for h in mi:
 *	    print('%s-%s-%s' % (h['name'], h['version'], h['release']))
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
    headerLink(h);
    return hdr_Wrap(hdr_Type, h);
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
     "mi.instance() -- Return the number (db key) of the current header."},
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
    freefunc free = PyType_GetSlot(Py_TYPE(s), Py_tp_free);
    free(s);
}

static Py_ssize_t rpmmi_length(rpmmiObject * s)
{
    return s->mi ? rpmdbGetIteratorCount(s->mi) : 0;
}

static int rpmmi_bool(rpmmiObject *s)
{
    return (s->mi != NULL);
}



static char rpmmi_doc[] =
  "rpm.mi match iterator object represents the result of a\n"
  "	database query.\n"
  "\n"
  "Instances of the rpm.mi object provide access to headers that match\n"
  "certain criteria. Typically, a primary index is accessed to find\n"
  "a set of headers that contain a key, and each header is returned\n"
  "serially.\n"
  "\n"
  "To obtain a rpm.mi object to query the database used by a transaction,\n"
  "the ts.match(tag,key,len) method is used.\n"
  "\n"
  "Here's an example that prints the name of all installed packages:\n"
  "	import rpm\n"
  "	ts = rpm.TransactionSet()\n"
  "	for h in ts.dbMatch():\n"
  "	    print(h['name'])\n"
  "\n"
  "Here's a more typical example that uses the Name index to retrieve\n"
  "all installed kernel(s):\n"
  "	import rpm\n"
  "	ts = rpm.TransactionSet()\n"
  "	mi = ts.dbMatch('name', 'kernel')\n"
  "	for h in mi:\n"
  "	    print('%s-%s-%s' % (h['name'], h['version'], h['release']))\n"
  "\n"
  "Finally, here's an example that retrieves all packages whose name\n"
  "matches the glob expression 'XFree*':\n"
  "	import rpm\n"
  "	ts = rpm.TransactionSet()\n"
  "	mi = ts.dbMatch()\n"
  "	mi.pattern('name', rpm.RPMMIRE_GLOB, 'XFree*')\n"
  "	for h in mi:\n"
  "	    print('%s-%s-%s' % (h['name'], h['version'], h['release']))\n"
;

static PyObject *disabled_new(PyTypeObject *type,
                              PyObject *args, PyObject *kwds)
{
    PyErr_SetString(PyExc_TypeError,
                    "TypeError: cannot create 'rpm.mi' instances");
    return NULL;
}

static PyType_Slot rpmmi_Type_Slots[] = {
    {Py_tp_new, disabled_new},
    {Py_tp_dealloc, rpmmi_dealloc},
    {Py_nb_bool, rpmmi_bool},
    {Py_mp_length, rpmmi_length},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, rpmmi_doc},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, rpmmi_iternext},
    {Py_tp_methods, rpmmi_methods},
    {0, NULL},
};

PyTypeObject* rpmmi_Type;
PyType_Spec rpmmi_Type_Spec = {
    .name = "rpm.mi",
    .basicsize = sizeof(rpmmiObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmmi_Type_Slots,
};

PyObject * rpmmi_Wrap(PyTypeObject *subtype, rpmdbMatchIterator mi, PyObject *s)
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    rpmmiObject *mio = (rpmmiObject *)subtype_alloc(subtype, 0);
    if (mio == NULL) return NULL;

    mio->mi = mi;
    mio->ref = s;
    Py_INCREF(mio->ref);
    return (PyObject *) mio;
}

