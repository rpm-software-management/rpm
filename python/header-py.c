/** \ingroup python
 * \file python/header-py.c
 */

#include "Python.h"
#include "rpmio_internal.h"
#include "rpmcli.h"	/* XXX for rpmCheckSig */

#include "rpmfi.h"
#include "rpmts.h"	/* XXX for ts->rpmdb */

#include "legacy.h"
#include "misc.h"
#include "header_internal.h"

#include "header-py.h"

/** \ingroup python
 * \class header
 * \brief A python header object represents an RPM package header.
 * 
 * All RPM packages have headers that provide metadata for the package.
 * Header objects can be returned by database queries or loaded from a
 * binary package on disk.
 * 
 * The headerFromPackage function loads the package header from a
 * package on disk.  It returns a tuple of a "isSource" flag and the
 * header object.  The "isSource" flag is set to 1 if the package
 * header was read from a source rpm or to 0 if the package header was
 * read from a binary rpm.
 * 
 * For example:
 * \code
 * 	import os, rpm
 *  
 * 	fd = os.open("/tmp/foo-1.0-1.i386.rpm", os.O_RDONLY)
 * 	(header, isSource) = rpm.headerFromPackage(fd)
 * 	fd.close()
 * \endcode
 * The Python interface to the header data is quite elegant.  It
 * presents the data in a dictionary form.  We'll take the header we
 * just loaded and access the data within it:
 * \code
 * 	print header[rpm.RPMTAG_NAME]
 * 	print header[rpm.RPMTAG_VERSION]
 * 	print header[rpm.RPMTAG_RELEASE]
 * \endcode
 * in the case of our "foor-1.0-1.i386.rpm" package, this code would
 * output:
\verbatim
  	foo
  	1.0
  	1
\endverbatim
 * You make also access the header data by string name:
 * \code
 * 	print header['name']
 * \endcode
 * This method of access is a bit slower because the name must be
 * translated into the tag number dynamically. You also must make sure
 * the strings in header lookups don't get translated, or the lookups
 * will fail.
 */

/** \ingroup python
 * \name Class: header
 */
/*@{*/

/** \ingroup python
 */
struct hdrObject_s {
    PyObject_HEAD;
    Header h;
    char ** md5list;
    char ** fileList;
    char ** linkList;
    int_32 * fileSizes;
    int_32 * mtimes;
    int_32 * uids, * gids;	/* XXX these tags are not used anymore */
    unsigned short * rdevs;
    unsigned short * modes;
} ;

/*@unused@*/ static inline Header headerAllocated(Header h) {
    h->flags |= HEADERFLAG_ALLOCATED;
    return 0;
}

/** \ingroup python
 */
static PyObject * hdrKeyList(hdrObject * s, PyObject * args) {
    PyObject * list, *o;
    HeaderIterator iter;
    int tag, type;

    if (!PyArg_ParseTuple(args, "")) return NULL;

    list = PyList_New(0);

    iter = headerInitIterator(s->h);
    while (headerNextIterator(iter, &tag, &type, NULL, NULL)) {
        if (tag == HEADER_I18NTABLE) continue;

	switch (type) {
	  case RPM_BIN_TYPE:
	  case RPM_INT32_TYPE:
	  case RPM_CHAR_TYPE:
	  case RPM_INT8_TYPE:
	  case RPM_INT16_TYPE:
	  case RPM_STRING_ARRAY_TYPE:
	  case RPM_STRING_TYPE:
	    PyList_Append(list, o=PyInt_FromLong(tag));
	    Py_DECREF(o);
	}
    }

    headerFreeIterator(iter);

    return list;
}

/** \ingroup python
 */
static PyObject * hdrUnload(hdrObject * s, PyObject * args, PyObject *keywords) {
    char * buf;
    PyObject * rc;
    int len, legacy = 0;
    Header h;
    static char *kwlist[] = { "legacyHeader", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|i", kwlist, &legacy))
	return NULL; 

    h = headerLink(s->h, "hdrUnload h");
    /* XXX this legacy switch is a hack, needs to be removed. */
    if (legacy) {
	h = headerCopy(s->h);	/* XXX strip region tags, etc */
	headerFree(s->h, "hdrUnload s->h");
    }
    len = headerSizeof(h, 0);
    buf = headerUnload(h);
    h = headerFree(h, "hdrUnload h");

    if (buf == NULL || len == 0) {
	PyErr_SetString(pyrpmError, "can't unload bad header\n");
	return NULL;
    }

    rc = PyString_FromStringAndSize(buf, len);
    free(buf);

    return rc;
}

/** \ingroup python
 * Returns a list of these tuples for each item that failed:
 *	(attr_name, correctValue, currentValue)
 * It should be passed the file number to verify.
 */
static PyObject * hdrVerifyFile(hdrObject * s, PyObject * args) {
    int fileNumber;
    rpmVerifyAttrs verifyResult = 0;
    PyObject * list, * tuple, * attrName;
    int type, count;
    struct stat sb;
    char buf[2048];
    int i;
    time_t timeInt;
    struct tm * timeStruct;

    if (!PyInt_Check(args)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    fileNumber = (int) PyInt_AsLong(args);

    {	rpmTransactionSet ts;
	int scareMem = 1;
	TFI_t fi;
	int rc;

	ts = rpmtransCreateSet(NULL, NULL);
	fi = fiNew(ts, NULL, s->h, RPMTAG_BASENAMES, scareMem);
	fi = tfiInit(fi, fileNumber);
	if (fi != NULL && tfiNext(fi) >= 0) {
	    /* XXX this routine might use callbacks intelligently. */
	    rc = rpmVerifyFile(ts, fi, &verifyResult, RPMVERIFY_NONE);
	} else
	    rc = 1;

	fi = fiFree(fi, 1);
	rpmtransFree(ts);

	if (rc) {
	    Py_INCREF(Py_None);
	    return Py_None;
	}
    }

    list = PyList_New(0);

    if (!verifyResult) return list;

    /* XXX Legacy tag needs to go away. */
    if (!s->fileList) {
	headerGetEntry(s->h, RPMTAG_OLDFILENAMES, &type, (void **) &s->fileList,
		 &count);
    }

    lstat(s->fileList[fileNumber], &sb);

    if (verifyResult & RPMVERIFY_MD5) {
	if (!s->md5list) {
	    headerGetEntry(s->h, RPMTAG_FILEMD5S, &type, (void **) &s->md5list,
		     &count);
	}

	if (domd5(s->fileList[fileNumber], buf, 1)) {
	    strcpy(buf, "00000000000000000000000000000000");
	}

	tuple = PyTuple_New(3);
	attrName = PyString_FromString("checksum");
	PyTuple_SetItem(tuple, 0, attrName);
	PyTuple_SetItem(tuple, 1, PyString_FromString(s->md5list[fileNumber]));
	PyTuple_SetItem(tuple, 2, PyString_FromString(buf));
	PyList_Append(list, tuple);
	Py_DECREF(tuple);
    }

    if (verifyResult & RPMVERIFY_FILESIZE) {
	if (!s->fileSizes) {
	    headerGetEntry(s->h, RPMTAG_FILESIZES, &type, (void **) &s->fileSizes,
		     &count);

	}

	tuple = PyTuple_New(3);
	attrName = PyString_FromString("size");
	PyTuple_SetItem(tuple, 0, attrName);

	sprintf(buf, "%d", 100);
	PyTuple_SetItem(tuple, 1, PyString_FromString(buf));
	sprintf(buf, "%ld", (long)sb.st_size);
	PyTuple_SetItem(tuple, 2, PyString_FromString(buf));
	PyList_Append(list, tuple);
	Py_DECREF(tuple);
    }

    if (verifyResult & RPMVERIFY_LINKTO) {
	if (!s->linkList) {
	    headerGetEntry(s->h, RPMTAG_FILELINKTOS, &type, (void **) &s->linkList,
		     &count);
	}

	i = readlink(s->fileList[fileNumber], buf, sizeof(buf));
	if (i <= 0)
	    strcpy(buf, "(unknown)");
	else
	    buf[i] = '\0';

	tuple = PyTuple_New(3);
	attrName = PyString_FromString("link");
	PyTuple_SetItem(tuple, 0, attrName);
	PyTuple_SetItem(tuple, 1, PyString_FromString(s->linkList[fileNumber]));
	PyTuple_SetItem(tuple, 2, PyString_FromString(buf));
	PyList_Append(list, tuple);
	Py_DECREF(tuple);
    }

    if (verifyResult & RPMVERIFY_MTIME) {
	if (!s->mtimes) {
	    headerGetEntry(s->h, RPMTAG_FILEMTIMES, &type, (void **) &s->mtimes,
		     &count);
	}

	tuple = PyTuple_New(3);
	attrName = PyString_FromString("time");
	PyTuple_SetItem(tuple, 0, attrName);

	timeInt = sb.st_mtime;
	timeStruct = localtime(&timeInt);
	strftime(buf, sizeof(buf) - 1, "%c", timeStruct);
	PyTuple_SetItem(tuple, 1, PyString_FromString(buf));

	timeInt = s->mtimes[fileNumber];
	timeStruct = localtime(&timeInt);
	strftime(buf, sizeof(buf) - 1, "%c", timeStruct);

	PyTuple_SetItem(tuple, 2, PyString_FromString(buf));

	PyList_Append(list, tuple);
	Py_DECREF(tuple);
    }

    if (verifyResult & RPMVERIFY_RDEV) {
	if (!s->rdevs) {
	    headerGetEntry(s->h, RPMTAG_FILERDEVS, &type, (void **) &s->rdevs,
		     &count);
	}

	tuple = PyTuple_New(3);
	attrName = PyString_FromString("device");

	PyTuple_SetItem(tuple, 0, attrName);
	sprintf(buf, "0x%-4x", s->rdevs[fileNumber]);
	PyTuple_SetItem(tuple, 1, PyString_FromString(buf));
	sprintf(buf, "0x%-4x", (unsigned int) sb.st_rdev);
	PyTuple_SetItem(tuple, 2, PyString_FromString(buf));
	PyList_Append(list, tuple);
	Py_DECREF(tuple);
    }

    /*
     * RPMVERIFY_USER and RPM_VERIFY_GROUP are handled wrong here, but rpmlib.a
     * doesn't do these correctly either. At least this is consistent.
     *
     * XXX Consistent? rpmlib.a verifies user/group quite well, thank you.
     * XXX The code below does nothing useful. FILEUSERNAME needs to be
     * XXX retrieved and looked up.
     */
    if (verifyResult & RPMVERIFY_USER) {
	if (!s->uids) {
	    headerGetEntry(s->h, RPMTAG_FILEUIDS, &type, (void **) &s->uids,
		     &count);
	}

	tuple = PyTuple_New(3);
	attrName = PyString_FromString("uid");
	PyTuple_SetItem(tuple, 0, attrName);
	sprintf(buf, "%d", s->uids[fileNumber]);
	PyTuple_SetItem(tuple, 1, PyString_FromString(buf));
	sprintf(buf, "%d", sb.st_uid);
	PyTuple_SetItem(tuple, 2, PyString_FromString(buf));
	PyList_Append(list, tuple);
	Py_DECREF(tuple);
    }

    /*
     * XXX The code below does nothing useful. FILEGROUPNAME needs to be
     * XXX retrieved and looked up.
     */
    if (verifyResult & RPMVERIFY_GROUP) {
	if (!s->gids) {
	    headerGetEntry(s->h, RPMTAG_FILEGIDS, &type, (void **) &s->gids,
		     &count);
	}

	tuple = PyTuple_New(3);
	attrName = PyString_FromString("gid");
	PyTuple_SetItem(tuple, 0, attrName);
	sprintf(buf, "%d", s->gids[fileNumber]);
	PyTuple_SetItem(tuple, 1, PyString_FromString(buf));
	sprintf(buf, "%d", sb.st_gid);
	PyTuple_SetItem(tuple, 2, PyString_FromString(buf));
	PyList_Append(list, tuple);
	Py_DECREF(tuple);
    }

    if (verifyResult & RPMVERIFY_MODE) {
	if (!s->modes) {
	    headerGetEntry(s->h, RPMTAG_FILEMODES, &type, (void **) &s->modes,
		     &count);
	}

	tuple = PyTuple_New(3);
	attrName = PyString_FromString("permissions");
	PyTuple_SetItem(tuple, 0, attrName);
	sprintf(buf, "0%-4o", s->modes[fileNumber]);
	PyTuple_SetItem(tuple, 1, PyString_FromString(buf));
	sprintf(buf, "0%-4o", sb.st_mode);
	PyTuple_SetItem(tuple, 2, PyString_FromString(buf));
	PyList_Append(list, tuple);
	Py_DECREF(tuple);
    }

    return list;
}

/** \ingroup python
 */
static PyObject * hdrExpandFilelist(hdrObject * s, PyObject * args) {
    expandFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * hdrCompressFilelist(hdrObject * s, PyObject * args) {
    compressFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}

/* make a header with _all_ the tags we need */
/** \ingroup python
 */
static void mungeFilelist(Header h)
{
    const char ** fileNames = NULL;
    int count = 0;

    if (!headerIsEntry (h, RPMTAG_BASENAMES)
	|| !headerIsEntry (h, RPMTAG_DIRNAMES)
	|| !headerIsEntry (h, RPMTAG_DIRINDEXES))
	compressFilelist(h);
    
    rpmBuildFileList(h, &fileNames, &count);

    if (fileNames == NULL || count <= 0)
	return;

    /* XXX Legacy tag needs to go away. */
    headerAddEntry(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			fileNames, count);

    free((void *)fileNames);
}

/** 
 */
static PyObject * rhnUnload(hdrObject * s, PyObject * args) {
    int len;
    char * uh;
    PyObject * rc;
    Header h;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    h = headerLink(s->h, "rhnUnload h");

    /* Retrofit a RHNPlatform: tag. */
    if (!headerIsEntry(h, RPMTAG_RHNPLATFORM)) {
	const char * arch;
	int_32 at;
	if (headerGetEntry(h, RPMTAG_ARCH, &at, (void **)&arch, NULL))
	    headerAddEntry(h, RPMTAG_RHNPLATFORM, at, arch, 1);
    }

    /* Legacy headers are forced into immutable region. */
    if (!headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	Header nh = headerReload(h, RPMTAG_HEADERIMMUTABLE);
	/* XXX Another unload/load cycle to "seal" the immutable region. */
	uh = headerUnload(nh);
	headerFree(nh, "rhnUnload nh");
	h = headerLoad(uh);
	headerAllocated(h);
    }

    /* All headers have SHA1 digest, compute and add if necessary. */
    if (!headerIsEntry(h, RPMTAG_SHA1HEADER)) {
	int_32 uht, uhc;
	const char * digest;
        size_t digestlen;
        DIGEST_CTX ctx;

	headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, (void **)&uh, &uhc);

	ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
        rpmDigestUpdate(ctx, uh, uhc);
        rpmDigestFinal(ctx, (void **)&digest, &digestlen, 1);

	headerAddEntry(h, RPMTAG_SHA1RHN, RPM_STRING_TYPE, digest, 1);

	uh = headerFreeData(uh, uht);
	digest = _free(digest);
    }

    len = headerSizeof(h, 0);
    uh = headerUnload(h);
    headerFree(h, "rhnUnload h");

    rc = PyString_FromStringAndSize(uh, len);
    free(uh);

    return rc;
}

/** \ingroup python
 */
static PyObject * hdrFullFilelist(hdrObject * s, PyObject * args) {
    if (!PyArg_ParseTuple(args, ""))
	return NULL;

    mungeFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * hdrSprintf(hdrObject * s, PyObject * args) {
    char * fmt;
    char * r;
    errmsg_t err;
    PyObject * result;

    if (!PyArg_ParseTuple(args, "s", &fmt))
	return NULL;

    r = headerSprintf(s->h, fmt, rpmTagTable, rpmHeaderFormats, &err);
    if (!r) {
	PyErr_SetString(pyrpmError, err);
	return NULL;
    }

    result = Py_BuildValue("s", r);
    free(r);

    return result;
}


/** \ingroup python
 */
static struct PyMethodDef hdrMethods[] = {
	{"keys",	(PyCFunction) hdrKeyList,	1 },
	{"unload",	(PyCFunction) hdrUnload,	METH_VARARGS|METH_KEYWORDS },
	{"verifyFile",	(PyCFunction) hdrVerifyFile,	1 },
	{"expandFilelist",	(PyCFunction) hdrExpandFilelist,	1 },
	{"compressFilelist",	(PyCFunction) hdrCompressFilelist,	1 },
	{"fullFilelist",	(PyCFunction) hdrFullFilelist,	1 },
	{"rhnUnload",	(PyCFunction) rhnUnload, METH_VARARGS },
	{"sprintf",     (PyCFunction) hdrSprintf, METH_VARARGS },
	{NULL,		NULL}		/* sentinel */
};

/** \ingroup python
 */
static PyObject * hdrGetAttr(hdrObject * s, char * name) {
    return Py_FindMethod(hdrMethods, (PyObject * ) s, name);
}

/** \ingroup python
 */
static void hdrDealloc(hdrObject * s) {
    if (s->h) headerFree(s->h, "hdrDealloc s->h");
    if (s->md5list) free(s->md5list);
    if (s->fileList) free(s->fileList);
    if (s->linkList) free(s->linkList);
    PyMem_DEL(s);
}

/** \ingroup python
 */
long tagNumFromPyObject (PyObject *item)
{
    char * str;
    int i;

    if (PyInt_Check(item)) {
	return PyInt_AsLong(item);
    } else if (PyString_Check(item)) {
	str = PyString_AsString(item);
	for (i = 0; i < rpmTagTableSize; i++)
	    if (!xstrcasecmp(rpmTagTable[i].name + 7, str)) break;
	if (i < rpmTagTableSize) return rpmTagTable[i].val;
    }
    return -1;
}

/** \ingroup python
 */
static PyObject * hdrSubscript(hdrObject * s, PyObject * item) {
    int type, count, i, tag = -1;
    void * data;
    PyObject * o, * metao;
    char ** stringArray;
    int forceArray = 0;
    int freeData = 0;
    char * str;
    struct headerSprintfExtension_s * ext = NULL;
    const struct headerSprintfExtension_s * extensions = rpmHeaderFormats;

    if (PyCObject_Check (item))
        ext = PyCObject_AsVoidPtr(item);
    else
	tag = tagNumFromPyObject (item);
    if (tag == -1 && PyString_Check(item)) {
	/* if we still don't have the tag, go looking for the header
	   extensions */
	str = PyString_AsString(item);
	while (extensions->name) {
	    if (extensions->type == HEADER_EXT_TAG
		&& !xstrcasecmp(extensions->name + 7, str)) {
		(const struct headerSprintfExtension *) ext = extensions;
	    }
	    extensions++;
	}
    }

    if (ext) {
        ext->u.tagFunction(s->h, &type, (const void **) &data, &count, &freeData);
    } else {
        if (tag == -1) {
            PyErr_SetString(PyExc_KeyError, "unknown header tag");
            return NULL;
        }
        
        if (!rpmHeaderGetEntry(s->h, tag, &type, &data, &count)) {
            Py_INCREF(Py_None);
            return Py_None;
        }
    }

    switch (tag) {
      case RPMTAG_OLDFILENAMES:
      case RPMTAG_FILESIZES:
      case RPMTAG_FILESTATES:
      case RPMTAG_FILEMODES:
      case RPMTAG_FILEUIDS:
      case RPMTAG_FILEGIDS:
      case RPMTAG_FILERDEVS:
      case RPMTAG_FILEMTIMES:
      case RPMTAG_FILEMD5S:
      case RPMTAG_FILELINKTOS:
      case RPMTAG_FILEFLAGS:
      case RPMTAG_ROOT:
      case RPMTAG_FILEUSERNAME:
      case RPMTAG_FILEGROUPNAME:
	forceArray = 1;
	break;
      case RPMTAG_SUMMARY:
      case RPMTAG_GROUP:
      case RPMTAG_DESCRIPTION:
	freeData = 1;
	break;
      default:
        break;
    }

    switch (type) {
      case RPM_BIN_TYPE:
	o = PyString_FromStringAndSize(data, count);
	break;

      case RPM_INT32_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((int *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((int *) data));
	}
	break;

      case RPM_CHAR_TYPE:
      case RPM_INT8_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((char *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((char *) data));
	}
	break;

      case RPM_INT16_TYPE:
	if (count != 1 || forceArray) {
	    metao = PyList_New(0);
	    for (i = 0; i < count; i++) {
		o = PyInt_FromLong(((short *) data)[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyInt_FromLong(*((short *) data));
	}
	break;

      case RPM_STRING_ARRAY_TYPE:
	stringArray = data;

	metao = PyList_New(0);
	for (i = 0; i < count; i++) {
	    o = PyString_FromString(stringArray[i]);
	    PyList_Append(metao, o);
	    Py_DECREF(o);
	}
	free (stringArray);
	o = metao;
	break;

      case RPM_STRING_TYPE:
	if (count != 1 || forceArray) {
	    stringArray = data;

	    metao = PyList_New(0);
	    for (i=0; i < count; i++) {
		o = PyString_FromString(stringArray[i]);
		PyList_Append(metao, o);
		Py_DECREF(o);
	    }
	    o = metao;
	} else {
	    o = PyString_FromString(data);
	    if (freeData)
		free (data);
	}
	break;

      default:
	PyErr_SetString(PyExc_TypeError, "unsupported type in header");
	return NULL;
    }

    return o;
}

/** \ingroup python
 */
static PyMappingMethods hdrAsMapping = {
	(inquiry) 0,			/* mp_length */
	(binaryfunc) hdrSubscript,	/* mp_subscript */
	(objobjargproc)0,		/* mp_ass_subscript */
};

/** \ingroup python
 */
PyTypeObject hdrType = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"header",			/* tp_name */
	sizeof(hdrObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) hdrDealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) hdrGetAttr, 	/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,	 			/* tp_as_sequence */
	&hdrAsMapping,			/* tp_as_mapping */
};

hdrObject * createHeaderObject(Header h) {
    hdrObject * ho;

    ho = PyObject_NEW(hdrObject, &hdrType);
    ho->h = headerLink(h, NULL);
    ho->fileList = ho->linkList = ho->md5list = NULL;
    ho->uids = ho->gids = ho->mtimes = ho->fileSizes = NULL;
    ho->modes = ho->rdevs = NULL;

    return ho;
}

Header hdrGetHeader(hdrObject * h) {
    return h->h;
}

/**
 */
PyObject * rpmHeaderFromPackage(PyObject * self, PyObject * args) {
    hdrObject * h;
    Header header;
    FD_t fd;
    int rawFd;
    int isSource = 0;
    rpmRC rc;

    if (!PyArg_ParseTuple(args, "i", &rawFd)) return NULL;

    fd = fdDup(rawFd);
    {	rpmTransactionSet ts;
	ts = rpmtransCreateSet(NULL, NULL);
	rc = rpmReadPackageFile(ts, fd, "rpmHeaderFromPackage", &header);
	rpmtransFree(ts);
    }
    Fclose(fd);

    switch (rc) {
    case RPMRC_BADSIZE:
    case RPMRC_OK:
	h = (hdrObject *) PyObject_NEW(PyObject, &hdrType);
	h->h = header;
	h->fileList = h->linkList = h->md5list = NULL;
	h->uids = h->gids = h->mtimes = h->fileSizes = NULL;
	h->modes = h->rdevs = NULL;
	isSource = headerIsEntry(header, RPMTAG_SOURCEPACKAGE);
	break;

    case RPMRC_BADMAGIC:
	Py_INCREF(Py_None);
	h = (hdrObject *) Py_None;
	break;

    case RPMRC_FAIL:
    case RPMRC_SHORTREAD:
    default:
	PyErr_SetString(pyrpmError, "error reading package");
	return NULL;
    }

    return Py_BuildValue("(Ni)", h, isSource);
}

/**
 */
PyObject * hdrLoad(PyObject * self, PyObject * args) {
    char * obj, * copy=NULL;
    Header hdr;
    hdrObject * h;
    int len;

    if (!PyArg_ParseTuple(args, "s#", &obj, &len)) return NULL;
    
    /* malloc is needed to avoid surprises from data swab in headerLoad(). */
    copy = malloc(len);
    if (copy == NULL) {
	PyErr_SetString(pyrpmError, "out of memory");
	return NULL;
    }
    memcpy (copy, obj, len);

    hdr = headerLoad(copy);
    if (!hdr) {
	PyErr_SetString(pyrpmError, "bad header");
	return NULL;
    }
    headerAllocated(hdr);
    compressFilelist (hdr);
    providePackageNVR (hdr);

    h = (hdrObject *) PyObject_NEW(PyObject, &hdrType);
    h->h = hdr;
    h->fileList = h->linkList = h->md5list = NULL;
    h->uids = h->gids = h->mtimes = h->fileSizes = NULL;
    h->modes = h->rdevs = NULL;

    return (PyObject *) h;
}

/**
 */
PyObject * rhnLoad(PyObject * self, PyObject * args) {
    char * obj, * copy=NULL;
    Header hdr;
    hdrObject * h;
    int len;

    if (!PyArg_ParseTuple(args, "s#", &obj, &len)) return NULL;
    
    /* malloc is needed to avoid surprises from data swab in headerLoad(). */
    copy = malloc(len);
    if (copy == NULL) {
	PyErr_SetString(pyrpmError, "out of memory");
	return NULL;
    }
    memcpy (copy, obj, len);

    hdr = headerLoad(copy);
    if (!hdr) {
	PyErr_SetString(pyrpmError, "bad header");
	return NULL;
    }
    headerAllocated(hdr);

    /* XXX avoid the false OK's from rpmverifyDigest() with missing tags. */
    if (!headerIsEntry(hdr, RPMTAG_HEADERIMMUTABLE)) {
	PyErr_SetString(pyrpmError, "bad header, not immutable");
	headerFree(hdr, "rhnLoad hdr #1");
	return NULL;
    }

    /* XXX avoid the false OK's from rpmverifyDigest() with missing tags. */
    if (!headerIsEntry(hdr, RPMTAG_SHA1HEADER)
    &&  !headerIsEntry(hdr, RPMTAG_SHA1RHN)) {
	PyErr_SetString(pyrpmError, "bad header, no digest");
	headerFree(hdr, "rhnLoad hdr #2");
	return NULL;
    }

    if (rpmVerifyDigest(hdr)) {
	PyErr_SetString(pyrpmError, "bad header, digest check failed");
	headerFree(hdr, "rhnLoad hdr #3");
	return NULL;
    }

    /* Retrofit a RHNPlatform: tag. */
    if (!headerIsEntry(hdr, RPMTAG_RHNPLATFORM)) {
	const char * arch;
	int_32 at;
	if (headerGetEntry(hdr, RPMTAG_ARCH, &at, (void **)&arch, NULL))
	    headerAddEntry(hdr, RPMTAG_RHNPLATFORM, at, arch, 1);
    }

    h = createHeaderObject(hdr);

    return (PyObject *) h;
}

/**
 */
PyObject * rpmReadHeaders (FD_t fd) {
    PyObject * list;
    Header header;
    hdrObject * h;

    if (!fd) {
	PyErr_SetFromErrno(pyrpmError);
	return NULL;
    }

    list = PyList_New(0);
    Py_BEGIN_ALLOW_THREADS
    header = headerRead(fd, HEADER_MAGIC_YES);

    Py_END_ALLOW_THREADS
    while (header) {
	compressFilelist (header);
	providePackageNVR (header);
	h = (hdrObject *) PyObject_NEW(PyObject, &hdrType);
	h->h = header;
	h->fileList = h->linkList = h->md5list = NULL;
	h->uids = h->gids = h->mtimes = h->fileSizes = NULL;
	h->modes = h->rdevs = NULL;
	if (PyList_Append(list, (PyObject *) h)) {
	    Py_DECREF(list);
	    Py_DECREF(h);
	    return NULL;
	}

	Py_DECREF(h);

	Py_BEGIN_ALLOW_THREADS
	header = headerRead(fd, HEADER_MAGIC_YES);
	Py_END_ALLOW_THREADS
    }

    return list;
}

/**
 */
PyObject * rpmHeaderFromFD(PyObject * self, PyObject * args) {
    FD_t fd;
    int fileno;
    PyObject * list;

    if (!PyArg_ParseTuple(args, "i", &fileno)) return NULL;
    fd = fdDup(fileno);

    list = rpmReadHeaders (fd);
    Fclose(fd);

    return list;
}

/**
 */
PyObject * rpmHeaderFromFile(PyObject * self, PyObject * args) {
    char * filespec;
    FD_t fd;
    PyObject * list;

    if (!PyArg_ParseTuple(args, "s", &filespec)) return NULL;
    fd = Fopen(filespec, "r.fdio");

    if (!fd) {
	PyErr_SetFromErrno(pyrpmError);
	return NULL;
    }

    list = rpmReadHeaders (fd);
    Fclose(fd);

    return list;
}

/**
 * This assumes the order of list matches the order of the new headers, and
 * throws an exception if that isn't true.
 */
int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag) {
    Header newH;
    HeaderIterator iter;
    int_32 * newMatch, * oldMatch;
    hdrObject * ho;
    int count = 0;
    int type, c, tag;
    void * p;

    Py_BEGIN_ALLOW_THREADS
    newH = headerRead(fd, HEADER_MAGIC_YES);

    Py_END_ALLOW_THREADS
    while (newH) {
	if (!headerGetEntry(newH, matchTag, NULL, (void **) &newMatch, NULL)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    return 1;
	}

	ho = (hdrObject *) PyList_GetItem(list, count++);
	if (!ho) return 1;

	if (!headerGetEntry(ho->h, matchTag, NULL, (void **) &oldMatch, NULL)) {
	    PyErr_SetString(pyrpmError, "match tag missing in new header");
	    return 1;
	}

	if (*newMatch != *oldMatch) {
	    PyErr_SetString(pyrpmError, "match tag mismatch");
	    return 1;
	}

	if (ho->md5list) free(ho->md5list);
	if (ho->fileList) free(ho->fileList);
	if (ho->linkList) free(ho->linkList);

	ho->md5list = NULL;
	ho->fileList = NULL;
	ho->linkList = NULL;

	iter = headerInitIterator(newH);

	while (headerNextIterator(iter, &tag, &type, (void *) &p, &c)) {
	    /* could be dupes */
	    headerRemoveEntry(ho->h, tag);
	    headerAddEntry(ho->h, tag, type, p, c);
	    headerFreeData(p, type);
	}

	headerFreeIterator(iter);

	Py_BEGIN_ALLOW_THREADS
	newH = headerRead(fd, HEADER_MAGIC_YES);
	Py_END_ALLOW_THREADS
    }

    return 0;
}

PyObject * rpmMergeHeadersFromFD(PyObject * self, PyObject * args) {
    FD_t fd;
    int fileno;
    PyObject * list;
    int rc;
    int matchTag;

    if (!PyArg_ParseTuple(args, "Oii", &list, &fileno, &matchTag)) return NULL;

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

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
PyObject * versionCompare (PyObject * self, PyObject * args) {
    hdrObject * h1, * h2;

    if (!PyArg_ParseTuple(args, "O!O!", &hdrType, &h1, &hdrType, &h2)) return NULL;

    return Py_BuildValue("i", rpmVersionCompare(h1->h, h2->h));
}

/**
 */
PyObject * labelCompare (PyObject * self, PyObject * args) {
    char *v1, *r1, *e1, *v2, *r2, *e2;
    int rc;

    if (!PyArg_ParseTuple(args, "(zzz)(zzz)",
			  &e1, &v1, &r1,
			  &e2, &v2, &r2)) return NULL;

    if (e1 && !e2)
	return Py_BuildValue("i", 1);
    else if (!e1 && e2)
	return Py_BuildValue("i", -1);
    else if (e1 && e2) {
	int ep1, ep2;
	ep1 = atoi (e1);
	ep2 = atoi (e2);
	if (ep1 < ep2)
	    return Py_BuildValue("i", -1);
	else if (ep1 > ep2)
	    return Py_BuildValue("i", 1);
    }

    rc = rpmvercmp(v1, v2);
    if (rc)
	return Py_BuildValue("i", rc);

    return Py_BuildValue("i", rpmvercmp(r1, r2));
}

/*@}*/
