/** \ingroup python
 * \file python/db-py.c
 */

#include "system.h"

#include "Python.h"

#include "rpmdb.h"

#include "db-py.h"
#include "header-py.h"

#include "debug.h"

/** \ingroup python
 * \class rpm.mi
 * \brief A python rpm.mi match iterator object represents the result of an RPM
 *	database query.
 */

/** \ingroup python
 * \name Class: rpm.mi
 */
/*@{*/

/** \ingroup python
 */
static PyObject *
rpmmi_Next(rpmmiObject * s, PyObject * args) {
    /* XXX assume header? */
    Header h;
    hdrObject * ho;
    
    if (!PyArg_ParseTuple (args, ":Next")) return NULL;

    h = rpmdbNextIterator(s->mi);
    if (!h) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    ho = createHeaderObject(h);
    
    return (PyObject *) ho;
}

/** \ingroup python
 */
static PyObject *
rpmmi_Pattern(rpmmiObject * s, PyObject * args) {
    PyObject *TagN = NULL;
    int type;
    char * pattern;
    rpmTag tag;
    
    if (!PyArg_ParseTuple(args, "Ois:Pattern", &TagN, &type, &pattern))
	return NULL;

    if (TagN == NULL)
	tag = 0;
    else if ((tag = tagNumFromPyObject (TagN)) == -1) {
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
- Set a secondary match pattern on retrieved header tags\n" },
    {NULL,		NULL}		/* sentinel */
};

/** \ingroup python
 */
static void rpmmi_dealloc(rpmmiObject * s)
{
    if (s) {
	s->mi = rpmdbFreeIterator(s->mi);
	Py_DECREF (s->db);
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
	0,				/* tp_iter */
	0,				/* tp_iternext */
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

/*@}*/

/** \ingroup python
 * \class rpm.db
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
 * @deprecated	Use mi = db.match() instead.
 * 
 * - nextkey(index) Returns the index of the next record after "index" in the
 * 		database.
 * @param index	current rpmdb location
 * @deprecated	Use hdr = mi.next() instead.
 * 
 * - findbyfile(file) Returns a list of the indexes to records that own file
 * 		"file".
 * @param file	absolute path to file
 * @deprecated	Use mi = db.match('basename') instead.
 * 
 * - findbyname(name) Returns a list of the indexes to records for packages
 *		named "name".
 * @param name	package name
 * @deprecated	Use mi = db.match('name') instead.
 * 
 * - findbyprovides(dep) Returns a list of the indexes to records for packages
 *		that provide "dep".
 * @param dep	provided dependency string
 * @deprecated	Use mi = db.match('providename') instead.
 * 
 * To obtain a db object, the opendb function in the rpm module
 * must be called.  The opendb function takes two optional arguments.
 * The first optional argument is a boolean flag that specifies if the
 * database is to be opened for read/write access or read-only access.
 * The second argument specifies an alternate root directory for RPM
 * to use.
 * 
 * An example of opening a database and retrieving the first header in
 * the database, then printing the name of the package that the header
 * represents:
 * \code
 * 	import rpm
 * 	db = rpm.opendb()
 * 	mi = db.match()
 *	h = mi.next()
 *	if h:
 * 	    print h[rpm.RPMTAG_NAME]
 * \endcode
 *
 * To print all of the packages in the database that match a package
 * name, the code will look like this:
 * \code
 * 	import rpm
 * 	db = rpm.opendb()
 * 	mi = db.match(rpm.RPMTAG_NAME, "foo")
 * 	while 1:
 *	    h = mi.next()
 *	    if not h:
 *		break
 * 	    print "%s-%s-%s" % (h[rpm.RPMTAG_NAME],
 * 			        h[rpm.RPMTAG_VERSION],
 * 			        h[rpm.RPMTAG_RELEASE])
 * \endcode
 */

/** \ingroup python
 * \name Class: rpm.db
 */
/*@{*/

/**
 */
static PyObject * rpmdb_First(rpmdbObject * s, PyObject * args)
{
    int first;

    if (!PyArg_ParseTuple (args, "")) return NULL;

    /* Acquire all offsets in one fell swoop. */
    if (s->offsets == NULL || s->noffs <= 0) {
	rpmdbMatchIterator mi;
	Header h;

	s->offsets = _free(s->offsets);
	s->noffs = 0;
	mi = rpmdbInitIterator(s->db, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    s->noffs++;
	    s->offsets = realloc(s->offsets, s->noffs * sizeof(s->offsets[0]));
	    s->offsets[s->noffs-1] = rpmdbGetIteratorOffset(mi);
	}
	mi = rpmdbFreeIterator(mi);
    }

    s->offx = 0;
    if (s->offsets != NULL && s->offx < s->noffs)
	first = s->offsets[s->offx++];
    else
	first = 0;

    if (!first) {
	PyErr_SetString(pyrpmError, "cannot find first entry in database\n");
	return NULL;
    }

    return Py_BuildValue("i", first);
}

/**
 */
static PyObject * rpmdb_Next(rpmdbObject * s, PyObject * args)
{
    int where;

    if (!PyArg_ParseTuple (args, "i", &where)) return NULL;

    if (s->offsets == NULL || s->offx >= s->noffs) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    where = s->offsets[s->offx++];

    if (!where) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    return Py_BuildValue("i", where);
}

/**
 */
static PyObject * handleDbResult(rpmdbMatchIterator mi)
{
    PyObject * list = PyList_New(0);
    PyObject * o;

    if (mi != NULL) {
	while (rpmdbNextIterator(mi)) {
	    PyList_Append(list, o=PyInt_FromLong(rpmdbGetIteratorOffset(mi)));
	    Py_DECREF(o);
	}
	mi = rpmdbFreeIterator(mi);
    }

    return list;
}

/**
 */
static PyObject * rpmdb_ByFile(rpmdbObject * s, PyObject * args)
{
    char * str;

    if (!PyArg_ParseTuple(args, "s", &str)) return NULL;

    return handleDbResult(rpmdbInitIterator(s->db, RPMTAG_BASENAMES, str, 0));
}

/**
 */
static PyObject * rpmdb_ByName(rpmdbObject * s, PyObject * args)
{
    char * str;

    if (!PyArg_ParseTuple(args, "s", &str)) return NULL;

    return handleDbResult(rpmdbInitIterator(s->db, RPMTAG_NAME, str, 0));
}

/**
 */
static PyObject * rpmdb_ByProvides(rpmdbObject * s, PyObject * args)
{
    char * str;

    if (!PyArg_ParseTuple(args, "s", &str)) return NULL;

    return handleDbResult(rpmdbInitIterator(s->db, RPMTAG_PROVIDENAME, str, 0));
}

/**
 */
static rpmmiObject *
rpmdb_InitIterator (rpmdbObject * s, PyObject * args)
{
    PyObject *TagN = NULL;
    char *key = NULL;
    int len = 0, tag = -1;
    rpmmiObject * mio;
    
    if (!PyArg_ParseTuple(args, "|Ozi", &TagN, &key, &len))
	return NULL;

    if (TagN == NULL)
	tag = 0;
    else if ((tag = tagNumFromPyObject (TagN)) == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }
    
    mio = (rpmmiObject *) PyObject_NEW(rpmmiObject, &rpmmi_Type);
    if (mio == NULL) {
	PyErr_SetString(pyrpmError, "out of memory creating rpmmiObject");
	return NULL;
    }
    
    mio->mi = rpmdbInitIterator(s->db, tag, key, len);
    mio->db = s;
    Py_INCREF (mio->db);
    
    return mio;
}

/**
 */
static struct PyMethodDef rpmdb_methods[] = {
    {"firstkey",	(PyCFunction) rpmdb_First,	METH_VARARGS,
	NULL },
    {"nextkey",		(PyCFunction) rpmdb_Next,	METH_VARARGS,
	NULL },
    {"findbyfile",	(PyCFunction) rpmdb_ByFile,	METH_VARARGS,
	NULL },
    {"findbyname",	(PyCFunction) rpmdb_ByName,	METH_VARARGS,
	NULL },
    {"findbyprovides",  (PyCFunction) rpmdb_ByProvides,	METH_VARARGS,
	NULL },
    {"match",	    (PyCFunction) rpmdb_InitIterator,	METH_VARARGS,
"db.match([TagN, [key, [len]]]) -> mi\n\
- Create an rpm db match iterator.\n" },
    {NULL,		NULL}		/* sentinel */
};

/**
 */
static int
rpmdb_length(rpmdbObject * s)
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

    ho = createHeaderObject(h);
    h = headerFree(h, NULL);

    return ho;
}

/**
 */
static PyMappingMethods rpmdb_as_mapping = {
	(inquiry) rpmdb_length,		/* mp_length */
	(binaryfunc) rpmdb_subscript,	/* mp_subscript */
	(objobjargproc)0,		/* mp_ass_subscript */
};

/**
 */
static void rpmdb_dealloc(rpmdbObject * s) {
    s->offsets = _free(s->offsets);
    if (s->db)
	rpmdbClose(s->db);
    PyMem_DEL(s);
}

/**
 */
static PyObject * rpmdb_getattr(rpmdbObject * s, char * name) {
    return Py_FindMethod(rpmdb_methods, (PyObject * ) s, name);
}

/**
 */
static char rpmdb_doc[] =
"";

/**
 */
PyTypeObject rpmdb_Type = {
	PyObject_HEAD_INIT(NULL)
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

rpmdb dbFromDb(rpmdbObject * db) {
    return db->db;
}

/**
 */
rpmdbObject * rpmOpenDB(PyObject * self, PyObject * args) {
    rpmdbObject * o;
    char * root = "";
    int forWrite = 0;

    if (!PyArg_ParseTuple(args, "|is", &forWrite, &root)) return NULL;

    o = PyObject_NEW(rpmdbObject, &rpmdb_Type);
    o->db = NULL;
    o->offx = 0;
    o->noffs = 0;
    o->offsets = NULL;

    if (rpmdbOpen(root, &o->db, forWrite ? O_RDWR | O_CREAT: O_RDONLY, 0644)) {
	char * errmsg = "cannot open database in %s";
	char * errstr = NULL;
	int errsize;

	Py_DECREF(o);
	/* PyErr_SetString should take varargs... */
	errsize = strlen(errmsg) + *root == '\0' ? 15 /* "/var/lib/rpm" */ : strlen(root);
	errstr = alloca(errsize);
	snprintf(errstr, errsize, errmsg, *root == '\0' ? "/var/lib/rpm" : root);
	PyErr_SetString(pyrpmError, errstr);
	return NULL;
    }

    return o;
}

/**
 */
PyObject * rebuildDB (PyObject * self, PyObject * args) {
    char * root = "";

    if (!PyArg_ParseTuple(args, "s", &root)) return NULL;

    return Py_BuildValue("i", rpmdbRebuild(root));
}

/*@}*/
