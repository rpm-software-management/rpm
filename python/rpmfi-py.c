/** \ingroup python
 * \file python/rpmfi-py.c
 */

#include "system.h"

#include "Python.h"

#include <rpmlib.h>

#include "header-py.h"
#include "rpmfi-py.h"

#include "debug.h"

static PyObject *
rpmfi_Debug(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "i", &_rpmfi_debug)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmfi_FC(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FC")) return NULL;
    return Py_BuildValue("i", rpmfiFC(s->fi));
}

static PyObject *
rpmfi_FX(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FX")) return NULL;
    return Py_BuildValue("i", rpmfiFX(s->fi));
}

static PyObject *
rpmfi_DC(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":DC")) return NULL;
    return Py_BuildValue("i", rpmfiDC(s->fi));
}

static PyObject *
rpmfi_DX(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":DX")) return NULL;
    return Py_BuildValue("i", rpmfiDX(s->fi));
}

static PyObject *
rpmfi_BN(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":BN")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiBN(s->fi)));
}

static PyObject *
rpmfi_DN(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":DN")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiDN(s->fi)));
}

static PyObject *
rpmfi_FN(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FN")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiFN(s->fi)));
}

static PyObject *
rpmfi_FFlags(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FFlags")) return NULL;
    return Py_BuildValue("i", rpmfiFFlags(s->fi));
}

static PyObject *
rpmfi_VFlags(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":VFlags")) return NULL;
    return Py_BuildValue("i", rpmfiVFlags(s->fi));
}

static PyObject *
rpmfi_FMode(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FMode")) return NULL;
    return Py_BuildValue("i", rpmfiFMode(s->fi));
}

static PyObject *
rpmfi_FState(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FState")) return NULL;
    return Py_BuildValue("i", rpmfiFState(s->fi));
}

/* XXX rpmfiMD5 */
static PyObject *
rpmfi_MD5(rpmfiObject * s, PyObject * args)
{
    const unsigned char * md5;
    char fmd5[33];
    char * t;
    int i;
    
    if (!PyArg_ParseTuple(args, ":MD5")) return NULL;
    md5 = rpmfiMD5(s->fi);
    for (i = 0, t = fmd5; i < 16; i++, t += 2)
	sprintf(t, "%02x", md5[i]);
    *t = '\0';
    return Py_BuildValue("s", xstrdup(fmd5));
}

static PyObject *
rpmfi_FLink(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FLink")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiFLink(s->fi)));
}

static PyObject *
rpmfi_FSize(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FSize")) return NULL;
    return Py_BuildValue("i", rpmfiFSize(s->fi));
}

static PyObject *
rpmfi_FRdev(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FRdev")) return NULL;
    return Py_BuildValue("i", rpmfiFRdev(s->fi));
}

static PyObject *
rpmfi_FMtime(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FMtime")) return NULL;
    return Py_BuildValue("i", rpmfiFMtime(s->fi));
}

static PyObject *
rpmfi_FUser(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FUser")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiFUser(s->fi)));
}

static PyObject *
rpmfi_FGroup(rpmfiObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":FGroup")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiFGroup(s->fi)));
}

#if Py_TPFLAGS_HAVE_ITER
static PyObject *
rpmfi_Next(rpmfiObject * s, PyObject * args)
{
    PyObject * result = NULL;

    if (rpmfiNext(s->fi) >= 0) {
	const char * FN = rpmfiFN(s->fi);
	int FSize = rpmfiFSize(s->fi);
	int FMode = rpmfiFMode(s->fi);
	int FMtime = rpmfiFMtime(s->fi);
	int FFlags = rpmfiFFlags(s->fi);

	result = PyTuple_New(5);
	PyTuple_SET_ITEM(result, 0, Py_BuildValue("s", FN));
	PyTuple_SET_ITEM(result, 1, PyInt_FromLong(FSize));
	PyTuple_SET_ITEM(result, 2, PyInt_FromLong(FMode));
	PyTuple_SET_ITEM(result, 3, PyInt_FromLong(FMtime));
	PyTuple_SET_ITEM(result, 4, PyInt_FromLong(FFlags));

/* XXX FIXME: more to return */
    }
    return result;
}

static PyObject *
rpmfi_Iter(rpmfiObject * s, PyObject * args)
{
    rpmfiInit(s->fi, 0);
    Py_INCREF(s);
    return (PyObject *)s;
}
#endif

#ifdef	NOTYET
static PyObject *
rpmfi_NextD(rpmfiObject * s, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ":NextD"))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
rpmfi_InitD(rpmfiObject * s, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ":InitD"))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}
#endif

static struct PyMethodDef rpmfi_methods[] = {
 {"Debug",	(PyCFunction)rpmfi_Debug,	METH_VARARGS,
	NULL},
 {"FC",		(PyCFunction)rpmfi_FC,		METH_VARARGS,
	NULL},
 {"FX",		(PyCFunction)rpmfi_FX,		METH_VARARGS,
	NULL},
 {"DC",		(PyCFunction)rpmfi_DC,		METH_VARARGS,
	NULL},
 {"DX",		(PyCFunction)rpmfi_DX,		METH_VARARGS,
	NULL},
 {"BN",		(PyCFunction)rpmfi_BN,		METH_VARARGS,
	NULL},
 {"DN",		(PyCFunction)rpmfi_DN,		METH_VARARGS,
	NULL},
 {"FN",		(PyCFunction)rpmfi_FN,		METH_VARARGS,
	NULL},
 {"FFlags",	(PyCFunction)rpmfi_FFlags,	METH_VARARGS,
	NULL},
 {"VFlags",	(PyCFunction)rpmfi_VFlags,	METH_VARARGS,
	NULL},
 {"FMode",	(PyCFunction)rpmfi_FMode,	METH_VARARGS,
	NULL},
 {"FState",	(PyCFunction)rpmfi_FState,	METH_VARARGS,
	NULL},
 {"MD5",	(PyCFunction)rpmfi_MD5,		METH_VARARGS,
	NULL},
 {"FLink",	(PyCFunction)rpmfi_FLink,	METH_VARARGS,
	NULL},
 {"FSize",	(PyCFunction)rpmfi_FSize,	METH_VARARGS,
	NULL},
 {"FRdev",	(PyCFunction)rpmfi_FRdev,	METH_VARARGS,
	NULL},
 {"FMtime",	(PyCFunction)rpmfi_FMtime,	METH_VARARGS,
	NULL},
 {"FUser",	(PyCFunction)rpmfi_FUser,	METH_VARARGS,
	NULL},
 {"FGroup",	(PyCFunction)rpmfi_FGroup,	METH_VARARGS,
	NULL},
#if Py_TPFLAGS_HAVE_ITER
 {"next",	(PyCFunction)rpmfi_Next,	METH_VARARGS,
	NULL},
 {"iter",	(PyCFunction)rpmfi_Iter,	METH_VARARGS,
	NULL},
#endif
#ifdef	NOTYET
 {"NextD",	(PyCFunction)rpmfi_NextD,	METH_VARARGS,
	NULL},
 {"InitD",	(PyCFunction)rpmfi_InitD,	METH_VARARGS,
	NULL},
#endif
 {NULL,		NULL}		/* sentinel */
};

/* ---------- */

static void
rpmfi_dealloc(rpmfiObject * s)
{
    if (s) {
	s->fi = rpmfiFree(s->fi, 1);
	PyMem_DEL(s);
    }
}

static int
rpmfi_print(rpmfiObject * s, FILE * fp, int flags)
{
    if (!(s && s->fi))
	return -1;

    rpmfiInit(s->fi, 0);
    while (rpmfiNext(s->fi) >= 0)
	fprintf(fp, "%s\n", rpmfiFN(s->fi));
    return 0;
}

static PyObject *
rpmfi_getattr(rpmfiObject * s, char * name)
{
    return Py_FindMethod(rpmfi_methods, (PyObject *)s, name);
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
    return Py_BuildValue("s", xstrdup(rpmfiFN(s->fi)));
}

static PyMappingMethods rpmfi_as_mapping = {
        (inquiry) rpmfi_length,		/* mp_length */
        (binaryfunc) rpmfi_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

/**
 */
static char rpmfi_doc[] =
"";

PyTypeObject rpmfi_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.fi",			/* tp_name */
	sizeof(rpmfiObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor)rpmfi_dealloc,	/* tp_dealloc */
	(printfunc)rpmfi_print,		/* tp_print */
	(getattrfunc)rpmfi_getattr,	/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmfi_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmfi_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc)rpmfi_Iter,	/* tp_iter */
	(iternextfunc)rpmfi_Next,	/* tp_iternext */
	rpmfi_methods,			/* tp_methods */
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

/* ---------- */

rpmfi fiFromFi(rpmfiObject * s)
{
    return s->fi;
}

rpmfiObject *
rpmfi_Wrap(rpmfi fi)
{
    rpmfiObject *s = PyObject_NEW(rpmfiObject, &rpmfi_Type);
    if (s == NULL)
	return NULL;
    s->fi = fi;
    return s;
}

rpmfiObject *
hdr_fiFromHeader(PyObject * s, PyObject * args)
{
    hdrObject * ho;

    if (!PyArg_ParseTuple(args, "O!:fiFromHeader", &hdr_Type, &ho))
	return NULL;
    return rpmfi_Wrap( rpmfiNew(NULL, NULL, hdrGetHeader(ho), RPMTAG_BASENAMES, 0) );
}
