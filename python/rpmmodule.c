/** \ingroup python
 * \file python/rpmmodule.c
 */

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <glob.h>	/* XXX rpmio.h */
#include <dirent.h>	/* XXX rpmio.h */
#include <locale.h>
#include <time.h>

#include "Python.h"
#include "rpmcli.h"	/* XXX for rpmCheckSig */
#include "misc.h"
#include "rpmio_internal.h"
#include "header_internal.h"
#include "upgrade.h"

extern int _rpmio_debug;

/*@unused@*/ static inline Header headerAllocated(Header h) {
    h->flags |= HEADERFLAG_ALLOCATED;
    return 0;
}

#ifdef __LCLINT__
#undef	PyObject_HEAD
#define	PyObject_HEAD	int _PyObjectHead
#endif

extern int mdfile(const char *fn, unsigned char *digest);

void initrpm(void);

/* from lib/misc.c */
int rpmvercmp(const char * one, const char * two);


/** \ingroup python
 */
typedef struct rpmdbObject_s rpmdbObject;

/** \ingroup python
 */
typedef struct rpmdbMIObject_s rpmdbMIObject;

/** \ingroup python
 */
typedef struct rpmtransObject_s rpmtransObject;

/** \ingroup python
 */
typedef struct hdrObject_s hdrObject;

/** \ingroup python
 */
static PyObject * pyrpmError;

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
 * translated into the tag number dynamically.
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
    Header sigs;		/* XXX signature tags are in header */
    char ** md5list;
    char ** fileList;
    char ** linkList;
    int_32 * fileSizes;
    int_32 * mtimes;
    int_32 * uids, * gids;	/* XXX these tags are not used anymore */
    unsigned short * rdevs;
    unsigned short * modes;
} ;

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

    /* XXX this routine might use callbacks intelligently. */
    if (rpmVerifyFile("", s->h, fileNumber, &verifyResult, RPMVERIFY_NONE)) {
	Py_INCREF(Py_None);
	return Py_None;
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

	if (mdfile(s->fileList[fileNumber], buf)) {
	    strcpy(buf, "(unknown)");
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
	sprintf(buf, "%ld", sb.st_size);
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
static PyObject * rhnUnload(PyObject * self, PyObject * args) {
    int len;
    char * uh;
    PyObject * rc;
    hdrObject *s;
    Header h;

    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    h = headerLink(s->h);

    /* Legacy headers are forced into immutable region. */
    if (!headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	Header nh = headerReload(h, RPMTAG_HEADERIMMUTABLE);
	/* XXX Another unload/load cycle to "seal" the immutable region. */
	uh = headerUnload(nh);
	headerFree(nh);
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

	ctx = rpmDigestInit(RPMDIGEST_SHA1);
        rpmDigestUpdate(ctx, uh, uhc);
        rpmDigestFinal(ctx, (void **)&digest, &digestlen, 1);

	headerAddEntry(h, RPMTAG_SHA1RHN, RPM_STRING_TYPE, digest, 1);

	uh = headerFreeData(uh, uht);
	digest = _free(digest);
    }

    len = headerSizeof(h, 0);
    uh = headerUnload(h);
    headerFree(h);

    rc = PyString_FromStringAndSize(uh, len);
    free(uh);

    return rc;
}

/** \ingroup python
 */
static PyObject * hdrFullFilelist(hdrObject * s, PyObject * args) {
    mungeFilelist (s->h);

    Py_INCREF(Py_None);
    return Py_None;
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
	{"rhnUnload",	(PyCFunction) rhnUnload, 1 },
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
    if (s->h) headerFree(s->h);
    if (s->sigs) headerFree(s->sigs);
    if (s->md5list) free(s->md5list);
    if (s->fileList) free(s->fileList);
    if (s->linkList) free(s->linkList);
    PyMem_DEL(s);
}

/** \ingroup python
 */
static long tagNumFromPyObject (PyObject *item)
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
        
	/* XXX signature tags are appended to header, this API is gonna die */
        if (!rpmPackageGetEntry(NULL, s->sigs, s->h, tag, &type, &data, &count))
	{
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
static PyTypeObject hdrType = {
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

/*@}*/

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
struct rpmdbObject_s {
    PyObject_HEAD;
    rpmdb db;
    int offx;
    int noffs;
    int *offsets;
} ;

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
    

    h = rpmdbNextIterator(s->mi);
    if (!h) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    ho = PyObject_NEW(hdrObject, &hdrType);
    ho->h = headerLink(h);
    ho->sigs = NULL;
    ho->fileList = ho->linkList = ho->md5list = NULL;
    ho->uids = ho->gids = ho->mtimes = ho->fileSizes = NULL;
    ho->modes = ho->rdevs = NULL;
    
    return (PyObject *) ho;
}

/** \ingroup python
 */
static struct PyMethodDef rpmdbMIMethods[] = {
	{"next",	    (PyCFunction) rpmdbMINext,	1 },
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
static PyTypeObject rpmdbMIType = {
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
    hdrObject * h;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    offset = (int) PyInt_AsLong(key);

    h = PyObject_NEW(hdrObject, &hdrType);
    h->h = NULL;
    h->sigs = NULL;
    {	rpmdbMatchIterator mi;
	mi = rpmdbInitIterator(s->db, RPMDBI_PACKAGES, &offset, sizeof(offset));
	if ((h->h = rpmdbNextIterator(mi)) != NULL)
	    h->h = headerLink(h->h);
	rpmdbFreeIterator(mi);
    }
    h->fileList = h->linkList = h->md5list = NULL;
    h->uids = h->gids = h->mtimes = h->fileSizes = NULL;
    h->modes = h->rdevs = NULL;
    if (!h->h) {
	Py_DECREF(h);
	PyErr_SetString(pyrpmError, "cannot read rpmdb entry");
	return NULL;
    }

    return h;
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
static PyTypeObject rpmdbType = {
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

/*@}*/

/** \ingroup python
 * \name Class: rpmtrans
 * \class rpmtrans
 * \brief A python rpmtrans object represents an RPM transaction set.
 * 
 * The transaction set is the workhorse of RPM.  It performs the
 * installation and upgrade of packages.  The rpmtrans object is
 * instantiated by the TransactionSet function in the rpm module.
 *
 * The TransactionSet function takes two optional arguments.  The first
 * argument is the root path, the second is an open database to perform
 * the transaction set upon.
 *
 * A rpmtrans object has the following methods:
 *
 * - add(header,data,mode)	Add a binary package to a transaction set.
 * @param header the header to be added
 * @param data	user data that will be passed to the transaction callback
 *		during transaction execution
 * @param mode 	optional argument that specifies if this package should
 *		be installed ('i'), upgraded ('u'), or if it is just
 *		available to the transaction when computing
 *		dependencies but no action should be performed with it
 *		('a').
 *
 * - remove
 *
 * - depcheck()	Perform a dependency and conflict check on the
 *		transaction set. After headers have been added to a
 *		transaction set, a dependency check can be performed
 *		to make sure that all package dependencies are
 *		satisfied.
 * @return	None If there are no unresolved dependencies
 *		Otherwise a list of complex tuples is returned, one tuple per
 *		unresolved dependency, with
 * The format of the dependency tuple is:
 *     ((packageName, packageVersion, packageRelease),
 *      (reqName, reqVersion),
 *      needsFlags,
 *      suggestedPackage,
 *      sense)
 *     packageName, packageVersion, packageRelease are the name,
 *     version, and release of the package that has the unresolved
 *     dependency or conflict.
 *     The reqName and reqVersion are the name and version of the
 *     requirement or conflict.
 *     The needsFlags is a bitfield that describes the versioned
 *     nature of a requirement or conflict.  The constants
 *     rpm.RPMDEP_SENSE_LESS, rpm.RPMDEP_SENSE_GREATER, and
 *     rpm.RPMDEP_SENSE_EQUAL can be logical ANDed with the needsFlags
 *     to get versioned dependency information.
 *     suggestedPackage is a tuple if the dependency check was aware
 *     of a package that solves this dependency problem when the
 *     dependency check was run.  Packages that are added to the
 *     transaction set as "available" are examined during the
 *     dependency check as possible dependency solvers. The tuple
 *     contains two values, (header, suggestedName).  These are set to
 *     the header of the suggested package and its name, respectively.
 *     If there is no known package to solve the dependency problem,
 *     suggestedPackage is None.
 *     The constants rpm.RPMDEP_SENSE_CONFLICTS and
 *     rpm.RPMDEP_SENSE_REQUIRES are set to show a dependency as a
 *     requirement or a conflict.
 *
 * - run(flags,problemSetFilter,callback,data) Attempt to execute a
 *	transaction set. After the transaction set has been populated
 *	with install and upgrade actions, it can be executed by invoking
 *	the run() method.
 * @param flags - modifies the behavior of the transaction set as it is
 *		processed.  The following values can be locical ORed
 *		together:
 *	- rpm.RPMTRANS_FLAG_TEST - test mode, do not modify the RPM
 *		database, change any files, or run any package scripts
 *	- rpm.RPMTRANS_FLAG_BUILD_PROBS - only build a list of
 *		problems encountered when attempting to run this transaction
 *		set
 *	- rpm.RPMTRANS_FLAG_NOSCRIPTS - do not execute package scripts
 *	- rpm.RPMTRANS_FLAG_JUSTDB - only make changes to the rpm
 *		database, do not modify files.
 *	- rpm.RPMTRANS_FLAG_NOTRIGGERS - do not run trigger scripts
 *	- rpm.RPMTRANS_FLAG_NODOCS - do not install files marked as %doc
 *	- rpm.RPMTRANS_FLAG_ALLFILES - create all files, even if a
 *		file is marked %config(missingok) and an upgrade is
 *		being performed.
 *	- rpm.RPMTRANS_FLAG_KEEPOBSOLETE - do not remove obsoleted
 *		packages.
 * @param problemSetFilter - control bit(s) to ignore classes of problems,
 *		any of
 *	- rpm.RPMPROB_FILTER_IGNOREOS - 
 *	- rpm.RPMPROB_FILTER_IGNOREARCH - 
 *	- rpm.RPMPROB_FILTER_REPLACEPKG - 
 *	- rpm.RPMPROB_FILTER_FORCERELOCATE - 
 *	- rpm.RPMPROB_FILTER_REPLACENEWFILES - 
 *	- rpm.RPMPROB_FILTER_REPLACEOLDFILES - 
 *	- rpm.RPMPROB_FILTER_OLDPACKAGE - 
 *	- rpm.RPMPROB_FILTER_DISKSPACE - 
 */

/** \ingroup python
 * \name Class: rpmtrans
 */
/*@{*/

/** \ingroup python
 */
struct rpmtransObject_s {
    PyObject_HEAD;
    rpmdbObject * dbo;
    rpmTransactionSet ts;
    PyObject * keyList;			/* keeps reference counts correct */
    FD_t scriptFd;
} ;

/** \ingroup python
 */
static PyObject * rpmtransAdd(rpmtransObject * s, PyObject * args) {
    hdrObject * h;
    PyObject * key;
    char * how = NULL;
    int isUpgrade = 0;

    if (!PyArg_ParseTuple(args, "OO|s", &h, &key, &how)) return NULL;
    if (h->ob_type != &hdrType) {
	PyErr_SetString(PyExc_TypeError, "bad type for header argument");
	return NULL;
    }

    if (how && strcmp(how, "a") && strcmp(how, "u") && strcmp(how, "i")) {
	PyErr_SetString(PyExc_TypeError, "how argument must be \"u\", \"a\", or \"i\"");
	return NULL;
    } else if (how && !strcmp(how, "u"))
    	isUpgrade = 1;

    if (how && !strcmp(how, "a"))
	rpmtransAvailablePackage(s->ts, h->h, key);
    else
	rpmtransAddPackage(s->ts, h->h, NULL, key, isUpgrade, NULL);

    /* This should increment the usage count for me */
    if (key) {
	PyList_Append(s->keyList, key);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * rpmtransRemove(rpmtransObject * s, PyObject * args) {
    char * name;
    int count;
    rpmdbMatchIterator mi;
    
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    /* XXX: Copied hack from ../lib/rpminstall.c, rpmErase() */
    mi = rpmdbInitIterator(s->dbo->db, RPMDBI_LABEL, name, 0);
    count = rpmdbGetIteratorCount(mi);
    if (count <= 0) {
        PyErr_SetString(pyrpmError, "package not installed");
        return NULL;
    } else { /* XXX: Note that we automatically choose to remove all matches */
        Header h;
        while ((h = rpmdbNextIterator(mi)) != NULL) {
	    unsigned int recOffset = rpmdbGetIteratorOffset(mi);
	    if (recOffset) {
	        rpmtransRemovePackage(s->ts, recOffset);
	    }
	}
    }
    rpmdbFreeIterator(mi);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * rpmtransDepCheck(rpmtransObject * s, PyObject * args) {
    rpmDependencyConflict conflicts;
    int numConflicts;
    PyObject * list, * cf;
    int i;

    if (!PyArg_ParseTuple(args, "")) return NULL;

    rpmdepCheck(s->ts, &conflicts, &numConflicts);
    if (numConflicts) {
	list = PyList_New(0);

	/* XXX TODO: rpmlib-4.0.3 can return multiple suggested packages. */
	for (i = 0; i < numConflicts; i++) {
	    cf = Py_BuildValue("((sss)(ss)iOi)", conflicts[i].byName,
			       conflicts[i].byVersion, conflicts[i].byRelease,

			       conflicts[i].needsName,
			       conflicts[i].needsVersion,

			       conflicts[i].needsFlags,
			       conflicts[i].suggestedPackages ?
				   conflicts[i].suggestedPackages[0] : Py_None,
			       conflicts[i].sense);
	    PyList_Append(list, (PyObject *) cf);
	    Py_DECREF(cf);
	}

	conflicts = rpmdepFreeConflicts(conflicts, numConflicts);

	return list;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * rpmtransOrder(rpmtransObject * s, PyObject * args) {
    if (!PyArg_ParseTuple(args, "")) return NULL;

    rpmdepOrder(s->ts);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup python
 */
static PyObject * py_rpmtransGetKeys(rpmtransObject * s, PyObject * args) {
    const void **data = NULL;
    int num, i;
    PyObject *tuple;

    rpmtransGetKeys(s->ts, &data, &num);
    if (data == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    tuple = PyTuple_New(num);

    for (i = 0; i < num; i++) {
	PyObject *obj = (PyObject *) data[i];
	Py_INCREF(obj);
	PyTuple_SetItem(tuple, i, obj);
    }

    free (data);

    return tuple;
}

/** \ingroup python
 */
struct tsCallbackType {
    PyObject * cb;
    PyObject * data;
    int pythonError;
};

/** \ingroup python
 * @todo Remove, there's no headerLink refcount on the pointer.
 */
static Header transactionSetHeader = NULL;

/** \ingroup python
 */
static void * tsCallback(const void * hd, const rpmCallbackType what,
		         const unsigned long amount, const unsigned long total,
	                 const void * pkgKey, rpmCallbackData data) {
    struct tsCallbackType * cbInfo = data;
    PyObject * args, * result;
    int fd;
    static FD_t fdt;
    const Header h = (Header) hd;

    if (cbInfo->pythonError) return NULL;

    if (!pkgKey) pkgKey = Py_None;
    transactionSetHeader = h;    

    args = Py_BuildValue("(illOO)", what, amount, total, pkgKey, cbInfo->data);
    result = PyEval_CallObject(cbInfo->cb, args);
    Py_DECREF(args);

    if (!result) {
	cbInfo->pythonError = 1;
	return NULL;
    }

    if (what == RPMCALLBACK_INST_OPEN_FILE) {
        if (!PyArg_Parse(result, "i", &fd)) {
	    cbInfo->pythonError = 1;
	    return NULL;
	}
	fdt = fdDup(fd);
	
	Py_DECREF(result);
	return fdt;
    }

    if (what == RPMCALLBACK_INST_CLOSE_FILE) {
	Fclose (fdt);
    }

    Py_DECREF(result);

    return NULL;
}

/** \ingroup python
 */
static PyObject * rpmtransRun(rpmtransObject * s, PyObject * args) {
    int flags, ignoreSet;
    int rc, i;
    PyObject * list, * prob;
    rpmProblemSet probs;
    struct tsCallbackType cbInfo;

    if (!PyArg_ParseTuple(args, "iiOO", &flags, &ignoreSet, &cbInfo.cb,
			  &cbInfo.data))
	return NULL;

    cbInfo.pythonError = 0;

    rc = rpmRunTransactions(s->ts, tsCallback, &cbInfo, NULL, &probs, flags,
			    ignoreSet);

    if (cbInfo.pythonError) {
	if (rc > 0)
	    rpmProblemSetFree(probs);
	return NULL;
    }

    if (rc < 0) {
	list = PyList_New(0);
	return list;
    } else if (!rc) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    list = PyList_New(0);
    for (i = 0; i < probs->numProblems; i++) {
	rpmProblem myprob = probs->probs + i;
	prob = Py_BuildValue("s(isi)", rpmProblemString(myprob),
			     myprob->type,
			     myprob->str1,
			     myprob->ulong1);
	PyList_Append(list, prob);
	Py_DECREF(prob);
    }

    rpmProblemSetFree(probs);

    return list;
}

/** \ingroup python
 */
static struct PyMethodDef rpmtransMethods[] = {
	{"add",		(PyCFunction) rpmtransAdd,	1 },
	{"remove",	(PyCFunction) rpmtransRemove,	1 },
	{"depcheck",	(PyCFunction) rpmtransDepCheck,	1 },
	{"order",	(PyCFunction) rpmtransOrder,	1 },
	{"getKeys",	(PyCFunction) py_rpmtransGetKeys, 1 },
	{"run",		(PyCFunction) rpmtransRun, 1 },
	{NULL,		NULL}		/* sentinel */
};

/** \ingroup python
 */
static PyObject * rpmtransGetAttr(rpmtransObject * o, char * name) {
    return Py_FindMethod(rpmtransMethods, (PyObject *) o, name);
}

/** \ingroup python
 */
static void rpmtransDealloc(PyObject * o) {
    rpmtransObject * trans = (void *) o;

    rpmtransFree(trans->ts);
    if (trans->dbo) {
	Py_DECREF(trans->dbo);
    }
    if (trans->scriptFd) Fclose(trans->scriptFd);
    /* this will free the keyList, and decrement the ref count of all
       the items on the list as well :-) */
    Py_DECREF(trans->keyList);
    PyMem_DEL(o);
}

/** \ingroup python
 */
static int rpmtransSetAttr(rpmtransObject * o, char * name,
			   PyObject * val) {
    int i;

    if (!strcmp(name, "scriptFd")) {
	if (!PyArg_Parse(val, "i", &i)) return 0;
	if (i < 0) {
	    PyErr_SetString(PyExc_TypeError, "bad file descriptor");
	    return -1;
	} else {
	    o->scriptFd = fdDup(i);
	    rpmtransSetScriptFd(o->ts, o->scriptFd);
	}
    } else {
	PyErr_SetString(PyExc_AttributeError, name);
	return -1;
    }

    return 0;
}

/** \ingroup python
 */
static PyTypeObject rpmtransType = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"rpmtrans",			/* tp_name */
	sizeof(rpmtransObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmtransDealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmtransGetAttr, 	/* tp_getattr */
	(setattrfunc) rpmtransSetAttr,	/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
};

/*@}*/

/** \ingroup python
 * \name Module: rpm
 */
/*@{*/

/**
 */
static PyObject * rpmtransCreate(PyObject * self, PyObject * args) {
    rpmtransObject * o;
    rpmdbObject * db = NULL;
    char * rootPath = "/";

    if (!PyArg_ParseTuple(args, "|sO", &rootPath, &db)) return NULL;
    if (db && db->ob_type != &rpmdbType) {
	PyErr_SetString(PyExc_TypeError, "bad type for database argument");
	return NULL;
    }

    o = (void *) PyObject_NEW(rpmtransObject, &rpmtransType);

    Py_XINCREF(db);
    o->dbo = db;
    o->scriptFd = NULL;
    o->ts = rpmtransCreateSet(db ? db->db : NULL, rootPath);
    o->keyList = PyList_New(0);

    return (void *) o;
}

/**
 */
static PyObject * doAddMacro(PyObject * self, PyObject * args) {
    char * name, * val;

    if (!PyArg_ParseTuple(args, "ss", &name, &val))
	return NULL;

    addMacro(NULL, name, NULL, val, RMIL_DEFAULT);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject * doDelMacro(PyObject * self, PyObject * args) {
    char * name;

    if (!PyArg_ParseTuple(args, "s", &name))
	return NULL;

    delMacro(NULL, name);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject * archScore(PyObject * self, PyObject * args) {
    char * arch;
    int score;

    if (!PyArg_ParseTuple(args, "s", &arch))
	return NULL;

    score = rpmMachineScore(RPM_MACHTABLE_INSTARCH, arch);

    return Py_BuildValue("i", score);
}

/**
 */
static int psGetArchScore(Header h) {
    void * pkgArch;
    int type, count;

    if (!headerGetEntry(h, RPMTAG_ARCH, &type, (void **) &pkgArch, &count) ||
        type == RPM_INT8_TYPE)
       return 150;
    else
        return rpmMachineScore(RPM_MACHTABLE_INSTARCH, pkgArch);
}

/**
 */
static int pkgCompareVer(void * first, void * second) {
    struct packageInfo ** a = first;
    struct packageInfo ** b = second;
    int ret, score1, score2;

    /* put packages w/o names at the end */
    if (!(*a)->name) return 1;
    if (!(*b)->name) return -1;

    ret = xstrcasecmp((*a)->name, (*b)->name);
    if (ret) return ret;
    score1 = psGetArchScore((*a)->h);
    if (!score1) return 1;
    score2 = psGetArchScore((*b)->h);
    if (!score2) return -1;
    if (score1 < score2) return -1;
    if (score1 > score2) return 1;
    return rpmVersionCompare((*b)->h, (*a)->h);
}

/**
 */
static void pkgSort(struct pkgSet * psp) {
    int i;
    char *name;

    if (psp->numPackages <= 0)
	return;

    qsort(psp->packages, psp->numPackages, sizeof(*psp->packages),
	 (void *) pkgCompareVer);

    name = psp->packages[0]->name;
    if (!name) {
       psp->numPackages = 0;
       return;
    }
    for (i = 1; i < psp->numPackages; i++) {
       if (!psp->packages[i]->name) break;
       if (!strcmp(psp->packages[i]->name, name))
	   psp->packages[i]->name = NULL;
       else
	   name = psp->packages[i]->name;
    }

    qsort(psp->packages, psp->numPackages, sizeof(*psp->packages),
	 (void *) pkgCompareVer);

    for (i = 0; i < psp->numPackages; i++)
       if (!psp->packages[i]->name) break;
    psp->numPackages = i;
}

/**
 */
static PyObject * findUpgradeSet(PyObject * self, PyObject * args) {
    PyObject * hdrList, * result;
    char * root = "/";
    int i;
    struct pkgSet list;
    hdrObject * hdr;

    if (!PyArg_ParseTuple(args, "O|s", &hdrList, &root)) return NULL;

    if (!PyList_Check(hdrList)) {
	PyErr_SetString(PyExc_TypeError, "list of headers expected");
	return NULL;
    }

    list.numPackages = PyList_Size(hdrList);
    list.packages = alloca(sizeof(list.packages) * list.numPackages);
    for (i = 0; i < list.numPackages; i++) {
	hdr = (hdrObject *) PyList_GetItem(hdrList, i);
	if (hdr->ob_type != &hdrType) {
	    PyErr_SetString(PyExc_TypeError, "list of headers expected");
	    return NULL;
	}
	list.packages[i] = alloca(sizeof(struct packageInfo));
	list.packages[i]->h = hdr->h;
	list.packages[i]->selected = 0;
	list.packages[i]->data = hdr;

	headerGetEntry(hdr->h, RPMTAG_NAME, NULL,
		      (void **) &list.packages[i]->name, NULL);
    }

    pkgSort (&list);

    if (ugFindUpgradePackages(&list, root)) {
	PyErr_SetString(pyrpmError, "error during upgrade check");
	return NULL;
    }

    result = PyList_New(0);
    for (i = 0; i < list.numPackages; i++) {
	if (list.packages[i]->selected) {
	    PyList_Append(result, list.packages[i]->data);
/*  	    Py_DECREF(list.packages[i]->data); */
	}
    }

    return result;
}

/**
 */
static PyObject * rpmHeaderFromPackage(PyObject * self, PyObject * args) {
    hdrObject * h;
    Header header;
    Header sigs;
    FD_t fd;
    int rawFd;
    int isSource = 0;
    rpmRC rc;

    if (!PyArg_ParseTuple(args, "i", &rawFd)) return NULL;
    fd = fdDup(rawFd);

    rc = rpmReadPackageInfo(fd, &sigs, &header);
    Fclose(fd);

    switch (rc) {
    case RPMRC_BADSIZE:
    case RPMRC_OK:
	h = (hdrObject *) PyObject_NEW(PyObject, &hdrType);
	h->h = header;
	h->sigs = sigs;
	h->fileList = h->linkList = h->md5list = NULL;
	h->uids = h->gids = h->mtimes = h->fileSizes = NULL;
	h->modes = h->rdevs = NULL;
	if (headerIsEntry(header, RPMTAG_SOURCEPACKAGE))
	    isSource = 1;
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
static PyObject * hdrLoad(PyObject * self, PyObject * args) {
    char * obj, * copy=NULL;
    Header hdr;
    hdrObject * h;
    int len;

    if (!PyArg_ParseTuple(args, "s#", &obj, &len)) return NULL;
    
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
    h->sigs = NULL;
    h->fileList = h->linkList = h->md5list = NULL;
    h->uids = h->gids = h->mtimes = h->fileSizes = NULL;
    h->modes = h->rdevs = NULL;

    return (PyObject *) h;
}

/**
 */
static PyObject * rhnLoad(PyObject * self, PyObject * args) {
    char * obj, * copy=NULL;
    Header hdr;
    hdrObject * h;
    int len;

    if (!PyArg_ParseTuple(args, "s#", &obj, &len)) return NULL;
    
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

    if (!headerIsEntry(hdr, RPMTAG_HEADERIMMUTABLE)) {
	PyErr_SetString(pyrpmError, "bad header, not immutable");
	headerFree(hdr);
	return NULL;
    }

    if (!headerIsEntry(hdr, RPMTAG_SHA1HEADER)) {
	PyErr_SetString(pyrpmError, "bad header, no digest");
	headerFree(hdr);
	return NULL;
    }

    if (rpmVerifyDigest(hdr)) {
	PyErr_SetString(pyrpmError, "bad header, digest check failed");
	headerFree(hdr);
	return NULL;
    }

    h = (hdrObject *) PyObject_NEW(PyObject, &hdrType);
    h->h = hdr;
    h->sigs = NULL;
    h->fileList = h->linkList = h->md5list = NULL;
    h->uids = h->gids = h->mtimes = h->fileSizes = NULL;
    h->modes = h->rdevs = NULL;

    return (PyObject *) h;
}

/**
 */
static PyObject * rpmInitDB(PyObject * self, PyObject * args) {
    char *root;
    int forWrite = 0;

    if (!PyArg_ParseTuple(args, "i|s", &forWrite, &root)) return NULL;

    if (rpmdbInit(root, forWrite ? O_RDWR | O_CREAT: O_RDONLY)) {
	char * errmsg = "cannot initialize database in %s";
	char * errstr = NULL;
	int errsize;

	errsize = strlen(errmsg) + strlen(root);
	errstr = alloca(errsize);
	snprintf(errstr, errsize, errmsg, root);
	PyErr_SetString(pyrpmError, errstr);
	return NULL;
    }

    Py_INCREF(Py_None);
    return(Py_None);
}

/**
 */
static rpmdbObject * rpmOpenDB(PyObject * self, PyObject * args) {
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
static PyObject * rebuildDB (PyObject * self, PyObject * args) {
    char * root = "";

    if (!PyArg_ParseTuple(args, "s", &root)) return NULL;

    return Py_BuildValue("i", rpmdbRebuild(root));
}

/**
 */
static PyObject * rpmReadHeaders (FD_t fd) {
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
	h->sigs = NULL;
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
static PyObject * rpmHeaderFromFD(PyObject * self, PyObject * args) {
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
static PyObject * rpmHeaderFromFile(PyObject * self, PyObject * args) {
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
static int rpmMergeHeaders(PyObject * list, FD_t fd, int matchTag) {
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

	if (ho->sigs) headerFree(ho->sigs);
	if (ho->md5list) free(ho->md5list);
	if (ho->fileList) free(ho->fileList);
	if (ho->linkList) free(ho->linkList);

	ho->sigs = NULL;
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

static PyObject * rpmMergeHeadersFromFD(PyObject * self, PyObject * args) {
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
static PyObject * errorCB = NULL, * errorData = NULL;

/**
 */
static void errorcb (void)
{
    PyObject * result, * args = NULL;

    if (errorData)
	args = Py_BuildValue("(O)", errorData);

    result = PyEval_CallObject(errorCB, args);
    Py_XDECREF(args);

    if (result == NULL) {
	PyErr_Print();
	PyErr_Clear();
    }
    Py_DECREF (result);
}

/**
 */
static PyObject * errorSetCallback (PyObject * self, PyObject * args) {
    PyObject *newCB = NULL, *newData = NULL;

    if (!PyArg_ParseTuple(args, "O|O", &newCB, &newData)) return NULL;

    /* if we're getting a void*, set the error callback to this. */
    /* also, we can possibly decref any python callbacks we had  */
    /* and set them to NULL.                                     */
    if (PyCObject_Check (newCB)) {
	rpmErrorSetCallback (PyCObject_AsVoidPtr(newCB));

	Py_XDECREF (errorCB);
	Py_XDECREF (errorData);

	errorCB   = NULL;
	errorData = NULL;
	
	Py_INCREF(Py_None);
	return Py_None;
    }
    
    if (!PyCallable_Check (newCB)) {
	PyErr_SetString(PyExc_TypeError, "parameter must be callable");
	return NULL;
    }

    Py_XDECREF(errorCB);
    Py_XDECREF(errorData);

    errorCB = newCB;
    errorData = newData;
    
    Py_INCREF (errorCB);
    Py_XINCREF (errorData);

    return PyCObject_FromVoidPtr(rpmErrorSetCallback (errorcb), NULL);
}

/**
 */
static PyObject * errorString (PyObject * self, PyObject * args) {
    return PyString_FromString(rpmErrorString ());
}

/**
 */
static PyObject * versionCompare (PyObject * self, PyObject * args) {
    hdrObject * h1, * h2;

    if (!PyArg_ParseTuple(args, "O!O!", &hdrType, &h1, &hdrType, &h2)) return NULL;

    return Py_BuildValue("i", rpmVersionCompare(h1->h, h2->h));
}

/**
 */
static PyObject * labelCompare (PyObject * self, PyObject * args) {
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

/**
 */
static PyObject * checkSig (PyObject * self, PyObject * args) {
    char * filename;
    int flags;
    int rc = 255;

    if (PyArg_ParseTuple(args, "si", &filename, &flags)) {
	const char *av[2];
	av[0] = filename;
	av[1] = NULL;
	rc = rpmCheckSig(flags, av);
    }
    return Py_BuildValue("i", rc);
}

/* hack to get the current header that's in the transaction set */
/**
 */
static PyObject * getTsHeader (PyObject * self, PyObject * args) {
    hdrObject * h;
    
    if (transactionSetHeader) {
	h = (hdrObject *) PyObject_NEW(PyObject, &hdrType);
	h->h = headerLink(transactionSetHeader);
	h->sigs = NULL;
	h->fileList = h->linkList = h->md5list = NULL;
	h->uids = h->gids = h->mtimes = h->fileSizes = NULL;
	h->modes = h->rdevs = NULL;
	return (PyObject *) h;
    }
    Py_INCREF(Py_None);
    return (PyObject *) Py_None;
}

/**
 */
typedef struct FDlist_t FDlist;

/**
 */
struct FDlist_t {
    FILE *f;
    FD_t fd;
    char *note;
    FDlist *next;
} ;

/**
 */
static FDlist *fdhead = NULL;

/**
 */
static FDlist *fdtail = NULL;

/**
 */
static int closeCallback(FILE * f) {
    FDlist *node, *last;

    printf ("close callback on %p\n", f);
    
    node = fdhead;
    last = NULL;
    while (node) {
        if (node->f == f)
            break;
        last = node;
        node = node->next;
    }
    if (node) {
        if (last)
            last->next = node->next;
        else
            fdhead = node->next;
        printf ("closing %s %p\n", node->note, node->fd);
	free (node->note);
        node->fd = fdLink(node->fd, "closeCallback");
        Fclose (node->fd);
        while (node->fd)
            node->fd = fdFree(node->fd, "closeCallback");
        free (node);
    }
    return 0; 
}

/**
 */
static PyObject * doFopen(PyObject * self, PyObject * args) {
    char * path, * mode;
    FDlist *node;
    
    if (!PyArg_ParseTuple(args, "ss", &path, &mode))
	return NULL;
    
    node = malloc (sizeof(FDlist));
    
    node->fd = Fopen(path, mode);
    node->fd = fdLink(node->fd, "doFopen");
    node->note = strdup (path);

    if (!node->fd) {
	PyErr_SetFromErrno(pyrpmError);
        free (node);
	return NULL;
    }
    
    if (Ferror(node->fd)) {
	const char *err = Fstrerror(node->fd);
        free(node);
	if (err) {
	    PyErr_SetString(pyrpmError, err);
	    return NULL;
	}
    }
    node->f = fdGetFp(node->fd);
    printf ("opening %s fd = %p f = %p\n", node->note, node->fd, node->f);
    if (!node->f) {
	PyErr_SetString(pyrpmError, "FD_t has no FILE*");
        free(node);
	return NULL;
    }

    node->next = NULL;
    if (!fdhead) {
	fdhead = fdtail = node;
    } else if (fdtail) {
        fdtail->next = node;
    } else {
        fdhead = node;
    }
    fdtail = node;
    
    return PyFile_FromFile (node->f, path, mode, closeCallback);
}

/**
 */
static PyMethodDef rpmModuleMethods[] = {
    { "TransactionSet", (PyCFunction) rpmtransCreate, METH_VARARGS, NULL },
    { "addMacro", (PyCFunction) doAddMacro, METH_VARARGS, NULL },
    { "delMacro", (PyCFunction) doDelMacro, METH_VARARGS, NULL },
    { "archscore", (PyCFunction) archScore, METH_VARARGS, NULL },
    { "findUpgradeSet", (PyCFunction) findUpgradeSet, METH_VARARGS, NULL },
    { "headerFromPackage", (PyCFunction) rpmHeaderFromPackage, METH_VARARGS, NULL },
    { "headerLoad", (PyCFunction) hdrLoad, METH_VARARGS, NULL },
    { "rhnLoad", (PyCFunction) rhnLoad, METH_VARARGS, NULL },
    { "initdb", (PyCFunction) rpmInitDB, METH_VARARGS, NULL },
    { "opendb", (PyCFunction) rpmOpenDB, METH_VARARGS, NULL },
    { "rebuilddb", (PyCFunction) rebuildDB, METH_VARARGS, NULL },
    { "mergeHeaderListFromFD", (PyCFunction) rpmMergeHeadersFromFD, METH_VARARGS, NULL },
    { "readHeaderListFromFD", (PyCFunction) rpmHeaderFromFD, METH_VARARGS, NULL },
    { "readHeaderListFromFile", (PyCFunction) rpmHeaderFromFile, METH_VARARGS, NULL },
    { "errorSetCallback", (PyCFunction) errorSetCallback, METH_VARARGS, NULL },
    { "errorString", (PyCFunction) errorString, METH_VARARGS, NULL },
    { "versionCompare", (PyCFunction) versionCompare, METH_VARARGS, NULL },
    { "labelCompare", (PyCFunction) labelCompare, METH_VARARGS, NULL },
    { "checksig", (PyCFunction) checkSig, METH_VARARGS, NULL },
    { "getTransactionCallbackHeader", (PyCFunction) getTsHeader, METH_VARARGS, NULL },
/*      { "Fopen", (PyCFunction) doFopen, METH_VARARGS, NULL }, */
    { NULL }
} ;

/**
 */
void initrpm(void) {
    PyObject * m, * d, *o, * tag = NULL, * dict;
    int i;
    const struct headerSprintfExtension_s * extensions = rpmHeaderFormats;
    struct headerSprintfExtension_s * ext;

    m = Py_InitModule("rpm", rpmModuleMethods);

    hdrType.ob_type = &PyType_Type;
    rpmdbMIType.ob_type = &PyType_Type;
    rpmdbType.ob_type = &PyType_Type;
    rpmtransType.ob_type = &PyType_Type;

    if(!m)
	return;

/*      _rpmio_debug = -1; */
    rpmReadConfigFiles(NULL, NULL);

    d = PyModule_GetDict(m);

    pyrpmError = PyString_FromString("rpm.error");
    PyDict_SetItemString(d, "error", pyrpmError);
    Py_DECREF(pyrpmError);

    dict = PyDict_New();

    for (i = 0; i < rpmTagTableSize; i++) {
	tag = PyInt_FromLong(rpmTagTable[i].val);
	PyDict_SetItemString(d, (char *) rpmTagTable[i].name, tag);
	Py_DECREF(tag);
        PyDict_SetItem(dict, tag, o=PyString_FromString(rpmTagTable[i].name + 7));
	Py_DECREF(o);
    }

    while (extensions->name) {
	if (extensions->type == HEADER_EXT_TAG) {
            (const struct headerSprintfExtension *) ext = extensions;
            PyDict_SetItemString(d, extensions->name, o=PyCObject_FromVoidPtr(ext, NULL));
	    Py_DECREF(o);
            PyDict_SetItem(dict, tag, o=PyString_FromString(ext->name + 7));
	    Py_DECREF(o);    
        }
        extensions++;
    }

    PyDict_SetItemString(d, "tagnames", dict);
    Py_DECREF(dict);


#define REGISTER_ENUM(val) \
    PyDict_SetItemString(d, #val, o=PyInt_FromLong( val )); \
    Py_DECREF(o);
    
    REGISTER_ENUM(RPMFILE_STATE_NORMAL);
    REGISTER_ENUM(RPMFILE_STATE_REPLACED);
    REGISTER_ENUM(RPMFILE_STATE_NOTINSTALLED);
    REGISTER_ENUM(RPMFILE_STATE_NETSHARED);

    REGISTER_ENUM(RPMFILE_CONFIG);
    REGISTER_ENUM(RPMFILE_DOC);
    REGISTER_ENUM(RPMFILE_MISSINGOK);
    REGISTER_ENUM(RPMFILE_NOREPLACE);
    REGISTER_ENUM(RPMFILE_GHOST);
    REGISTER_ENUM(RPMFILE_LICENSE);
    REGISTER_ENUM(RPMFILE_README);

    REGISTER_ENUM(RPMDEP_SENSE_REQUIRES);
    REGISTER_ENUM(RPMDEP_SENSE_CONFLICTS);

    REGISTER_ENUM(RPMSENSE_SERIAL);
    REGISTER_ENUM(RPMSENSE_LESS);
    REGISTER_ENUM(RPMSENSE_GREATER);
    REGISTER_ENUM(RPMSENSE_EQUAL);
    REGISTER_ENUM(RPMSENSE_PREREQ);
    REGISTER_ENUM(RPMSENSE_INTERP);
    REGISTER_ENUM(RPMSENSE_SCRIPT_PRE);
    REGISTER_ENUM(RPMSENSE_SCRIPT_POST);
    REGISTER_ENUM(RPMSENSE_SCRIPT_PREUN);
    REGISTER_ENUM(RPMSENSE_SCRIPT_POSTUN);
    REGISTER_ENUM(RPMSENSE_SCRIPT_VERIFY);
    REGISTER_ENUM(RPMSENSE_FIND_REQUIRES);
    REGISTER_ENUM(RPMSENSE_FIND_PROVIDES);
    REGISTER_ENUM(RPMSENSE_TRIGGERIN);
    REGISTER_ENUM(RPMSENSE_TRIGGERUN);
    REGISTER_ENUM(RPMSENSE_TRIGGERPOSTUN);
    REGISTER_ENUM(RPMSENSE_MULTILIB);
    REGISTER_ENUM(RPMSENSE_SCRIPT_PREP);
    REGISTER_ENUM(RPMSENSE_SCRIPT_BUILD);
    REGISTER_ENUM(RPMSENSE_SCRIPT_INSTALL);
    REGISTER_ENUM(RPMSENSE_SCRIPT_CLEAN);
    REGISTER_ENUM(RPMSENSE_RPMLIB);
    REGISTER_ENUM(RPMSENSE_TRIGGERPREIN);

    REGISTER_ENUM(RPMTRANS_FLAG_TEST);
    REGISTER_ENUM(RPMTRANS_FLAG_BUILD_PROBS);
    REGISTER_ENUM(RPMTRANS_FLAG_NOSCRIPTS);
    REGISTER_ENUM(RPMTRANS_FLAG_JUSTDB);
    REGISTER_ENUM(RPMTRANS_FLAG_NOTRIGGERS);
    REGISTER_ENUM(RPMTRANS_FLAG_NODOCS);
    REGISTER_ENUM(RPMTRANS_FLAG_ALLFILES);
    REGISTER_ENUM(RPMTRANS_FLAG_KEEPOBSOLETE);
    REGISTER_ENUM(RPMTRANS_FLAG_MULTILIB);

    REGISTER_ENUM(RPMPROB_FILTER_IGNOREOS);
    REGISTER_ENUM(RPMPROB_FILTER_IGNOREARCH);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEPKG);
    REGISTER_ENUM(RPMPROB_FILTER_FORCERELOCATE);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACENEWFILES);
    REGISTER_ENUM(RPMPROB_FILTER_REPLACEOLDFILES);
    REGISTER_ENUM(RPMPROB_FILTER_OLDPACKAGE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKSPACE);
    REGISTER_ENUM(RPMPROB_FILTER_DISKNODES);

    REGISTER_ENUM(RPMCALLBACK_INST_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_INST_START);
    REGISTER_ENUM(RPMCALLBACK_INST_OPEN_FILE);
    REGISTER_ENUM(RPMCALLBACK_INST_CLOSE_FILE);
    REGISTER_ENUM(RPMCALLBACK_TRANS_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_TRANS_START);
    REGISTER_ENUM(RPMCALLBACK_TRANS_STOP);
    REGISTER_ENUM(RPMCALLBACK_UNINST_PROGRESS);
    REGISTER_ENUM(RPMCALLBACK_UNINST_START);
    REGISTER_ENUM(RPMCALLBACK_UNINST_STOP);

    REGISTER_ENUM(RPMPROB_BADARCH);
    REGISTER_ENUM(RPMPROB_BADOS);
    REGISTER_ENUM(RPMPROB_PKG_INSTALLED);
    REGISTER_ENUM(RPMPROB_BADRELOCATE);
    REGISTER_ENUM(RPMPROB_REQUIRES);
    REGISTER_ENUM(RPMPROB_CONFLICT);
    REGISTER_ENUM(RPMPROB_NEW_FILE_CONFLICT);
    REGISTER_ENUM(RPMPROB_FILE_CONFLICT);
    REGISTER_ENUM(RPMPROB_OLDPACKAGE);
    REGISTER_ENUM(RPMPROB_DISKSPACE);
    REGISTER_ENUM(RPMPROB_DISKNODES);
    REGISTER_ENUM(RPMPROB_BADPRETRANS);

    REGISTER_ENUM(CHECKSIG_PGP);
    REGISTER_ENUM(CHECKSIG_GPG);
    REGISTER_ENUM(CHECKSIG_MD5);
}

/*@}*/
