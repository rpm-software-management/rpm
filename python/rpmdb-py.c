/** \ingroup python
 * \file python/rpmdb-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include <rpmlib.h>

#include "rpmdb-py.h"
#include "rpmmi-py.h"
#include "header-py.h"

#include "debug.h"

/*@access Header @*/

/** \ingroup python
 * \class Rpmdb
 * \brief A python rpmdb object represents an RPM database.
 * 
 * Instances of the rpmdb object provide access to the records of a
 * RPM database.  The records are accessed by index number.  To
 * retrieve the header data in the RPM database, the rpmdb object is
 * subscripted as you would access members of a list.
 * 
 * The rpmdb class contains the following methods:
 * 
 * - firstkey()	Returns the index of the first record in the database.
 * @deprecated	Use mi = ts.dbMatch() (or db.match()) instead.
 * 
 * - nextkey(index) Returns the index of the next record after "index" in the
 * 		database.
 * @param index	current rpmdb location
 * @deprecated	Use hdr = mi.next() instead.
 * 
 * - findbyfile(file) Returns a list of the indexes to records that own file
 * 		"file".
 * @param file	absolute path to file
 * @deprecated	Use mi = ts.dbMatch('basename') instead.
 * 
 * - findbyname(name) Returns a list of the indexes to records for packages
 *		named "name".
 * @param name	package name
 * @deprecated	Use mi = ts.dbMatch('name') instead.
 * 
 * - findbyprovides(dep) Returns a list of the indexes to records for packages
 *		that provide "dep".
 * @param dep	provided dependency string
 * @deprecated	Use mi = ts.dbMmatch('providename') instead.
 * 
 * To obtain a db object explicitly, the opendb function in the rpm module
 * can be called. The opendb function takes two optional arguments.
 * The first optional argument is a boolean flag that specifies if the
 * database is to be opened for read/write access or read-only access.
 * The second argument specifies an alternate root directory for RPM
 * to use.
 *
 * Note that in most cases, one is interested in querying the default
 * database in /var/lib/rpm and an rpm.mi match iterator derived from
 * an implicit open of the database from an rpm.ts transaction set object:
 * \code
 * 	import rpm
 * 	ts = rpm.TransactionSet()
 *	mi = ts.dbMatch()
 *	...
 * \endcode
 * is simpler than explicitly opening a database object:
 * \code
 * 	import rpm
 * 	db = rpm.opendb()
 * 	mi = db.match()
 * \endcode
 * 
 * An example of opening a database and retrieving the first header in
 * the database, then printing the name of the package that the header
 * represents:
 * \code
 * 	import rpm
 * 	db = rpm.opendb()
 * 	mi = db.match()
 *	if mi:
 *	    h = mi.next()
 *	    if h:
 * 		print h['name']
 * \endcode
 *
 * To print all of the packages in the database that match a package
 * name, the code will look like this:
 * \code
 * 	import rpm
 * 	db = rpm.opendb()
 * 	mi = db.match('name', "foo")
 * 	while mi:
 *	    h = mi.next()
 *	    if not h:
 *		break
 * 	    print "%s-%s-%s" % (h['name'], h['version'], h['release'])
 * \endcode
 */

/** \ingroup python
 * \name Class: rpm.db
 */
/*@{*/

/**
 */
static rpmmiObject *
rpmdb_Match (rpmdbObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject *TagN = NULL;
    char *key = NULL;
    int len = 0;
    int tag = RPMDBI_PACKAGES;

    if (!PyArg_ParseTuple(args, "|Ozi", &TagN, &key, &len))
	return NULL;

    if (TagN && (tag = tagNumFromPyObject (TagN)) == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    return rpmmi_Wrap( rpmdbInitIterator(s->db, tag, key, len) );
}

/**
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmdb_methods[] = {
    {"match",	    (PyCFunction) rpmdb_Match,	METH_VARARGS,
"db.match([TagN, [key, [len]]]) -> mi\n\
- Create an rpm db match iterator.\n" },
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/**
 */
static int
rpmdb_length(rpmdbObject * s)
	/*@modifies s @*/
{
    rpmdbMatchIterator mi;
    int count = 0;

    mi = rpmdbInitIterator(s->db, RPMDBI_PACKAGES, NULL, 0);
    while (rpmdbNextIterator(mi) != NULL)
	count++;
    mi = rpmdbFreeIterator(mi);

    return count;
}

/**
 */
static hdrObject *
rpmdb_subscript(rpmdbObject * s, PyObject * key)
	/*@modifies s @*/
{
    int offset;
    hdrObject * ho;
    Header h;
    rpmdbMatchIterator mi;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    offset = (int) PyInt_AsLong(key);

    mi = rpmdbInitIterator(s->db, RPMDBI_PACKAGES, &offset, sizeof(offset));
    if (!(h = rpmdbNextIterator(mi))) {
	mi = rpmdbFreeIterator(mi);
	PyErr_SetString(pyrpmError, "cannot read rpmdb entry");
	return NULL;
    }

    ho = hdr_Wrap(h);
    h = headerFree(h);

    return ho;
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static PyMappingMethods rpmdb_as_mapping = {
	(inquiry) rpmdb_length,		/* mp_length */
	(binaryfunc) rpmdb_subscript,	/* mp_subscript */
	(objobjargproc)0,		/* mp_ass_subscript */
};

/**
 */
static void rpmdb_dealloc(rpmdbObject * s)
	/*@modifies s @*/
{
    if (s->db)
	rpmdbClose(s->db);
    PyMem_DEL(s);
}

/**
 */
static PyObject * rpmdb_getattr(rpmdbObject * s, char * name)
	/*@*/
{
    return Py_FindMethod(rpmdb_methods, (PyObject * ) s, name);
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmdb_doc[] =
"";

/**
 */
/*@-fullinitblock@*/
PyTypeObject rpmdb_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.db",			/* tp_name */
	sizeof(rpmdbObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmdb_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmdb_getattr, 	/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmdb_as_mapping,		/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmdb_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmdb_methods,			/* tp_methods */
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

#ifdef  _LEGACY_BINDINGS_TOO
rpmdb dbFromDb(rpmdbObject * db)
{
    return db->db;
}

/**
 */
rpmdbObject * rpmOpenDB(/*@unused@*/ PyObject * self, PyObject * args) {
    rpmdbObject * o;
    char * root = "";
    int forWrite = 0;

    if (!PyArg_ParseTuple(args, "|is", &forWrite, &root)) return NULL;

    o = PyObject_NEW(rpmdbObject, &rpmdb_Type);
    o->db = NULL;

    if (rpmdbOpen(root, &o->db, forWrite ? O_RDWR | O_CREAT: O_RDONLY, 0644)) {
	char * errmsg = "cannot open database in %s";
	char * errstr = NULL;
	int errsize;

	Py_DECREF(o);
	/* PyErr_SetString should take varargs... */
	errsize = strlen(errmsg) + *root == '\0' ? 15 /* "/var/lib/rpm" */ : strlen(root);
	errstr = alloca(errsize);
/*@-formatconst@*/
	snprintf(errstr, errsize, errmsg, *root == '\0' ? "/var/lib/rpm" : root);
/*@=formatconst@*/
	PyErr_SetString(pyrpmError, errstr);
	return NULL;
    }

    return o;
}

/**
 * @todo Permit header checks when doing --rebuilddb.
 */
PyObject * rebuildDB (/*@unused@*/ PyObject * self, PyObject * args)
{
    char * rootDir = "";

    if (!PyArg_ParseTuple(args, "s", &rootDir)) return NULL;

    return Py_BuildValue("i", rpmdbRebuild(rootDir, NULL, NULL));
}
#endif

/*@}*/
