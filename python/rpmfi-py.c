/** \ingroup py_c
 * \file python/rpmfi-py.c
 */

#include "system.h"

#include <rpmlib.h>

#include "header-py.h"
#include "rpmfi-py.h"

#include "debug.h"

/*@access rpmfi @*/

/*@null@*/
static PyObject *
rpmfi_Debug(/*@unused@*/ rpmfiObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i", &_rpmfi_debug)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

/*@null@*/
static PyObject *
rpmfi_FC(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FC")) return NULL;
    return Py_BuildValue("i", rpmfiFC(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_FX(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FX")) return NULL;
    return Py_BuildValue("i", rpmfiFX(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_DC(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":DC")) return NULL;
    return Py_BuildValue("i", rpmfiDC(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_DX(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":DX")) return NULL;
    return Py_BuildValue("i", rpmfiDX(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_BN(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":BN")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiBN(s->fi)));
}

/*@null@*/
static PyObject *
rpmfi_DN(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":DN")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiDN(s->fi)));
}

/*@null@*/
static PyObject *
rpmfi_FN(rpmfiObject * s, PyObject * args)
	/*@modifies s @*/
{
    if (!PyArg_ParseTuple(args, ":FN")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiFN(s->fi)));
}

/*@null@*/
static PyObject *
rpmfi_FFlags(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FFlags")) return NULL;
    return Py_BuildValue("i", rpmfiFFlags(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_VFlags(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":VFlags")) return NULL;
    return Py_BuildValue("i", rpmfiVFlags(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_FMode(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FMode")) return NULL;
    return Py_BuildValue("i", rpmfiFMode(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_FState(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FState")) return NULL;
    return Py_BuildValue("i", rpmfiFState(s->fi));
}

/* XXX rpmfiMD5 */
/*@null@*/
static PyObject *
rpmfi_MD5(rpmfiObject * s, PyObject * args)
	/*@*/
{
    const unsigned char * MD5;
    char fmd5[33];
    char * t;
    int i;

    if (!PyArg_ParseTuple(args, ":MD5")) return NULL;
    MD5 = rpmfiMD5(s->fi);
    t = fmd5;
    if (MD5 != NULL)
    for (i = 0; i < 16; i++, t += 2)
	sprintf(t, "%02x", MD5[i]);
    *t = '\0';
    return Py_BuildValue("s", xstrdup(fmd5));
}

/*@null@*/
static PyObject *
rpmfi_FLink(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FLink")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiFLink(s->fi)));
}

/*@null@*/
static PyObject *
rpmfi_FSize(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FSize")) return NULL;
    return Py_BuildValue("i", rpmfiFSize(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_FRdev(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FRdev")) return NULL;
    return Py_BuildValue("i", rpmfiFRdev(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_FMtime(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FMtime")) return NULL;
    return Py_BuildValue("i", rpmfiFMtime(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_FUser(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FUser")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiFUser(s->fi)));
}

/*@null@*/
static PyObject *
rpmfi_FGroup(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FGroup")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmfiFGroup(s->fi)));
}

/*@null@*/
static PyObject *
rpmfi_FColor(rpmfiObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":FColor")) return NULL;
    return Py_BuildValue("i", rpmfiFColor(s->fi));
}

/*@null@*/
static PyObject *
rpmfi_FClass(rpmfiObject * s, PyObject * args)
	/*@*/
{
    const char * FClass;

    if (!PyArg_ParseTuple(args, ":FClass")) return NULL;
    if ((FClass = rpmfiFClass(s->fi)) == NULL)
	FClass = "";
    return Py_BuildValue("s", xstrdup(FClass));
}

#if Py_TPFLAGS_HAVE_ITER
static PyObject *
rpmfi_iter(rpmfiObject * s, /*@unused@*/ PyObject * args)
	/*@*/
{
    Py_INCREF(s);
    return (PyObject *)s;
}
#endif

/*@null@*/
static PyObject *
rpmfi_iternext(rpmfiObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
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
	int FSize = rpmfiFSize(s->fi);
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
/*@-shadow@*/
	const unsigned char * MD5 = rpmfiMD5(s->fi), *s = MD5;
/*@=shadow@*/
	char FMD5[2*16+1], *t = FMD5;
	static const char hex[] = "0123456789abcdef";
	int gotMD5, i;

	gotMD5 = 0;
	if (s)
	for (i = 0; i < 16; i++) {
	    gotMD5 |= *s;
	    *t++ = hex[ (*s >> 4) & 0xf ];
	    *t++ = hex[ (*s++   ) & 0xf ];
	}
	*t = '\0';

	result = PyTuple_New(13);
	if (FN == NULL) {
	    Py_INCREF(Py_None);
	    PyTuple_SET_ITEM(result, 0, Py_None);
	} else
	    PyTuple_SET_ITEM(result,  0, Py_BuildValue("s", FN));
	PyTuple_SET_ITEM(result,  1, PyInt_FromLong(FSize));
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
	if (!gotMD5) {
	    Py_INCREF(Py_None);
	    PyTuple_SET_ITEM(result, 12, Py_None);
	} else
	    PyTuple_SET_ITEM(result, 12, Py_BuildValue("s", FMD5));

    } else
	s->active = 0;

    return result;
}

static PyObject *
rpmfi_Next(rpmfiObject * s, /*@unused@*/ PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result = NULL;

    result = rpmfi_iternext(s);

    if (result == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    return result;
}

#ifdef	NOTYET
/*@null@*/
static PyObject *
rpmfi_NextD(rpmfiObject * s, PyObject * args)
	/*@*/
{
	if (!PyArg_ParseTuple(args, ":NextD"))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

/*@null@*/
static PyObject *
rpmfi_InitD(rpmfiObject * s, PyObject * args)
	/*@*/
{
	if (!PyArg_ParseTuple(args, ":InitD"))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}
#endif

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
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
 {"FColor",	(PyCFunction)rpmfi_FColor,	METH_VARARGS,
	NULL},
 {"FClass",	(PyCFunction)rpmfi_FClass,	METH_VARARGS,
	NULL},
 {"next",	(PyCFunction)rpmfi_Next,	METH_VARARGS,
"fi.next() -> (FN, FSize, FMode, FMtime, FFlags, FRdev, FInode, FNlink, FState, VFlags, FUser, FGroup, FMD5))\n\
- Retrieve next file info tuple.\n" },
#ifdef	NOTYET
 {"NextD",	(PyCFunction)rpmfi_NextD,	METH_VARARGS,
	NULL},
 {"InitD",	(PyCFunction)rpmfi_InitD,	METH_VARARGS,
	NULL},
#endif
 {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

static void
rpmfi_dealloc(/*@only@*/ /*@null@*/ rpmfiObject * s)
	/*@modifies s @*/
{
    if (s) {
	s->fi = rpmfiFree(s->fi);
	PyObject_Del(s);
    }
}

static int
rpmfi_print(rpmfiObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies s, fp, fileSystem @*/
{
    if (!(s && s->fi))
	return -1;

    s->fi = rpmfiInit(s->fi, 0);
    while (rpmfiNext(s->fi) >= 0)
	fprintf(fp, "%s\n", rpmfiFN(s->fi));
    return 0;
}

static PyObject * rpmfi_getattro(PyObject * o, PyObject * n)
	/*@*/
{
    return PyObject_GenericGetAttr(o, n);
}

static int rpmfi_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr(o, n, v);
}

static int
rpmfi_length(rpmfiObject * s)
	/*@*/
{
    return rpmfiFC(s->fi);
}

/*@null@*/
static PyObject *
rpmfi_subscript(rpmfiObject * s, PyObject * key)
	/*@modifies s @*/
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

/*@unchecked@*/ /*@observer@*/
static PyMappingMethods rpmfi_as_mapping = {
        (inquiry) rpmfi_length,		/* mp_length */
        (binaryfunc) rpmfi_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

/** \ingroup py_c
 */
static int rpmfi_init(rpmfiObject * s, PyObject *args, PyObject *kwds)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies s, rpmGlobalMacroContext @*/
{
    hdrObject * ho = NULL;
    PyObject * to = NULL;
    rpmts ts = NULL;	/* XXX FIXME: fiFromHeader should be a ts method. */
    int tagN = RPMTAG_BASENAMES;
    int flags = 0;

if (_rpmfi_debug < 0)
fprintf(stderr, "*** rpmfi_init(%p,%p,%p)\n", s, args, kwds);

    if (!PyArg_ParseTuple(args, "O!|Oi:rpmfi_init", &hdr_Type, &ho, &to, &flags))
	return -1;
    if (to != NULL) {
	tagN = tagNumFromPyObject(to);
	if (tagN == -1) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return -1;
	}
    }
    s->fi = rpmfiNew(ts, hdrGetHeader(ho), tagN, flags);
    s->active = 0;

    return 0;
}

/** \ingroup py_c
 */
static void rpmfi_free(/*@only@*/ rpmfiObject * s)
	/*@modifies s @*/
{
if (_rpmfi_debug)
fprintf(stderr, "%p -- fi %p\n", s, s->fi);
    s->fi = rpmfiFree(s->fi);

    PyObject_Del((PyObject *)s);
}

/** \ingroup py_c
 */
static PyObject * rpmfi_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * s = PyType_GenericAlloc(subtype, nitems);

if (_rpmfi_debug < 0)
fprintf(stderr, "*** rpmfi_alloc(%p,%d) ret %p\n", subtype, nitems, s);
    return s;
}

/** \ingroup py_c
 */
static PyObject * rpmfi_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/
{
    rpmfiObject * s = (void *) PyObject_New(rpmfiObject, subtype);

    /* Perform additional initialization. */
    if (rpmfi_init(s, args, kwds) < 0) {
	rpmfi_free(s);
	return NULL;
    }

if (_rpmfi_debug)
fprintf(stderr, "%p ++ fi %p\n", s, s->fi);

    return (PyObject *)s;
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmfi_doc[] =
"";

/*@-fullinitblock@*/
PyTypeObject rpmfi_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.fi",			/* tp_name */
	sizeof(rpmfiObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmfi_dealloc,	/* tp_dealloc */
	(printfunc) rpmfi_print,	/* tp_print */
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmfi_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	(getattrofunc) rpmfi_getattro,	/* tp_getattro */
	(setattrofunc) rpmfi_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmfi_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc) rpmfi_iter,	/* tp_iter */
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
	(allocfunc) rpmfi_alloc,	/* tp_alloc */
	(newfunc) rpmfi_new,		/* tp_new */
	rpmfi_free,			/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

/* ---------- */

rpmfi fiFromFi(rpmfiObject * s)
{
    return s->fi;
}

rpmfiObject *
rpmfi_Wrap(rpmfi fi)
{
    rpmfiObject *s = PyObject_New(rpmfiObject, &rpmfi_Type);

    if (s == NULL)
	return NULL;
    s->fi = fi;
    s->active = 0;
    return s;
}

rpmfiObject *
hdr_fiFromHeader(PyObject * s, PyObject * args)
{
    hdrObject * ho = (hdrObject *)s;
    PyObject * to = NULL;
    rpmts ts = NULL;	/* XXX FIXME: fiFromHeader should be a ts method. */
    rpmTag tagN = RPMTAG_BASENAMES;
    int flags = 0;

    if (!PyArg_ParseTuple(args, "|Oi:fiFromHeader", &to, &flags))
	return NULL;
    if (to != NULL) {
	tagN = tagNumFromPyObject(to);
	if (tagN == -1) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return NULL;
	}
    }
    return rpmfi_Wrap( rpmfiNew(ts, hdrGetHeader(ho), tagN, flags) );
}
