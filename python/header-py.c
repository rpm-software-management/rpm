#include "rpmsystem-py.h"

#include <rpm/rpmlib.h>		/* rpmvercmp */
#include <rpm/rpmtag.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmts.h>	/* XXX rpmtsCreate/rpmtsFree */

#include "header-py.h"
#include "rpmds-py.h"
#include "rpmfd-py.h"
#include "rpmfi-py.h"
#include "rpmtd-py.h"

#include "debug.h"

/** \ingroup python
 * \class Rpm
 * \brief START HERE / RPM base module for the Python API
 *
 * The rpm base module provides the main starting point for
 * accessing RPM from Python. For most usage, call
 * the TransactionSet method to get a transaction set (rpmts).
 *
 * For example:
 * \code
 *	import rpm
 *
 *	ts = rpm.TransactionSet()
 * \endcode
 *
 * The transaction set will open the RPM database as needed, so
 * in most cases, you do not need to explicitly open the
 * database. The transaction set is the workhorse of RPM.
 *
 * You can open another RPM database, such as one that holds
 * all packages for a given Linux distribution, to provide
 * packages used to solve dependencies. To do this, use
 * the following code:
 *
 * \code
 * rpm.addMacro('_dbpath', '/path/to/alternate/database')
 * solvets = rpm.TransactionSet()
 * solvets.openDB()
 * rpm.delMacro('_dbpath')
 *
 * # Open default database
 * ts = rpm.TransactionSet()
 * \endcode
 *
 * This code gives you access to two RPM databases through
 * two transaction sets (rpmts): ts is a transaction set
 * associated with the default RPM database and solvets
 * is a transaction set tied to an alternate database, which
 * is very useful for resolving dependencies.
 *
 * The rpm methods used here are:
 *
 * - addMacro(macro, value)
 * @param macro   Name of macro to add
 * @param value   Value for the macro
 *
 * - delMacro(macro)
 * @param macro   Name of macro to delete
 *
 */

/** \ingroup python
 * \class Rpmhdr
 * \brief A python header object represents an RPM package header.
 *
 * All RPM packages have headers that provide metadata for the package.
 * Header objects can be returned by database queries or loaded from a
 * binary package on disk.
 *
 * The ts.hdrFromFdno() function returns the package header from a
 * package on disk, verifying package signatures and digests of the
 * package while reading.
 *
 * Note: The older method rpm.headerFromPackage() which has been replaced
 * by ts.hdrFromFdno() used to return a (hdr, isSource) tuple.
 *
 * If you need to distinguish source/binary headers, do:
 * \code
 * 	import os, rpm
 *
 *	ts = rpm.TransactionSet()
 * 	fdno = os.open("/tmp/foo-1.0-1.i386.rpm", os.O_RDONLY)
 * 	hdr = ts.hdrFromFdno(fdno)
 *	os.close(fdno)
 *	if hdr[rpm.RPMTAG_SOURCEPACKAGE]:
 *	   print "header is from a source package"
 *	else:
 *	   print "header is from a binary package"
 * \endcode
 *
 * The Python interface to the header data is quite elegant.  It
 * presents the data in a dictionary form.  We'll take the header we
 * just loaded and access the data within it:
 * \code
 * 	print hdr[rpm.RPMTAG_NAME]
 * 	print hdr[rpm.RPMTAG_VERSION]
 * 	print hdr[rpm.RPMTAG_RELEASE]
 * \endcode
 * in the case of our "foo-1.0-1.i386.rpm" package, this code would
 * output:
\verbatim
  	foo
  	1.0
  	1
\endverbatim
 *
 * You make also access the header data by string name:
 * \code
 * 	print hdr['name']
 * 	print hdr['version']
 * 	print hdr['release']
 * \endcode
 *
 * This method of access is a teensy bit slower because the name must be
 * translated into the tag number dynamically. You also must make sure
 * the strings in header lookups don't get translated, or the lookups
 * will fail.
 */

/** \ingroup python
 * \name Class: rpm.hdr
 */

struct hdrObject_s {
    PyObject_HEAD
    Header h;
    HeaderIterator hi;
};

static PyObject * hdrKeyList(hdrObject * s)
{
    PyObject * keys = PyList_New(0);
    HeaderIterator hi = headerInitIterator(s->h);
    rpmTag tag;

    while ((tag = headerNextTag(hi)) != RPMTAG_NOT_FOUND) {
	PyList_Append(keys, PyInt_FromLong(tag));
    }
    headerFreeIterator(hi);

    return keys;
}

static PyObject * hdrUnload(hdrObject * s, PyObject * args, PyObject *keywords)
{
    char * buf;
    PyObject * rc;
    int len, legacy = 0;
    Header h;
    static char *kwlist[] = { "legacyHeader", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|i", kwlist, &legacy))
	return NULL;

    h = headerLink(s->h);
    /* XXX this legacy switch is a hack, needs to be removed. */
    if (legacy) {
	h = headerCopy(s->h);	/* XXX strip region tags, etc */
	headerFree(s->h);
    }
    len = headerSizeof(h, 0);
    buf = headerUnload(h);
    h = headerFree(h);

    if (buf == NULL || len == 0) {
	PyErr_SetString(pyrpmError, "can't unload bad header\n");
	return NULL;
    }

    rc = PyBytes_FromStringAndSize(buf, len);
    buf = _free(buf);

    return rc;
}

static PyObject * hdrExpandFilelist(hdrObject * s)
{
    DEPRECATED_METHOD("use hdr.convert() instead");
    headerConvert(s->h, HEADERCONV_EXPANDFILELIST);

    Py_RETURN_NONE;
}

static PyObject * hdrCompressFilelist(hdrObject * s)
{
    DEPRECATED_METHOD("use hdr.convert() instead");
    headerConvert(s->h, HEADERCONV_COMPRESSFILELIST);

    Py_RETURN_NONE;
}

/* make a header with _all_ the tags we need */
static PyObject * hdrFullFilelist(hdrObject * s)
{
    rpmtd fileNames = rpmtdNew();
    Header h = s->h;

    DEPRECATED_METHOD("obsolete method");
    if (!headerIsEntry (h, RPMTAG_BASENAMES)
	|| !headerIsEntry (h, RPMTAG_DIRNAMES)
	|| !headerIsEntry (h, RPMTAG_DIRINDEXES))
	headerConvert(h, HEADERCONV_COMPRESSFILELIST);

    if (headerGet(h, RPMTAG_FILENAMES, fileNames, HEADERGET_EXT)) {
	rpmtdSetTag(fileNames, RPMTAG_OLDFILENAMES);
	headerPut(h, fileNames, HEADERPUT_DEFAULT);
	rpmtdFreeData(fileNames);
    }
    rpmtdFree(fileNames);

    Py_RETURN_NONE;
}

static PyObject * hdrFormat(hdrObject * s, PyObject * args, PyObject * kwds)
{
    char * fmt;
    char * r;
    errmsg_t err;
    PyObject * result;
    char * kwlist[] = {"format", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &fmt))
	return NULL;

    r = headerFormat(s->h, fmt, &err);
    if (!r) {
	PyErr_SetString(pyrpmError, err);
	return NULL;
    }

    result = Py_BuildValue("s", r);
    r = _free(r);

    return result;
}

static PyObject *hdrInstance(hdrObject *s)
{
    return Py_BuildValue("i", headerGetInstance(s->h));
}

static PyObject *hdrIsSource(hdrObject *s)
{
    return PyBool_FromLong(headerIsSource(s->h));
}

static PyObject *hdrHasKey(hdrObject *s, PyObject *pytag)
{
    rpmTag tag;
    if (!tagNumFromPyObject(pytag, &tag)) return NULL;

    return PyBool_FromLong(headerIsEntry(s->h, tag));
}

static PyObject *hdrConvert(hdrObject *self, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = {"op", NULL};
    headerConvOps op = -1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &op)) {
        return NULL;
    }
    return PyBool_FromLong(headerConvert(self->h, op));
}

static PyObject * hdrWrite(hdrObject *s, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "file", "magic", NULL };
    int magic = 1;
    rpmfdObject *fdo = NULL;
    int rc;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|i", kwlist,
				     rpmfdFromPyObject, &fdo, &magic))
	return NULL;

    Py_BEGIN_ALLOW_THREADS;
    rc = headerWrite(rpmfdGetFd(fdo), s->h,
		     magic ? HEADER_MAGIC_YES : HEADER_MAGIC_NO);
    Py_END_ALLOW_THREADS;

    if (rc) PyErr_SetFromErrno(PyExc_IOError);
    Py_XDECREF(fdo); /* avoid messing up errno with file close  */
    if (rc) return NULL;

    Py_RETURN_NONE;
}

static PyObject * hdr_fiFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
{
    /* XXX this isn't quite right wrt arg passing */
    return PyObject_Call((PyObject *) &rpmfi_Type,
			 Py_BuildValue("(O)", s), kwds);
}

static PyObject * hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
{
    /* XXX this isn't quite right wrt arg passing */
    return PyObject_Call((PyObject *) &rpmds_Type,
			 Py_BuildValue("(O)", s), kwds);
}

static PyObject * hdr_dsOfHeader(PyObject * s)
{
    return PyObject_Call((PyObject *) &rpmds_Type,
			Py_BuildValue("(Oi)", s, RPMTAG_NEVR), NULL);
}

static int hdr_compare(hdrObject * a, hdrObject * b)
{
    return rpmVersionCompare(a->h, b->h);
}

static long hdr_hash(PyObject * h)
{
    return (long) h;
}

static struct PyMethodDef hdr_methods[] = {
    {"keys",		(PyCFunction) hdrKeyList,	METH_NOARGS,
	NULL },
    {"unload",		(PyCFunction) hdrUnload,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"expandFilelist",	(PyCFunction) hdrExpandFilelist,METH_NOARGS,
	NULL },
    {"compressFilelist",(PyCFunction) hdrCompressFilelist,METH_NOARGS,
	NULL },
    {"fullFilelist",	(PyCFunction) hdrFullFilelist,	METH_NOARGS,
	NULL },
    {"convert",		(PyCFunction) hdrConvert,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"format",		(PyCFunction) hdrFormat,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"has_key",		(PyCFunction) hdrHasKey,	METH_O,
	NULL },
    {"sprintf",		(PyCFunction) hdrFormat,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"instance",	(PyCFunction)hdrInstance,	METH_NOARGS,
	NULL },
    {"isSource",	(PyCFunction)hdrIsSource,	METH_NOARGS, 
	NULL },
    {"write",		(PyCFunction)hdrWrite,		METH_VARARGS|METH_KEYWORDS,
	NULL },
    {"dsOfHeader",	(PyCFunction)hdr_dsOfHeader,	METH_NOARGS,
	NULL},
    {"dsFromHeader",	(PyCFunction)hdr_dsFromHeader,	METH_VARARGS|METH_KEYWORDS,
	NULL},
    {"fiFromHeader",	(PyCFunction)hdr_fiFromHeader,	METH_VARARGS|METH_KEYWORDS,
	NULL},

    {NULL,		NULL}		/* sentinel */
};

/* TODO: permit keyring check + retrofits on copy/load */
static PyObject *hdr_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
    PyObject *obj = NULL;
    rpmfdObject *fdo = NULL;
    Header h = NULL;
    char *kwlist[] = { "obj", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &obj)) {
	return NULL;
    }

    if (obj == NULL) {
	h = headerNew();
    } else if (hdrObject_Check(obj)) {
	h = headerCopy(((hdrObject*) obj)->h);
    } else if (PyBytes_Check(obj)) {
	h = headerCopyLoad(PyBytes_AsString(obj));
    } else if (rpmfdFromPyObject(obj, &fdo)) {
	Py_BEGIN_ALLOW_THREADS;
	h = headerRead(rpmfdGetFd(fdo), HEADER_MAGIC_YES);
	Py_END_ALLOW_THREADS;
	Py_XDECREF(fdo);
    } else {
    	PyErr_SetString(PyExc_TypeError, "header, blob or file expected");
	return NULL;
    }

    if (h == NULL) {
	PyErr_SetString(pyrpmError, "bad header");
	return NULL;
    }
    
    return hdr_Wrap(subtype, h);
}

static void hdr_dealloc(hdrObject * s)
{
    if (s->h) headerFree(s->h);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject * hdr_iternext(hdrObject *s)
{
    PyObject *res = NULL;
    rpmTag tag;

    if (s->hi == NULL) s->hi = headerInitIterator(s->h);

    if ((tag = headerNextTag(s->hi)) != RPMTAG_NOT_FOUND) {
	int raw = 1;
	res = PyObject_Call((PyObject *) &rpmtd_Type,
			    Py_BuildValue("(Oii)", s, tag, raw), NULL);
    } else {
	s->hi = headerFreeIterator(s->hi);
    }
    return res;
}

int tagNumFromPyObject (PyObject *item, rpmTag *tagp)
{
    rpmTag tag = RPMTAG_NOT_FOUND;

    if (PyInt_Check(item)) {
	/* XXX we should probably validate tag numbers too */
	tag = PyInt_AsLong(item);
    } else if (PyBytes_Check(item)) {
	tag = rpmTagGetValue(PyBytes_AsString(item));
    } else {
	PyErr_SetString(PyExc_TypeError, "expected a string or integer");
	return 0;
    }
    if (tag == RPMTAG_NOT_FOUND) {
	PyErr_SetString(PyExc_ValueError, "unknown header tag");
	return 0;
    }

    *tagp = tag;
    return 1;
}

static PyObject * hdrGetTag(Header h, rpmTag tag)
{
    PyObject *res = NULL;
    struct rpmtd_s td;

    /* rpmtd_AsPyObj() knows how to handle empty containers and all */
    (void) headerGet(h, tag, &td, HEADERGET_EXT);
    res = rpmtd_AsPyobj(&td);
    rpmtdFreeData(&td);
    return res;
}

static PyObject * hdr_subscript(hdrObject * s, PyObject * item)
{
    rpmTag tag;

    if (!tagNumFromPyObject(item, &tag)) return NULL;
    return hdrGetTag(s->h, tag);
}

static int hdr_ass_subscript(hdrObject *s, PyObject *key, PyObject *value)
{
    rpmTag tag;
    rpmtd td;
    if (!tagNumFromPyObject(key, &tag)) return -1;

    if (value == NULL) {
	/* XXX should failure raise key error? */
	headerDel(s->h, tag);
    } else if (rpmtdFromPyObject(value, &td)) {
	headerPut(s->h, td, HEADERPUT_DEFAULT);
    } else {
	return -1;
    }
    return 0;
}

static PyObject * hdr_getattro(hdrObject * s, PyObject * n)
{
    PyObject *res = PyObject_GenericGetAttr((PyObject *) s, n);
    if (res == NULL) {
	rpmTag tag;
	if (tagNumFromPyObject(n, &tag)) {
	    PyErr_Clear();
	    res = hdrGetTag(s->h, tag);
	}
    }
    return res;
}

static int hdr_setattro(PyObject * o, PyObject * n, PyObject * v)
{
    return PyObject_GenericSetAttr(o, n, v);
}

static PyMappingMethods hdr_as_mapping = {
	(lenfunc) 0,			/* mp_length */
	(binaryfunc) hdr_subscript,	/* mp_subscript */
	(objobjargproc)hdr_ass_subscript,/* mp_ass_subscript */
};

static char hdr_doc[] =
"";

PyTypeObject hdr_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.hdr",			/* tp_name */
	sizeof(hdrObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) hdr_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) 0, 		/* tp_getattr */
	0,				/* tp_setattr */
	(cmpfunc) hdr_compare,		/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,	 			/* tp_as_sequence */
	&hdr_as_mapping,		/* tp_as_mapping */
	hdr_hash,			/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	(getattrofunc) hdr_getattro,	/* tp_getattro */
	(setattrofunc) hdr_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	hdr_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc)hdr_iternext,	/* tp_iternext */
	hdr_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	hdr_new,			/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

PyObject * hdr_Wrap(PyTypeObject *subtype, Header h)
{
    hdrObject * hdr = (hdrObject *)subtype->tp_alloc(subtype, 0);
    if (hdr == NULL) return NULL;

    hdr->h = headerLink(h);
    return (PyObject *) hdr;
}

int hdrFromPyObject(PyObject *item, Header *hptr)
{
    if (hdrObject_Check(item)) {
	*hptr = ((hdrObject *) item)->h;
	return 1;
    } else {
	PyErr_SetString(PyExc_TypeError, "header object expected");
	return 0;
    }
}

/**
 * This assumes the order of list matches the order of the new headers, and
 * throws an exception if that isn't true.
 */
static int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag)
{
    Header h;
    HeaderIterator hi;
    rpmTag newMatch, oldMatch;
    hdrObject * hdr;
    rpm_count_t count = 0;
    int rc = 1; /* assume failure */
    rpmtd td = rpmtdNew();

    Py_BEGIN_ALLOW_THREADS
    h = headerRead(fd, HEADER_MAGIC_YES);
    Py_END_ALLOW_THREADS

    while (h) {
	if (!headerGet(h, matchTag, td, HEADERGET_MINMEM)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    goto exit;
	}
	newMatch = rpmtdTag(td);
	rpmtdFreeData(td);

	hdr = (hdrObject *) PyList_GetItem(list, count++);
	if (!hdr) goto exit;

	if (!headerGet(hdr->h, matchTag, td, HEADERGET_MINMEM)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    goto exit;
	}
	oldMatch = rpmtdTag(td);
	rpmtdFreeData(td);

	if (newMatch != oldMatch) {
	    PyErr_SetString(pyrpmError, "match tag mismatch");
	    goto exit;
	}

	for (hi = headerInitIterator(h); headerNext(hi, td); rpmtdFreeData(td))
	{
	    /* could be dupes */
	    headerDel(hdr->h, rpmtdTag(td));
	    headerPut(hdr->h, td, HEADERPUT_DEFAULT);
	}

	headerFreeIterator(hi);
	h = headerFree(h);

	Py_BEGIN_ALLOW_THREADS
	h = headerRead(fd, HEADER_MAGIC_YES);
	Py_END_ALLOW_THREADS
    }
    rc = 0;

exit:
    rpmtdFree(td);
    return rc;
}

PyObject *
rpmMergeHeadersFromFD(PyObject * self, PyObject * args, PyObject * kwds)
{
    FD_t fd;
    int fileno;
    PyObject * list;
    int rc;
    int matchTag;
    char * kwlist[] = {"list", "fd", "matchTag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oii", kwlist, &list,
	    &fileno, &matchTag))
	return NULL;

    if (!PyList_Check(list)) {
	PyErr_SetString(PyExc_TypeError, "first parameter must be a list");
	return NULL;
    }

    fd = fdDup(fileno);

    rc = rpmMergeHeaders (list, fd, matchTag);
    Fclose(fd);

    if (rc) {
	return NULL;
    }

    Py_RETURN_NONE;
}

PyObject * versionCompare (PyObject * self, PyObject * args, PyObject * kwds)
{
    hdrObject * h1, * h2;
    char * kwlist[] = {"version0", "version1", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!O!", kwlist, &hdr_Type,
	    &h1, &hdr_Type, &h2))
	return NULL;

    return Py_BuildValue("i", hdr_compare(h1, h2));
}

static int compare_values(const char *str1, const char *str2)
{
    if (!str1 && !str2)
	return 0;
    else if (str1 && !str2)
	return 1;
    else if (!str1 && str2)
	return -1;
    return rpmvercmp(str1, str2);
}

PyObject * labelCompare (PyObject * self, PyObject * args)
{
    char *v1, *r1, *v2, *r2;
    const char *e1, *e2;
    int rc;

    if (!PyArg_ParseTuple(args, "(zzz)(zzz)",
			&e1, &v1, &r1, &e2, &v2, &r2))
	return NULL;

    if (e1 == NULL)	e1 = "0";
    if (e2 == NULL)	e2 = "0";

    rc = compare_values(e1, e2);
    if (!rc) {
	rc = compare_values(v1, v2);
	if (!rc)
	    rc = compare_values(r1, r2);
    }
    return Py_BuildValue("i", rc);
}

