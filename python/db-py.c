/** \ingroup python
 * \file python/db-py.c
 */

#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#include "Python.h"
#include "rpmio_internal.h"
#include "rpmcli.h"	/* XXX for rpmCheckSig */
#include "misc.h"
#include "header_internal.h"
#include "upgrade.h"

#include "db-py.h"
#include "header-py.h"

/** \ingroup python
 */
typedef struct rpmdbMIObject_s rpmdbMIObject;

/** \ingroup python
 * \class rpmdbMatchIterator
 * \brief A python rpmdbMatchIterator object represents the result of an RPM
 *	database query.
 */

/** \ingroup python
 * \name Class: rpmdbMatchIterator
 */
/*@{*/

/** \ingroup python
 */
struct rpmdbMIObject_s {
    PyObject_HEAD;
    rpmdbObject *db;
    rpmdbMatchIterator mi;
} ;

/** \ingroup python
 */
static PyObject *
rpmdbMINext(rpmdbMIObject * s, PyObject * args) {
    /* XXX assume header? */
    Header h;
    hdrObject * ho;
    
    if (!PyArg_ParseTuple (args, "")) return NULL;

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
rpmdbMIpattern(rpmdbMIObject * s, PyObject * args) {
    PyObject *index = NULL;
    int type;
    char * pattern;
    int tag;
    
    if (!PyArg_ParseTuple(args, "Ois", &index, &type, &pattern))
	return NULL;

    if (index == NULL)
	tag = 0;
    else if ((tag = tagNumFromPyObject (index)) == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    rpmdbSetIteratorRE(s->mi, tag, type, pattern);

    Py_INCREF (Py_None);
    return Py_None;
    
}

/** \ingroup python
 */
static struct PyMethodDef rpmdbMIMethods[] = {
	{"next",	    (PyCFunction) rpmdbMINext,	1 },
	{"pattern",	    (PyCFunction) rpmdbMIpattern, 1 },
	{NULL,		NULL}		/* sentinel */
};

/** \ingroup python
 */
static PyObject * rpmdbMIGetAttr (rpmdbObject *s, char *name) {
    return Py_FindMethod (rpmdbMIMethods, (PyObject *) s, name);
}

/** \ingroup python
 */
static void rpmdbMIDealloc(rpmdbMIObject * s) {
    if (s && s->mi) {
	rpmdbFreeIterator(s->mi);
    }
    Py_DECREF (s->db);
    PyMem_DEL(s);
}

/** \ingroup python
 */
PyTypeObject rpmdbMIType = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"rpmdbMatchIterator",		/* tp_name */
	sizeof(rpmdbMIObject),	/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmdbMIDealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmdbMIGetAttr, 	/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
};

/*@}*/

/** \ingroup python
 * \class rpmdb
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
 * @deprecated	Legacy, use rpmdbMatchIterator instead.
 * 
 * - nextkey(index) Returns the index of the next record after "index" in the
 * 		database.
 * @param index	current rpmdb location
 * @deprecated	Legacy, use rpmdbMatchIterator instead.
 * 
 * - findbyfile(file) Returns a list of the indexes to records that own file
 * 		"file".
 * @param file	absolute path to file
 * 
 * - findbyname(name) Returns a list of the indexes to records for packages
 *		named "name".
 * @param name	package name
 * 
 * - findbyprovides(dep) Returns a list of the indexes to records for packages
 *		that provide "dep".
 * @param dep	provided dependency string
 * 
 * To obtain a rpmdb object, the opendb function in the rpm module
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
 * 	rpmdb = rpm.opendb()
 * 	index = rpmdb.firstkey()
 * 	header = rpmdb[index]
 * 	print header[rpm.RPMTAG_NAME]
 * \endcode
 * To print all of the packages in the database that match a package
 * name, the code will look like this:
 * \code
 * 	import rpm
 * 	rpmdb = rpm.opendb()
 * 	indexes = rpmdb.findbyname("foo")
 * 	for index in indexes:
 * 	    header = rpmdb[index]
 * 	    print "%s-%s-%s" % (header[rpm.RPMTAG_NAME],
 * 			        header[rpm.RPMTAG_VERSION],
 * 			        header[rpm.RPMTAG_RELEASE])
 * \endcode
 */

/** \ingroup python
 * \name Class: rpmdb
 */
/*@{*/

/**
 */
static PyObject * rpmdbFirst(rpmdbObject * s, PyObject * args) {
    int first;

    if (!PyArg_ParseTuple (args, "")) return NULL;

    /* Acquire all offsets in one fell swoop. */
    if (s->offsets == NULL || s->noffs <= 0) {
	rpmdbMatchIterator mi;
	Header h;

	if (s->offsets)
	    free(s->offsets);
	s->offsets = NULL;
	s->noffs = 0;
	mi = rpmdbInitIterator(s->db, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    s->noffs++;
	    s->offsets = realloc(s->offsets, s->noffs * sizeof(s->offsets[0]));
	    s->offsets[s->noffs-1] = rpmdbGetIteratorOffset(mi);
	}
	rpmdbFreeIterator(mi);
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
static PyObject * rpmdbNext(rpmdbObject * s, PyObject * args) {
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
static PyObject * handleDbResult(rpmdbMatchIterator mi) {
    PyObject * list, *o;

    list = PyList_New(0);

    /* XXX FIXME: unnecessary header mallocs are side effect here */
    if (mi != NULL) {
	while (rpmdbNextIterator(mi)) {
	    PyList_Append(list, o=PyInt_FromLong(rpmdbGetIteratorOffset(mi)));
	    Py_DECREF(o);
	}
	rpmdbFreeIterator(mi);
    }

    return list;
}

/**
 */
static PyObject * rpmdbByFile(rpmdbObject * s, PyObject * args) {
    char * str;

    if (!PyArg_ParseTuple(args, "s", &str)) return NULL;

    return handleDbResult(rpmdbInitIterator(s->db, RPMTAG_BASENAMES, str, 0));
}

/**
 */
static PyObject * rpmdbByName(rpmdbObject * s, PyObject * args) {
    char * str;

    if (!PyArg_ParseTuple(args, "s", &str)) return NULL;

    return handleDbResult(rpmdbInitIterator(s->db, RPMTAG_NAME, str, 0));
}

/**
 */
static PyObject * rpmdbByProvides(rpmdbObject * s, PyObject * args) {
    char * str;

    if (!PyArg_ParseTuple(args, "s", &str)) return NULL;

    return handleDbResult(rpmdbInitIterator(s->db, RPMTAG_PROVIDENAME, str, 0));
}

/**
 */
static rpmdbMIObject *
py_rpmdbInitIterator (rpmdbObject * s, PyObject * args) {
    PyObject *index = NULL;
    char *key = NULL;
    int len = 0, tag = -1;
    rpmdbMIObject * mio;
    
    if (!PyArg_ParseTuple(args, "|Ozi", &index, &key, &len))
	return NULL;

    if (index == NULL)
	tag = 0;
    else if ((tag = tagNumFromPyObject (index)) == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }
    
    mio = (rpmdbMIObject *) PyObject_NEW(rpmdbMIObject, &rpmdbMIType);
    if (mio == NULL) {
	PyErr_SetString(pyrpmError, "out of memory creating rpmdbMIObject");
	return NULL;
    }
    
    mio->mi = rpmdbInitIterator(s->db, tag, key, len);
    mio->db = s;
    Py_INCREF (mio->db);
    
    return mio;
}

/**
 */
static struct PyMethodDef rpmdbMethods[] = {
	{"firstkey",	    (PyCFunction) rpmdbFirst,	1 },
	{"nextkey",	    (PyCFunction) rpmdbNext,	1 },
	{"findbyfile",	    (PyCFunction) rpmdbByFile, 1 },
	{"findbyname",	    (PyCFunction) rpmdbByName, 1 },
	{"findbyprovides",  (PyCFunction) rpmdbByProvides, 1 },
	{"match",	    (PyCFunction) py_rpmdbInitIterator, 1 },
	{NULL,		NULL}		/* sentinel */
};

/**
 */
static PyObject * rpmdbGetAttr(rpmdbObject * s, char * name) {
    return Py_FindMethod(rpmdbMethods, (PyObject * ) s, name);
}

/**
 */
static void rpmdbDealloc(rpmdbObject * s) {
    if (s->offsets) {
	free(s->offsets);
    }
    if (s->db) {
	rpmdbClose(s->db);
    }
    PyMem_DEL(s);
}

#ifndef DYINGSOON	/* XXX OK, when? */
/**
 */
static int
rpmdbLength(rpmdbObject * s) {
    int count = 0;

    {	rpmdbMatchIterator mi;

	/* RPMDBI_PACKAGES */
	mi = rpmdbInitIterator(s->db, RPMDBI_PACKAGES, NULL, 0);
	while (rpmdbNextIterator(mi) != NULL)
	    count++;
	rpmdbFreeIterator(mi);
    }

    return count;
}

/**
 */
static hdrObject *
rpmdbSubscript(rpmdbObject * s, PyObject * key) {
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
	rpmdbFreeIterator(mi);
	PyErr_SetString(pyrpmError, "cannot read rpmdb entry");
	return NULL;
    }

    ho = createHeaderObject(h);
    headerFree(h, NULL);

    return ho;
}

/**
 */
static PyMappingMethods rpmdbAsMapping = {
	(inquiry) rpmdbLength,		/* mp_length */
	(binaryfunc) rpmdbSubscript,	/* mp_subscript */
	(objobjargproc)0,		/* mp_ass_subscript */
};
#endif

/**
 */
PyTypeObject rpmdbType = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"rpmdb",			/* tp_name */
	sizeof(rpmdbObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmdbDealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmdbGetAttr, 	/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
#ifndef DYINGSOON
	&rpmdbAsMapping,		/* tp_as_mapping */
#else
	0,
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

    o = PyObject_NEW(rpmdbObject, &rpmdbType);
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

