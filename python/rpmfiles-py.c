#include "rpmsystem-py.h"

#include <rpm/rpmtag.h>
#include <rpm/rpmstring.h>

#include "header-py.h"
#include "rpmfiles-py.h"
#include "rpmfd-py.h"
#include "rpmarchive-py.h"
#include "rpmstrpool-py.h"

/* A single file from rpmfiles set, can't be independently instanciated */
struct rpmfileObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    rpmfiles files;		/* reference to rpmfiles */
    int ix;			/* index to rpmfiles */
};

static void rpmfile_dealloc(rpmfileObject * s)
{
    s->files = rpmfilesFree(s->files);
    freefunc free = PyType_GetSlot(Py_TYPE(s), Py_tp_free);
    free(s);
}

static char rpmfile_doc[] =
    "Gives access to the meta data of a single file.\n\n"
    "Instances of this class are only available through an rpm.files object.";

static PyObject *rpmfile_fx(rpmfileObject *s)
{
    return Py_BuildValue("i", s->ix);
}

static PyObject *rpmfile_dx(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesDI(s->files, s->ix));
}

static PyObject *rpmfile_name(rpmfileObject *s)
{
    char * fn = rpmfilesFN(s->files, s->ix);
    PyObject *o = utf8FromString(fn);
    free(fn);
    return o;
}

static PyObject *rpmfile_basename(rpmfileObject *s)
{
    return utf8FromString(rpmfilesBN(s->files, s->ix));
}

static PyObject *rpmfile_dirname(rpmfileObject *s)
{
    return utf8FromString(rpmfilesDN(s->files, rpmfilesDI(s->files, s->ix)));
}

static PyObject *rpmfile_orig_name(rpmfileObject *s)
{
    char * fn = rpmfilesOFN(s->files, s->ix);
    PyObject *o = utf8FromString(fn);
    free(fn);
    return o;
}

static PyObject *rpmfile_orig_basename(rpmfileObject *s)
{
    return utf8FromString(rpmfilesOBN(s->files, s->ix));
}

static PyObject *rpmfile_orig_dirname(rpmfileObject *s)
{
    return utf8FromString(rpmfilesODN(s->files, rpmfilesODI(s->files, s->ix)));
}
static PyObject *rpmfile_mode(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesFMode(s->files, s->ix));
}

static PyObject *rpmfile_size(rpmfileObject *s)
{
    return Py_BuildValue("L", rpmfilesFSize(s->files, s->ix));
}

static PyObject *rpmfile_mtime(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesFMtime(s->files, s->ix));
}

static PyObject *rpmfile_rdev(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesFRdev(s->files, s->ix));
}

static PyObject *rpmfile_inode(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesFInode(s->files, s->ix));
}

static PyObject *rpmfile_nlink(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesFNlink(s->files, s->ix));
}

static PyObject *rpmfile_linkto(rpmfileObject *s)
{
    return utf8FromString(rpmfilesFLink(s->files, s->ix));
}

static PyObject *rpmfile_user(rpmfileObject *s)
{
    return utf8FromString(rpmfilesFUser(s->files, s->ix));
}

static PyObject *rpmfile_group(rpmfileObject *s)
{
    return utf8FromString(rpmfilesFGroup(s->files, s->ix));
}

static PyObject *rpmfile_fflags(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesFFlags(s->files, s->ix));
}

static PyObject *rpmfile_vflags(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesVFlags(s->files, s->ix));
}

static PyObject *rpmfile_color(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesFColor(s->files, s->ix));
}

static PyObject *rpmfile_state(rpmfileObject *s)
{
    return Py_BuildValue("i", rpmfilesFState(s->files, s->ix));
}

static PyObject *rpmfile_digest(rpmfileObject *s)
{
    size_t diglen = 0;
    const unsigned char *digest = rpmfilesFDigest(s->files, s->ix,
						  NULL, &diglen);
    if (digest) {
	char * hex = rpmhex(digest, diglen);
	PyObject *o = utf8FromString(hex);
	free(hex);
	return o;
    }
    Py_RETURN_NONE;
}

static PyObject *bytebuf(const unsigned char *buf, size_t len)
{
    if (buf) {
	PyObject *o = PyBytes_FromStringAndSize((const char *)buf, len);
	return o;
    }
    Py_RETURN_NONE;
}

static PyObject *rpmfile_imasig(rpmfileObject *s)
{
    size_t len = 0;
    const unsigned char *sig = rpmfilesFSignature(s->files, s->ix, &len);
    return bytebuf(sig, len);
}

static PyObject *rpmfile_veritysig(rpmfileObject *s)
{
    size_t len = 0;
    const unsigned char *sig = rpmfilesVSignature(s->files, s->ix, &len, NULL);
    return bytebuf(sig, len);
}

static PyObject *rpmfile_class(rpmfileObject *s)
{
    return utf8FromString(rpmfilesFClass(s->files, s->ix));
}

static PyObject *rpmfile_caps(rpmfileObject *s)
{
    return utf8FromString(rpmfilesFCaps(s->files, s->ix));
}

static PyObject *rpmfile_langs(rpmfileObject *s)
{
    return utf8FromString(rpmfilesFLangs(s->files, s->ix));
}

static PyObject *rpmfile_links(rpmfileObject *s)
{
    PyObject *result = NULL;
    const int * links = NULL;
    uint32_t nlinks = rpmfilesFLinks(s->files, s->ix, &links);
    int res;

    if (nlinks == 0)
	Py_RETURN_NONE;
    else if (nlinks == 1)
	links = &s->ix; /* file itself */

    result = PyTuple_New(nlinks);
    if (result) {
	for (uint32_t i = 0; i < nlinks; i++) {
	    int lix = links[i];
	    PyObject * o;

	    if (lix == s->ix) {
		/* file itself, return a reference instead of new object */
		Py_INCREF(s);
		o = (PyObject *) s;
	    } else {
		o = rpmfile_Wrap(s->files, lix);
	    }

	    res = PyTuple_SetItem(result, i, o);
	    if (res < 0) {
		Py_DECREF(result);
		return NULL;
	    }
	}
    }
    return result;
}

/*
 * Exported as "matches" instead of a rich comparison operator or such
 * as this cannot be used for comparing file *object* equality,
 * rpmfilesCompare() determines whether the file *contents* match.
 */
static PyObject *rpmfile_matches(rpmfileObject *s, PyObject *o)
{
    PyObject *result = NULL;
    if (rpmfileObject_Check(o)) {
	rpmfileObject *of = (rpmfileObject *)o;
	int rc = rpmfilesCompare(s->files, s->ix, of->files, of->ix);
	result = PyBool_FromLong(rc == 0);
    } else {
	PyErr_SetObject(PyExc_TypeError, o);
    }
    return result;
}

static PyObject *rpmfile_verify(rpmfileObject *s, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = { "omitMask", NULL };
    rpmVerifyAttrs omitMask = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, &omitMask))
	return NULL;

    return Py_BuildValue("i", rpmfilesVerify(s->files, s->ix, omitMask));
}

static PyGetSetDef rpmfile_getseters[] = {
    { "fx",		(getter) rpmfile_fx,		NULL,
      "index in header and rpm.files object" },
    { "dx",		(getter) rpmfile_dx,		NULL,
      "index of dirname entry" },
    { "name",		(getter) rpmfile_name,		NULL,
      "file name (path)" },
    { "basename",	(getter) rpmfile_basename,	NULL, NULL },
    { "dirname",	(getter) rpmfile_dirname,	NULL, NULL },
    { "orig_name",	(getter) rpmfile_orig_name,	NULL,
      "original file name (may differ due to relocation)" },
    { "orig_basename",	(getter) rpmfile_orig_basename,	NULL,
      "original base name (may differ due to relocation)" },
    { "orig_dirname",	(getter) rpmfile_orig_dirname,	NULL,
      "original dir name (may differ due to relocation)" },
    { "mode",		(getter) rpmfile_mode,		NULL,
      "mode flags / unix permissions" },
    { "mtime",		(getter) rpmfile_mtime,		NULL,
      "modification time (in unix time)" },
    { "size",		(getter) rpmfile_size,		NULL,
      "file size" },
    { "rdev",		(getter) rpmfile_rdev,		NULL,
      "device number - for device files only" },
    { "inode",		(getter) rpmfile_inode,		NULL,
      "inode number - contains fake, data used to identify hard liked files" },
    { "fflags",		(getter) rpmfile_fflags,	NULL,
      "file flags - see RPMFILE_* constants" },
    { "vflags",		(getter) rpmfile_vflags,	NULL,
      "verification flags - see RPMVERIFY_* (in rpmfiles.h)" },
    { "linkto",		(getter) rpmfile_linkto,	NULL,
      "link target - symlinks only" },
    { "color",		(getter) rpmfile_color,		NULL,
      "file color - 2 for 64 bit binaries, 1 for 32 bit binaries, 0 else" },
    { "nlink",		(getter) rpmfile_nlink,		NULL,
      "number of hardlinks pointing to the same content as this file" },
    { "links",		(getter) rpmfile_links,		NULL,
      "list of file indexes that are hardlinked with this file" },
    { "user",		(getter) rpmfile_user,		NULL,
      "user name owning this file" },
    { "group",		(getter) rpmfile_group,		NULL,
      "group name owning this file" },
    { "digest",		(getter) rpmfile_digest,	NULL,
      "check sum of file content" },
    { "class",		(getter) rpmfile_class,		NULL,
      "classfication of file content based on libmagic/file(1)" },
    { "state",		(getter) rpmfile_state,		NULL,
      "file state  - see RPMFILE_STATE_* constants" },
    { "langs",		(getter) rpmfile_langs,		NULL,
      "language the file provides (typically for doc files)" },
    { "caps",		(getter) rpmfile_caps,		NULL,
      "file capabilities" },
    { "imasig",	(getter) rpmfile_imasig,	NULL,
      "IMA signature" },
    { "veritysig",	(getter) rpmfile_veritysig,	NULL,
      "Verity signature" },
    { NULL, NULL, NULL, NULL }
};

static struct PyMethodDef rpmfile_methods[] = {
    { "matches", (PyCFunction) rpmfile_matches,		METH_O,
	NULL },
    { "verify",	(PyCFunction) rpmfile_verify,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { NULL, NULL, 0, NULL }
};

static PyObject *disabled_new(PyTypeObject *type,
                              PyObject *args, PyObject *kwds)
{
    PyErr_SetString(PyExc_TypeError,
                    "TypeError: cannot create 'rpm.file' instances");
    return NULL;
}

static PyType_Slot rpmfile_Type_Slots[] = {
    {Py_tp_new, disabled_new},
    {Py_tp_dealloc, rpmfile_dealloc},
    {Py_tp_str, rpmfile_name},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, rpmfile_doc},
    {Py_tp_methods, rpmfile_methods},
    {Py_tp_getset, rpmfile_getseters},
    {0, NULL},
};

PyTypeObject* rpmfile_Type;
PyType_Spec rpmfile_Type_Spec = {
    .name = "rpm.file",
    .basicsize = sizeof(rpmfileObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmfile_Type_Slots,
};

PyObject * rpmfile_Wrap(rpmfiles files, int ix)
{
    rpmfileObject *s = PyObject_New(rpmfileObject, rpmfile_Type);
    if (s == NULL) return NULL;

    s->files = rpmfilesLink(files);
    s->ix = ix;
    return (PyObject *) s;
}

/* The actual rpmfiles info set */
struct rpmfilesObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmfiles files;
};

static void rpmfiles_dealloc(rpmfilesObject * s)
{
    s->files = rpmfilesFree(s->files);
    freefunc free = PyType_GetSlot(Py_TYPE(s), Py_tp_free);
    free(s);
}

static PyObject * rpmfiles_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
{
    PyObject * to = NULL;
    Header h = NULL;
    rpmfiles files = NULL;
    rpmTagVal tagN = RPMTAG_BASENAMES;
    int flags = 0;
    rpmstrPool pool = NULL;
    char * kwlist[] = {"header", "tag", "flags", "pool", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|OiO&:rpmfiles_new", kwlist,
				hdrFromPyObject, &h, &to, &flags,
				poolFromPyObject, &pool))
	return NULL;

    files = rpmfilesNew(pool, h, tagN, flags);

    if (files == NULL) {
	PyErr_SetString(PyExc_ValueError, "invalid file data in header");
	return NULL;
    }

    return rpmfiles_Wrap(subtype, files);
}

static Py_ssize_t rpmfiles_length(rpmfilesObject *s)
{
    return rpmfilesFC(s->files);
}

static PyObject * rpmfiles_getitem(rpmfilesObject *s, Py_ssize_t ix)
{
    if (ix >= 0 && ix < rpmfilesFC(s->files))
	return rpmfile_Wrap(s->files, ix);

    PyErr_SetObject(PyExc_IndexError, Py_BuildValue("i", ix));
    return NULL;
}

static int rpmfiles_contains(rpmfilesObject *s, PyObject *value)
{
    const char *fn = NULL;

    if (!PyArg_Parse(value, "s", &fn))
	return -1;

    return (rpmfilesFindFN(s->files, fn) >= 0) ? 1 : 0;
}


static PyObject * rpmfiles_find(rpmfileObject *s,
				PyObject *args, PyObject *kwds)
{
    const char *fn = NULL;
    int fx, orig = 0;
    char * kwlist[] = {"filename", "orig", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &fn, &orig))
	return NULL;

    if (orig)
	fx = rpmfilesFindOFN(s->files, fn);
    else
	fx = rpmfilesFindFN(s->files, fn);

    if (fx >= 0)
	return rpmfile_Wrap(s->files, fx);

    Py_RETURN_NONE;
}

static PyObject *rpmfiles_archive(rpmfilesObject *s,
				  PyObject *args, PyObject *kwds)
{
    char * kwlist[] = {"fd", "write", NULL};
    rpmfdObject *fdo = NULL;
    FD_t fd = NULL;
    rpmfi archive = NULL;
    int writer = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|i", kwlist,
				     rpmfdFromPyObject, &fdo, &writer)) {
	return NULL;
    }

    fd = rpmfdGetFd(fdo);
    if (writer) {
	archive = rpmfiNewArchiveWriter(fd, s->files);
    } else {
	archive = rpmfiNewArchiveReader(fd, s->files, RPMFI_ITER_READ_ARCHIVE);
    }

    return rpmarchive_Wrap(rpmarchive_Type, s->files, archive);
}

static PyObject *rpmfiles_subscript(rpmfilesObject *s, PyObject *item)
{
    PyObject *str = NULL;
    int res;

    /* treat numbers as sequence accesses */
    if (PyLong_Check(item)) {
	return rpmfiles_getitem(s, PyLong_AsSsize_t(item));
    }

    /* handle slices by returning tuples of rpm.file items */
    if (PySlice_Check(item)) {
	Py_ssize_t start, stop, step, slicelength, i, cur;
	PyObject * result;
	
	if (PySlice_GetIndicesEx(item, rpmfiles_length(s),
				 &start, &stop, &step, &slicelength) < 0) {
	    return NULL;
	}

	result = PyTuple_New(slicelength);
	if (result) {
	    for (cur = start, i = 0; i < slicelength; cur += step, i++) {
		PyObject *item = rpmfiles_getitem(s, cur);
		if (!item) {
		    Py_DECREF(result);
		    return NULL;
		}
		res = PyTuple_SetItem(result, i, item);
		if (res < 0) {
		    Py_DECREF(result);
		    return NULL;
		}
	    }
	}
	return result;
    }

    /* ... and strings as mapping access */
    if (utf8FromPyObject(item, &str)) {
	int fx = rpmfilesFindFN(s->files, PyBytes_AsString(str));
	Py_DECREF(str);

	if (fx >= 0) {
	    return rpmfile_Wrap(s->files, fx);
	} else {
	    PyErr_SetObject(PyExc_KeyError, item);
	}
    } else {
	PyErr_SetObject(PyExc_TypeError, item);
    }

    return NULL;
}


static struct PyMethodDef rpmfiles_methods[] = {
    { "archive", (PyCFunction) rpmfiles_archive, METH_VARARGS|METH_KEYWORDS,
      "files.archive(fd, write=False) -- Return a rpm.archive object\n\n"
      "Args:\n"
      "  fd : File to read from or write to.\n"
      "  write : True to get an archive writer, False for an archive reader"},
    { "find",	(PyCFunction) rpmfiles_find,	METH_VARARGS|METH_KEYWORDS,
      "files.find(filename, orig=False) -- Return index of given file name.\n\n"
      "  Return -1 if file is not found.\n"
      "  Leading \".\" in filename is ignored."},
    { NULL, NULL, 0, NULL }
};

static char rpmfiles_doc[] =
    "rpm.files(hdr, tag=RPMTAG_BASENAMES, flags=None, pool=None)\n\n"
    "Stores the meta data of a package's files.\n\n"
    "Args:\n"
    "\thdr: The header object to get the data from.\n"
    "\tflags : Controls which data to store and whether to create\n\t\tcopies or use the data from the header.\n\t\tBy default all data is copied.\n\t\tSee RPMFI_* constants in rpmfiles.h.\n"
    "\tpool : rpm.strpool object to store the strings in.\n\t\tLeave empty to use global pool.\n"
    "\ttag : Obsolete. Leave alone!\n\n"
    "rpm.files is basically a sequence of rpm.file objects.\nNote that this is a read only data structure. To write file data you\nhave to write it directly into aheader object.";

static PyType_Slot rpmfiles_Type_Slots[] = {
    {Py_tp_dealloc, rpmfiles_dealloc},
    {Py_sq_length, rpmfiles_length},
    {Py_sq_item, rpmfiles_getitem},
    {Py_sq_contains, rpmfiles_contains},
    {Py_mp_length, rpmfiles_length},
    {Py_mp_subscript, rpmfiles_subscript},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, rpmfiles_doc},
    {Py_tp_methods, rpmfiles_methods},
    {Py_tp_new, rpmfiles_new},
    {0, NULL},
};

PyTypeObject* rpmfiles_Type;
PyType_Spec rpmfiles_Type_Spec = {
    .name = "rpm.files",
    .basicsize = sizeof(rpmfilesObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmfiles_Type_Slots,
};

PyObject * rpmfiles_Wrap(PyTypeObject *subtype, rpmfiles files)
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    rpmfilesObject *s = (rpmfilesObject *)subtype_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->files = files;
    return (PyObject *) s;
}

