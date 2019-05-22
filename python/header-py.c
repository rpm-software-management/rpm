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
 *	   print("header is from a source package")
 *	else:
 *	   print("header is from a binary package")
 * \endcode
 *
 * The Python interface to the header data is quite elegant.  It
 * presents the data in a dictionary form.  We'll take the header we
 * just loaded and access the data within it:
 * \code
 * 	print(hdr[rpm.RPMTAG_NAME])
 * 	print(hdr[rpm.RPMTAG_VERSION])
 * 	print(hdr[rpm.RPMTAG_RELEASE])
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
 * 	print(hdr['name'])
 * 	print(hdr['version'])
 * 	print(hdr['release'])
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
    PyObject * keys;
    HeaderIterator hi;
    rpmTagVal tag;

    keys = PyList_New(0);
    if (!keys) {
        return NULL;
    }

    hi = headerInitIterator(s->h);
    while ((tag = headerNextTag(hi)) != RPMTAG_NOT_FOUND) {
	PyObject *to = PyInt_FromLong(tag);
        if (!to) {
            headerFreeIterator(hi);
            Py_DECREF(keys);
            return NULL;
        }
	PyList_Append(keys, to);
	Py_DECREF(to);
    }
    headerFreeIterator(hi);

    return keys;
}

static PyObject * hdrAsBytes(hdrObject * s)
{
    PyObject *res = NULL;
    unsigned int len = 0;
    char *buf = headerExport(s->h, &len);

    if (buf == NULL || len == 0) {
	PyErr_SetString(pyrpmError, "can't unload bad header\n");
    } else {
	res = PyBytes_FromStringAndSize(buf, len);
    }
    free(buf);
    return res;
}

static PyObject * hdrUnload(hdrObject * s)
{
    return hdrAsBytes(s);
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
    }
    rpmtdFree(fileNames);

    Py_RETURN_NONE;
}

static PyObject * hdrFormat(hdrObject * s, PyObject * args, PyObject * kwds)
{
    const char * fmt;
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

    result = utf8FromString(r);
    free(r);

    return result;
}

static PyObject *hdrIsSource(hdrObject *s)
{
    return PyBool_FromLong(headerIsSource(s->h));
}

static int hdrContains(hdrObject *s, PyObject *pytag)
{
    rpmTagVal tag;
    if (!tagNumFromPyObject(pytag, &tag)) return -1;

    return headerIsEntry(s->h, tag);
}

static PyObject *hdrConvert(hdrObject *self, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = {"op", NULL};
    int op = -1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &op)) {
        return NULL;
    }
    return PyBool_FromLong(headerConvert(self->h, op));
}

static PyObject * hdrWrite(hdrObject *s, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "file", "magic", NULL };
    int magic = HEADER_MAGIC_YES;
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

/*
 * Just a backwards-compatibility dummy, the arguments are not looked at
 * or used. TODO: push this over to python side...
 */
static PyObject * hdr_fiFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
{
    return PyObject_CallFunctionObjArgs((PyObject *) &rpmfi_Type, s, NULL);
}

/* Backwards compatibility. Flags argument is just a dummy and discarded. */
static PyObject * hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
{
    rpmTagVal tag = RPMTAG_REQUIRENAME;
    rpmsenseFlags flags = 0;
    char * kwlist[] = {"to", "flags", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O&i:dsFromHeader", kwlist,
            tagNumFromPyObject, &tag, &flags))
        return NULL;

    return PyObject_CallFunction((PyObject *) &rpmds_Type,
                                 "(Oi)", s, tag);
}

static PyObject * hdr_dsOfHeader(PyObject * s)
{
    return PyObject_CallFunction((PyObject *) &rpmds_Type,
                                 "(Oi)", s, RPMTAG_NEVR);
}

static long hdr_hash(PyObject * h)
{
    return (long) h;
}

static PyObject * hdr_reduce(hdrObject *s)
{
    PyObject *res = NULL;
    PyObject *blob = hdrAsBytes(s);
    if (blob) {
	res = Py_BuildValue("O(O)", Py_TYPE(s), blob);
	Py_DECREF(blob);
    }
    return res;
}

static struct PyMethodDef hdr_methods[] = {
    {"keys",		(PyCFunction) hdrKeyList,	METH_NOARGS,
     "hdr.keys() -- Return a list of the header's rpm tags (int RPMTAG_*)." },
    {"unload",		(PyCFunction) hdrUnload,	METH_NOARGS,
     "hdr.unload() -- Return binary representation\nof the header." },
    {"expandFilelist",	(PyCFunction) hdrExpandFilelist,METH_NOARGS,
     "DEPRECATED -- Use hdr.convert() instead." },
    {"compressFilelist",(PyCFunction) hdrCompressFilelist,METH_NOARGS,
     "DEPRECATED -- Use hdr.convert() instead." },
    {"fullFilelist",	(PyCFunction) hdrFullFilelist,	METH_NOARGS,
     "DEPRECATED -- Obsolete method."},
    {"convert",		(PyCFunction) hdrConvert,	METH_VARARGS|METH_KEYWORDS,
     "hdr.convert(op=-1) -- Convert header - See HEADERCONV_*\nfor possible values of op."},
    {"format",		(PyCFunction) hdrFormat,	METH_VARARGS|METH_KEYWORDS,
     "hdr.format(format) -- Expand a query string with the header data.\n\nSee rpm -q for syntax." },
    {"sprintf",		(PyCFunction) hdrFormat,	METH_VARARGS|METH_KEYWORDS,
     "Alias for .format()." },
    {"isSource",	(PyCFunction)hdrIsSource,	METH_NOARGS, 
     "hdr.isSource() -- Return if header describes a source package." },
    {"write",		(PyCFunction)hdrWrite,		METH_VARARGS|METH_KEYWORDS,
     "hdr.write(file, magic=True) -- Write header to file." },
    {"dsOfHeader",	(PyCFunction)hdr_dsOfHeader,	METH_NOARGS,
     "hdr.dsOfHeader() -- Return dependency set with the header's NEVR."},
    {"dsFromHeader",	(PyCFunction)hdr_dsFromHeader,	METH_VARARGS|METH_KEYWORDS,
     "hdr.dsFromHeader(to=RPMTAG_REQUIRENAME, flags=None)\nGet dependency set from header. to must be one of the NAME tags\nbelonging to a dependency:\n'Providename', 'Requirename', 'Obsoletename', 'Conflictname',\n'Triggername', 'Recommendname', 'Suggestname', 'Supplementname',\n'Enhancename' or one of the corresponding RPMTAG_*NAME constants." },
    {"fiFromHeader",	(PyCFunction)hdr_fiFromHeader,	METH_VARARGS|METH_KEYWORDS,
     "hdr.fiFromHeader() -- Return rpm.fi object containing the file\nmeta data from the header.\n\nDEPRECATED - Use rpm.files(hdr) instead."},
    {"__reduce__",	(PyCFunction)hdr_reduce,	METH_NOARGS,
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
    rpmTagVal tag;

    if (s->hi == NULL) s->hi = headerInitIterator(s->h);

    if ((tag = headerNextTag(s->hi)) != RPMTAG_NOT_FOUND) {
	res = PyInt_FromLong(tag);
    } else {
	s->hi = headerFreeIterator(s->hi);
    }
    return res;
}

PyObject * utf8FromString(const char *s)
{
/* In Python 3, we return all strings as surrogate-escaped utf-8 */
#if PY_MAJOR_VERSION >= 3
    if (s != NULL)
	return PyUnicode_DecodeUTF8(s, strlen(s), "surrogateescape");
#else
    if (s != NULL)
	return PyBytes_FromString(s);
#endif
    Py_RETURN_NONE;
}

int utf8FromPyObject(PyObject *item, PyObject **str)
{
    PyObject *res = NULL;
    if (PyBytes_Check(item)) {
	Py_XINCREF(item);
	res = item;
    } else if (PyUnicode_Check(item)) {
	res = PyUnicode_AsUTF8String(item);
    }
    if (res == NULL) return 0;

    *str = res;
    return 1;
}

int tagNumFromPyObject (PyObject *item, rpmTagVal *tagp)
{
    rpmTagVal tag = RPMTAG_NOT_FOUND;
    PyObject *str = NULL;

    if (PyInt_Check(item)) {
	/* XXX we should probably validate tag numbers too */
	tag = PyInt_AsLong(item);
    } else if (utf8FromPyObject(item, &str)) {
	tag = rpmTagGetValue(PyBytes_AsString(str));
	Py_DECREF(str);
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

static PyObject * hdrGetTag(Header h, rpmTagVal tag)
{
    PyObject *res = NULL;
    struct rpmtd_s td;

    (void) headerGet(h, tag, &td, HEADERGET_EXT);
    if (rpmtdGetFlags(&td) & RPMTD_INVALID) {
	PyErr_SetString(pyrpmError, "invalid header data");
    } else {
	/* rpmtd_AsPyobj() knows how to handle empty containers and all */
	res = rpmtd_AsPyobj(&td);
    }
    rpmtdFreeData(&td);
    return res;
}

static int validItem(rpmTagClass tclass, PyObject *item)
{
    int rc;

    switch (tclass) {
    case RPM_NUMERIC_CLASS:
	rc = (PyLong_Check(item) || PyInt_Check(item));
	break;
    case RPM_STRING_CLASS:
	rc = (PyBytes_Check(item) || PyUnicode_Check(item));
	break;
    case RPM_BINARY_CLASS:
	rc = PyBytes_Check(item);
	break;
    default:
	rc = 0;
	break;
    }
    return rc;
}

static int validData(rpmTagVal tag, rpmTagType type, rpmTagReturnType retype, PyObject *value)
{
    rpmTagClass tclass = rpmTagGetClass(tag);
    int valid = 1;
    
    if (retype == RPM_SCALAR_RETURN_TYPE) {
	valid = validItem(tclass, value);
    } else if (retype == RPM_ARRAY_RETURN_TYPE && PyList_Check(value)) {
	/* python lists can contain arbitrary objects, validate each item */
	Py_ssize_t len = PyList_Size(value);
	if (len == 0)
	    valid = 0;
	for (Py_ssize_t i = 0; i < len; i++) {
	    PyObject *item = PyList_GetItem(value, i);
	    if (!validItem(tclass, item)) {
		valid = 0;
		break;
	    }
	}
    } else {
	valid = 0;
    }
    return valid;
}

static int hdrAppendItem(Header h, rpmTagVal tag, rpmTagType type, PyObject *item)
{
    int rc = 0;

    switch (type) {
    case RPM_I18NSTRING_TYPE: /* XXX this needs to be handled separately */
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE: {
	PyObject *str = NULL;
	if (utf8FromPyObject(item, &str)) 
	    rc = headerPutString(h, tag, PyBytes_AsString(str));
	Py_XDECREF(str);
	} break;
    case RPM_BIN_TYPE: {
	uint8_t *val = (uint8_t *) PyBytes_AsString(item);
	rpm_count_t len = PyBytes_Size(item);
	rc = headerPutBin(h, tag, val, len);
	} break;
    case RPM_INT64_TYPE: {
	uint64_t val = PyInt_AsUnsignedLongLongMask(item);
	rc = headerPutUint64(h, tag, &val, 1);
	} break;
    case RPM_INT32_TYPE: {
	uint32_t val = PyInt_AsUnsignedLongMask(item);
	rc = headerPutUint32(h, tag, &val, 1);
	} break;
    case RPM_INT16_TYPE: {
	uint16_t val = PyInt_AsUnsignedLongMask(item);
	rc = headerPutUint16(h, tag, &val, 1);
	} break;
    case RPM_INT8_TYPE:
    case RPM_CHAR_TYPE: {
	uint8_t val = PyInt_AsUnsignedLongMask(item);
	rc = headerPutUint8(h, tag, &val, 1);
	} break;
    default:
	PyErr_SetString(PyExc_TypeError, "unhandled datatype");
    }
    return rc;
}

static int hdrPutTag(Header h, rpmTagVal tag, PyObject *value)
{
    rpmTagType type = rpmTagGetTagType(tag);
    rpmTagReturnType retype = rpmTagGetReturnType(tag);
    int rc = 0;

    /* XXX this isn't really right (i18n strings etc) but for now ... */
    if (headerIsEntry(h, tag)) {
	PyErr_SetString(PyExc_TypeError, "tag already exists");
	return rc;
    }

    /* validate all data before trying to insert */
    if (!validData(tag, type, retype, value)) { 
	PyErr_SetString(PyExc_TypeError, "invalid type for tag");
	return 0;
    }

    if (retype == RPM_SCALAR_RETURN_TYPE) {
	rc = hdrAppendItem(h, tag, type, value);
    } else if (retype == RPM_ARRAY_RETURN_TYPE && PyList_Check(value)) {
	Py_ssize_t len = PyList_Size(value);
	for (Py_ssize_t i = 0; i < len; i++) {
	    PyObject *item = PyList_GetItem(value, i);
	    rc = hdrAppendItem(h, tag, type, item);
	}
    } else {
	PyErr_SetString(PyExc_RuntimeError, "can't happen, right?");
    }

    return rc;
}

static PyObject * hdr_subscript(hdrObject * s, PyObject * item)
{
    rpmTagVal tag;

    if (!tagNumFromPyObject(item, &tag)) return NULL;
    return hdrGetTag(s->h, tag);
}

static int hdr_ass_subscript(hdrObject *s, PyObject *key, PyObject *value)
{
    rpmTagVal tag;
    if (!tagNumFromPyObject(key, &tag)) return -1;

    if (value == NULL) {
	/* XXX should failure raise key error? */
	headerDel(s->h, tag);
    } else if (!hdrPutTag(s->h, tag, value)) {
	return -1;
    }
    return 0;
}

static PyObject * hdr_getattro(hdrObject * s, PyObject * n)
{
    PyObject *res = PyObject_GenericGetAttr((PyObject *) s, n);
    if (res == NULL) {
	PyObject *type, *value, *traceback;
	rpmTagVal tag;

	/* Save and restore original exception if it's not a valid tag either */
	PyErr_Fetch(&type, &value, &traceback);
	if (tagNumFromPyObject(n, &tag)) {
	    Py_XDECREF(type);
	    Py_XDECREF(value);
	    Py_XDECREF(traceback);
	    res = hdrGetTag(s->h, tag);
	} else {
	    PyErr_Restore(type, value, traceback);
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

static PySequenceMethods hdr_as_sequence = {
    0,				/* sq_length */
    0,				/* sq_concat */
    0,				/* sq_repeat */
    0,				/* sq_item */
    0,				/* sq_slice */
    0,				/* sq_ass_item */
    0,				/* sq_ass_slice */
    (objobjproc)hdrContains,	/* sq_contains */
    0,				/* sq_inplace_concat */
    0,				/* sq_inplace_repeat */
};

static char hdr_doc[] =
  "A header object represents an RPM package header.\n"
  "\n"
  "All RPM packages have headers that provide metadata for the package.\n"
  "Header objects can be returned by database queries or loaded from a\n"
  "binary package on disk.\n"
  "\n"
  "The ts.hdrFromFdno() function returns the package header from a\n"
  "package on disk, verifying package signatures and digests of the\n"
  "package while reading.\n"
  "\n"
  "Note: The older method rpm.headerFromPackage() which has been replaced\n"
  "by ts.hdrFromFdno() used to return a (hdr, isSource) tuple.\n"
  "\n"
  "If you need to distinguish source/binary headers, do:\n"
  "\n"
  "	import os, rpm\n"
  "\n"
  "	ts = rpm.TransactionSet()\n"
  "	fdno = os.open('/tmp/foo-1.0-1.i386.rpm', os.O_RDONLY)\n"
  "	hdr = ts.hdrFromFdno(fdno)\n"
  "	os.close(fdno)\n"
  "	if hdr[rpm.RPMTAG_SOURCEPACKAGE]:\n"
  "	   print('header is from a source package')\n"
  "	else:\n"
  "	   print('header is from a binary package')\n"
  "\n"
  "The Python interface to the header data is quite elegant.  It\n"
  "presents the data in a dictionary form.  We'll take the header we\n"
  "just loaded and access the data within it:\n"
  "\n"
  "	print(hdr[rpm.RPMTAG_NAME])\n"
  "	print(hdr[rpm.RPMTAG_VERSION])\n"
  "	print(hdr[rpm.RPMTAG_RELEASE])\n"
  "\n"
  "in the case of our 'foo-1.0-1.i386.rpm' package, this code would\n"
  "output:\n"
  "	foo\n"
  "	1.0\n"
  "	1\n"
  "\n"
  "You make also access the header data by string name:\n"
  "\n"
  "	print(hdr['name'])\n"
  "	print(hdr['version'])\n"
  "	print(hdr['release'])\n"
  "\n"
  "This method of access is a teensy bit slower because the name must be\n"
  "translated into the tag number dynamically. You also must make sure\n"
  "the strings in header lookups don't get translated, or the lookups\n"
  "will fail.\n";

PyTypeObject hdr_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.hdr",			/* tp_name */
	sizeof(hdrObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) hdr_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) 0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	&hdr_as_sequence,		/* tp_as_sequence */
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
    hdr->h = h;
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
    rpmTagVal newMatch, oldMatch;
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

    return Py_BuildValue("i", rpmVersionCompare(h1->h, h2->h));
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
    const char *v1, *r1, *v2, *r2;
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

