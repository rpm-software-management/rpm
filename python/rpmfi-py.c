#include "rpmsystem-py.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmpgp.h>

#include "header-py.h"
#include "rpmfi-py.h"
#include "rpmstrpool-py.h"

struct rpmfiObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int active;
    rpmfi fi;
};

static PyObject *
rpmfi_FC(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiFC(s->fi));
}

static PyObject *
rpmfi_FX(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiFX(s->fi));
}

static PyObject *
rpmfi_DC(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiDC(s->fi));
}

static PyObject *
rpmfi_DX(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiDX(s->fi));
}

static PyObject *
rpmfi_BN(rpmfiObject * s, PyObject * unused)
{
    return utf8FromString(rpmfiBN(s->fi));
}

static PyObject *
rpmfi_DN(rpmfiObject * s, PyObject * unused)
{
    return utf8FromString(rpmfiDN(s->fi));
}

static PyObject *
rpmfi_FN(rpmfiObject * s, PyObject * unused)
{
    return utf8FromString(rpmfiFN(s->fi));
}

static PyObject *
rpmfi_FindFN(rpmfiObject * s, PyObject * args, PyObject * kwds)
{
    char * kwlist[] = {"filename", NULL};
    PyObject * filename = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "S:FindFN", kwlist,
                                     &filename))
        return NULL;
    return Py_BuildValue("i", rpmfiFindFN(s->fi, PyBytes_AsString(filename)));
}


static PyObject *
rpmfi_FFlags(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiFFlags(s->fi));
}

static PyObject *
rpmfi_VFlags(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiVFlags(s->fi));
}

static PyObject *
rpmfi_FMode(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiFMode(s->fi));
}

static PyObject *
rpmfi_FState(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiFState(s->fi));
}

/* XXX rpmfiFDigest */
static PyObject *
rpmfi_Digest(rpmfiObject * s, PyObject * unused)
{
    char *digest = rpmfiFDigestHex(s->fi, NULL);
    if (digest) {
	PyObject *dig = utf8FromString(digest);
	free(digest);
	return dig;
    } else {
	Py_RETURN_NONE;
    }
}

static PyObject *
rpmfi_FLink(rpmfiObject * s, PyObject * unused)
{
    return utf8FromString(rpmfiFLink(s->fi));
}

static PyObject *
rpmfi_FSize(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("L", rpmfiFSize(s->fi));
}

static PyObject *
rpmfi_FRdev(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiFRdev(s->fi));
}

static PyObject *
rpmfi_FMtime(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiFMtime(s->fi));
}

static PyObject *
rpmfi_FUser(rpmfiObject * s, PyObject * unused)
{
    return utf8FromString(rpmfiFUser(s->fi));
}

static PyObject *
rpmfi_FGroup(rpmfiObject * s, PyObject * unused)
{
    return utf8FromString(rpmfiFGroup(s->fi));
}

static PyObject *
rpmfi_FColor(rpmfiObject * s, PyObject * unused)
{
    return Py_BuildValue("i", rpmfiFColor(s->fi));
}

static PyObject *
rpmfi_FClass(rpmfiObject * s, PyObject * unused)
{
    const char * FClass;

    if ((FClass = rpmfiFClass(s->fi)) == NULL)
	FClass = "";
    return utf8FromString(FClass);
}

static PyObject *
rpmfi_FLinks(rpmfiObject * s, PyObject * unused)
{
    uint32_t nlinks;
    const int * files;
    PyObject * result;

    nlinks = rpmfiFLinks(s->fi, &files);
    if (nlinks==1) {
	return Py_BuildValue("(i)", rpmfiFX(s->fi));
    }

    result = PyTuple_New(nlinks);
    for (uint32_t i=0; i<nlinks; i++) {
	PyTuple_SET_ITEM(result,  i, PyInt_FromLong(files[i]));
    }
    return result;
}

static PyObject *
rpmfi_iternext(rpmfiObject * s)
{
    PyObject * result = NULL;

    /* Reset loop indices on 1st entry. */
    if (!s->active) {
	s->fi = rpmfiInit(s->fi, 0);
	s->active = 1;
    }

    /* If more to do, return the file tuple. */
    if (rpmfiNext(s->fi) >= 0) {
	const char * FN = rpmfiFN(s->fi);
	rpm_loff_t FSize = rpmfiFSize(s->fi);
	int FMode = rpmfiFMode(s->fi);
	int FMtime = rpmfiFMtime(s->fi);
	int FFlags = rpmfiFFlags(s->fi);
	int FRdev = rpmfiFRdev(s->fi);
	int FInode = rpmfiFInode(s->fi);
	int FNlink = rpmfiFNlink(s->fi);
	int FState = rpmfiFState(s->fi);
	int VFlags = rpmfiVFlags(s->fi);
	const char * FUser = rpmfiFUser(s->fi);
	const char * FGroup = rpmfiFGroup(s->fi);

	result = PyTuple_New(13);
	if (FN == NULL) {
	    Py_INCREF(Py_None);
	    PyTuple_SET_ITEM(result, 0, Py_None);
	} else
	    PyTuple_SET_ITEM(result,  0, utf8FromString(FN));
	PyTuple_SET_ITEM(result,  1, PyLong_FromLongLong(FSize));
	PyTuple_SET_ITEM(result,  2, PyInt_FromLong(FMode));
	PyTuple_SET_ITEM(result,  3, PyInt_FromLong(FMtime));
	PyTuple_SET_ITEM(result,  4, PyInt_FromLong(FFlags));
	PyTuple_SET_ITEM(result,  5, PyInt_FromLong(FRdev));
	PyTuple_SET_ITEM(result,  6, PyInt_FromLong(FInode));
	PyTuple_SET_ITEM(result,  7, PyInt_FromLong(FNlink));
	PyTuple_SET_ITEM(result,  8, PyInt_FromLong(FState));
	PyTuple_SET_ITEM(result,  9, PyInt_FromLong(VFlags));
	if (FUser == NULL) {
	    Py_INCREF(Py_None);
	    PyTuple_SET_ITEM(result, 10, Py_None);
	} else
	    PyTuple_SET_ITEM(result, 10, utf8FromString(FUser));
	if (FGroup == NULL) {
	    Py_INCREF(Py_None);
	    PyTuple_SET_ITEM(result, 11, Py_None);
	} else
	    PyTuple_SET_ITEM(result, 11, utf8FromString(FGroup));
	PyTuple_SET_ITEM(result, 12, rpmfi_Digest(s, NULL));

    } else
	s->active = 0;

    return result;
}

static struct PyMethodDef rpmfi_methods[] = {
 {"FC",		(PyCFunction)rpmfi_FC,		METH_NOARGS,
  "fi.FC() -- Return number of files entries."},
 {"FX",		(PyCFunction)rpmfi_FX,		METH_NOARGS,
  "fi.FX() -- Return current position of the iterator."},
 {"DC",		(PyCFunction)rpmfi_DC,		METH_NOARGS,
  "fi.DC() --Return number of directory entries."},
 {"DX",		(PyCFunction)rpmfi_DX,		METH_NOARGS,
  "fi.DX() -- Return number of directory entry matching current file."},
 {"BN",		(PyCFunction)rpmfi_BN,		METH_NOARGS,
  "fi.BN() -- Return base name of current file."},
 {"DN",		(PyCFunction)rpmfi_DN,		METH_NOARGS,
  "fi.DN() -- Return directory name of the current file."},
 {"FN",		(PyCFunction)rpmfi_FN,		METH_NOARGS,
  "fi.FN() -- Return the name/path of the current file."},
 {"FindFN",	(PyCFunction)rpmfi_FindFN,	METH_VARARGS|METH_KEYWORDS,
  "fi.FindFN(pathname) -- Return entry number of given pathname.\n\nReturn -1 if file is not found.\nLeading '.' in the given name is stripped before the search."},
 {"FFlags",	(PyCFunction)rpmfi_FFlags,	METH_NOARGS,
  "fi.FFlags() -- Return the flags of the current file."},
 {"VFlags",	(PyCFunction)rpmfi_VFlags,	METH_NOARGS,
  "fi.VFlags() -- Return the verify flags of the current file.\n\nSee RPMVERIFY_* (in rpmfiles.h)"},
 {"FMode",	(PyCFunction)rpmfi_FMode,	METH_NOARGS,
  "fi.FMode() -- Return the mode flags of the current file."},
 {"FState",	(PyCFunction)rpmfi_FState,	METH_NOARGS,
  "fi.FState() -- Return the file state of the current file."},
 {"MD5",	(PyCFunction)rpmfi_Digest,	METH_NOARGS,
  "fi.() -- Return the checksum of the current file.\n\nDEPRECATED! Use fi.Digest instead!"},
 {"Digest",	(PyCFunction)rpmfi_Digest,	METH_NOARGS,
  "fi.() -- Return the checksum of the current file."},
 {"FLink",	(PyCFunction)rpmfi_FLink,	METH_NOARGS,
  "fi.() -- Return the link target of the current file.\n\nFor soft links only."},
 {"FSize",	(PyCFunction)rpmfi_FSize,	METH_NOARGS,
  "fi.() -- Return the size of the current file."},
 {"FRdev",	(PyCFunction)rpmfi_FRdev,	METH_NOARGS,
  "fi.() -- Return the device number of the current file.\n\nFor device files only."},
 {"FMtime",	(PyCFunction)rpmfi_FMtime,	METH_NOARGS,
  "fi.() -- Return the modification time of the current file."},
 {"FUser",	(PyCFunction)rpmfi_FUser,	METH_NOARGS,
  "fi.() -- Return the user name owning the current file."},
 {"FGroup",	(PyCFunction)rpmfi_FGroup,	METH_NOARGS,
  "fi.() -- Return the group name of the current file."},
 {"FColor",	(PyCFunction)rpmfi_FColor,	METH_NOARGS,
  "fi.() -- Return the color of the current file.\n\n2 for 64 bit binaries\n1 for 32 bit binaries\n0 for everything else"},
 {"FClass",	(PyCFunction)rpmfi_FClass,	METH_NOARGS,
  "fi.() -- Return the classification of the current file."},
 {"FLinks",	(PyCFunction)rpmfi_FLinks,	METH_NOARGS,
  "fi.() -- Return the number of hardlinks pointing to of the\ncurrent file."},
 {NULL,		NULL}		/* sentinel */
};

/* ---------- */

static void
rpmfi_dealloc(rpmfiObject * s)
{
    s->fi = rpmfiFree(s->fi);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static int
rpmfi_length(rpmfiObject * s)
{
    return rpmfiFC(s->fi);
}

static PyObject *
rpmfi_subscript(rpmfiObject * s, PyObject * key)
{
    int ix;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    ix = (int) PyInt_AsLong(key);
    rpmfiSetFX(s->fi, ix);
    return utf8FromString(rpmfiFN(s->fi));
}

static PyMappingMethods rpmfi_as_mapping = {
        (lenfunc) rpmfi_length,		/* mp_length */
        (binaryfunc) rpmfi_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

static int rpmfi_init(rpmfiObject * s, PyObject *args, PyObject *kwds)
{
    s->active = 0;
    return 0;
}

static PyObject * rpmfi_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
{
    PyObject * to = NULL;
    Header h = NULL;
    rpmfi fi = NULL;
    rpmTagVal tagN = RPMTAG_BASENAMES;
    int flags = 0;
    rpmstrPool pool = NULL;
    char * kwlist[] = {"header", "tag", "flags", "pool", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|OiO&:rpmfi_init", kwlist,
				hdrFromPyObject, &h, &to, &flags,
				poolFromPyObject, &pool))
	return NULL;

    fi = rpmfiNewPool(pool, h, tagN, flags);

    if (fi == NULL) {
	PyErr_SetString(PyExc_ValueError, "invalid file data in header");
	return NULL;
    }

    return rpmfi_Wrap(subtype, fi);
}

static char rpmfi_doc[] =
"File iterator\n\n"
"DEPRECATED! This old API mixes storing and iterating over the meta data\n"
"of the files of a package. Use rpm.files and rpm.file data types as a\n"
"much cleaner API.\n\n"
"Iteration returns a tuple of\n(FN, FSize, FMode, FMtime, FFlags, FRdev, FInode, FNlink, FState,\n VFlags, FUser, FGroup, Digest)";

PyTypeObject rpmfi_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.fi",			/* tp_name */
	sizeof(rpmfiObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmfi_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	0,				/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmfi_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmfi_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc) rpmfi_iternext,	/* tp_iternext */
	rpmfi_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmfi_init,		/* tp_init */
	0,				/* tp_alloc */
	(newfunc) rpmfi_new,		/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

/* ---------- */

rpmfi fiFromFi(rpmfiObject * s)
{
    return s->fi;
}

PyObject * rpmfi_Wrap(PyTypeObject *subtype, rpmfi fi)
{
    rpmfiObject *s = (rpmfiObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->fi = fi;
    s->active = 0;
    return (PyObject *) s;
}

