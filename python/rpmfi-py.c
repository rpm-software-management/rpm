#include "rpmsystem-py.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmpgp.h>

#include "header-py.h"
#include "rpmfi-py.h"

#include "debug.h"

struct rpmfiObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int active;
    rpmfi fi;
};

static PyObject *
rpmfi_FC(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiFC(s->fi));
}

static PyObject *
rpmfi_FX(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiFX(s->fi));
}

static PyObject *
rpmfi_DC(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiDC(s->fi));
}

static PyObject *
rpmfi_DX(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiDX(s->fi));
}

static PyObject *
rpmfi_BN(rpmfiObject * s)
{
    return Py_BuildValue("s", rpmfiBN(s->fi));
}

static PyObject *
rpmfi_DN(rpmfiObject * s)
{
    return Py_BuildValue("s", rpmfiDN(s->fi));
}

static PyObject *
rpmfi_FN(rpmfiObject * s)
{
    return Py_BuildValue("s", rpmfiFN(s->fi));
}

static PyObject *
rpmfi_FFlags(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiFFlags(s->fi));
}

static PyObject *
rpmfi_VFlags(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiVFlags(s->fi));
}

static PyObject *
rpmfi_FMode(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiFMode(s->fi));
}

static PyObject *
rpmfi_FState(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiFState(s->fi));
}

/* XXX rpmfiFDigest */
static PyObject *
rpmfi_Digest(rpmfiObject * s)
{
    char *digest = rpmfiFDigestHex(s->fi, NULL);
    if (digest) {
	PyObject *dig = Py_BuildValue("s", digest);
	free(digest);
	return dig;
    } else {
	Py_RETURN_NONE;
    }
}

static PyObject *
rpmfi_FLink(rpmfiObject * s)
{
    return Py_BuildValue("s", rpmfiFLink(s->fi));
}

static PyObject *
rpmfi_FSize(rpmfiObject * s)
{
    return Py_BuildValue("L", rpmfiFSize(s->fi));
}

static PyObject *
rpmfi_FRdev(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiFRdev(s->fi));
}

static PyObject *
rpmfi_FMtime(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiFMtime(s->fi));
}

static PyObject *
rpmfi_FUser(rpmfiObject * s)
{
    return Py_BuildValue("s", rpmfiFUser(s->fi));
}

static PyObject *
rpmfi_FGroup(rpmfiObject * s)
{
    return Py_BuildValue("s", rpmfiFGroup(s->fi));
}

static PyObject *
rpmfi_FColor(rpmfiObject * s)
{
    return Py_BuildValue("i", rpmfiFColor(s->fi));
}

static PyObject *
rpmfi_FClass(rpmfiObject * s)
{
    const char * FClass;

    if ((FClass = rpmfiFClass(s->fi)) == NULL)
	FClass = "";
    return Py_BuildValue("s", FClass);
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
	    PyTuple_SET_ITEM(result,  0, Py_BuildValue("s", FN));
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
	    PyTuple_SET_ITEM(result, 10, Py_BuildValue("s", FUser));
	if (FGroup == NULL) {
	    Py_INCREF(Py_None);
	    PyTuple_SET_ITEM(result, 11, Py_None);
	} else
	    PyTuple_SET_ITEM(result, 11, Py_BuildValue("s", FGroup));
	PyTuple_SET_ITEM(result, 12, rpmfi_Digest(s));

    } else
	s->active = 0;

    return result;
}

static struct PyMethodDef rpmfi_methods[] = {
 {"FC",		(PyCFunction)rpmfi_FC,		METH_NOARGS,
	NULL},
 {"FX",		(PyCFunction)rpmfi_FX,		METH_NOARGS,
	NULL},
 {"DC",		(PyCFunction)rpmfi_DC,		METH_NOARGS,
	NULL},
 {"DX",		(PyCFunction)rpmfi_DX,		METH_NOARGS,
	NULL},
 {"BN",		(PyCFunction)rpmfi_BN,		METH_NOARGS,
	NULL},
 {"DN",		(PyCFunction)rpmfi_DN,		METH_NOARGS,
	NULL},
 {"FN",		(PyCFunction)rpmfi_FN,		METH_NOARGS,
	NULL},
 {"FFlags",	(PyCFunction)rpmfi_FFlags,	METH_NOARGS,
	NULL},
 {"VFlags",	(PyCFunction)rpmfi_VFlags,	METH_NOARGS,
	NULL},
 {"FMode",	(PyCFunction)rpmfi_FMode,	METH_NOARGS,
	NULL},
 {"FState",	(PyCFunction)rpmfi_FState,	METH_NOARGS,
	NULL},
 {"MD5",	(PyCFunction)rpmfi_Digest,	METH_NOARGS,
	NULL},
 {"Digest",	(PyCFunction)rpmfi_Digest,	METH_NOARGS,
	NULL},
 {"FLink",	(PyCFunction)rpmfi_FLink,	METH_NOARGS,
	NULL},
 {"FSize",	(PyCFunction)rpmfi_FSize,	METH_NOARGS,
	NULL},
 {"FRdev",	(PyCFunction)rpmfi_FRdev,	METH_NOARGS,
	NULL},
 {"FMtime",	(PyCFunction)rpmfi_FMtime,	METH_NOARGS,
	NULL},
 {"FUser",	(PyCFunction)rpmfi_FUser,	METH_NOARGS,
	NULL},
 {"FGroup",	(PyCFunction)rpmfi_FGroup,	METH_NOARGS,
	NULL},
 {"FColor",	(PyCFunction)rpmfi_FColor,	METH_NOARGS,
	NULL},
 {"FClass",	(PyCFunction)rpmfi_FClass,	METH_NOARGS,
	NULL},
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
    return Py_BuildValue("s", rpmfiFN(s->fi));
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
    char * kwlist[] = {"header", "tag", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&|Oi:rpmfi_init", kwlist,
				hdrFromPyObject, &h, &to, &flags))
	return NULL;

    fi = rpmfiNew(NULL, h, tagN, flags);

    return rpmfi_Wrap(subtype, fi);
}

static char rpmfi_doc[] =
"";

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

