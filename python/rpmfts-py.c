/** \ingroup python
 * \file python/rpmfts-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include <fts.h>

#include <rpmlib.h>	/* XXX _free */

#include "rpmfts-py.h"

#include "debug.h"

/*@unchecked@*/
static int _rpmfts_debug = 1;

static char * ftsPaths[] = {
    "/usr/share/doc",
    NULL
};

static int ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

static const char * ftsInfoStrings[] = {
    "UNKNOWN",
    "D",
    "DC",
    "DEFAULT",
    "DNR",
    "DOT",
    "DP",
    "ERR",
    "F",
    "INIT",
    "NS",
    "NSOK",
    "SL",
    "SLNONE",
    "W",
};

static const char * ftsInfoStr(int fts_info)
	/*@*/
{
    if (!(fts_info >= 1 && fts_info <= 14))
	fts_info = 0; 
    return ftsInfoStrings[ fts_info ];
}

/** \ingroup python
 * \name Class: Rpmfts
 * \class Rpmfts
 * \brief A python rpm.fts object represents an rpm fts(3) handle.
 */

static PyObject *
rpmfts_Debug(rpmftsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i:Debug", &_rpmfts_debug)) return NULL;

    return (PyObject *)s;
}

static PyObject *
rpmfts_Open(rpmftsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":Open")) return NULL;

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_Open(%p) ftsp %p fts %p\n", s, s->ftsp, s->fts);

    s->ftsp = Fts_open(ftsPaths, ftsOpts, NULL);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmfts_Read(rpmftsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":Read")) return NULL;

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_Read(%p) ftsp %p fts %p\n", s, s->ftsp, s->fts);

    if (!(s && s->ftsp))
	return NULL;

    s->fts = Fts_read(s->ftsp);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmfts_Children(rpmftsObject * s, PyObject * args)
	/*@*/
{
    int instr = 0;

    if (!PyArg_ParseTuple(args, ":Children")) return NULL;

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_Children(%p) ftsp %p fts %p\n", s, s->ftsp, s->fts);

    if (!(s && s->ftsp))
	return NULL;

    s->fts = Fts_children(s->ftsp, instr);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmfts_Close(rpmftsObject * s, PyObject * args)
	/*@*/
{
    int rc = 0;

    if (!PyArg_ParseTuple(args, ":Close")) return NULL;

    if (s->ftsp) {
	rc = Fts_close(s->ftsp);
	s->ftsp = NULL;
	s->fts = NULL;
    }

    return Py_BuildValue("i", rc);
}

static PyObject *
rpmfts_Set(rpmftsObject * s, PyObject * args)
	/*@*/
{
    int instr = 0;
    int rc = 0;

    if (!PyArg_ParseTuple(args, ":Set")) return NULL;

    if (s->ftsp && s->fts)
	rc = Fts_set(s->ftsp, s->fts, instr);

    return Py_BuildValue("i", rc);
}

/** \ingroup python
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmfts_methods[] = {
    {"Debug",	(PyCFunction)rpmfts_Debug,	METH_VARARGS,
	NULL},
    {"open",	(PyCFunction)rpmfts_Open,	METH_VARARGS,
	NULL},
    {"read",	(PyCFunction)rpmfts_Read,	METH_VARARGS,
	NULL},
    {"children",(PyCFunction)rpmfts_Children,	METH_VARARGS,
	NULL},
    {"close",	(PyCFunction)rpmfts_Close,	METH_VARARGS,
	NULL},
    {"set",	(PyCFunction)rpmfts_Set,	METH_VARARGS,
	NULL},
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

/** \ingroup python
 */
static void
rpmfts_dealloc(/*@only@*/ /*@null@*/ rpmftsObject * s)
	/*@modifies s @*/
{
    if (s) {
	if (s->ftsp) {
	    (void) Fts_close(s->ftsp);
	    s->ftsp = NULL;
	    s->fts = NULL;
	}
	PyObject_Del(s);
    }
}

/** \ingroup python
 */
static int
rpmfts_print(rpmftsObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@*/
{
    int indent = 2;
    if (!(s && s->ftsp && s->fts))
	return -1;
    fprintf(fp, "FTS_%-7s %*s%s", ftsInfoStr(s->fts->fts_info),
	indent * (s->fts->fts_level < 0 ? 0 : s->fts->fts_level), "",
	s->fts->fts_name);
    return 0;

}

/** \ingroup python
 */
static PyObject * rpmfts_getattr(rpmftsObject * o, char * name)
	/*@*/
{
    return Py_FindMethod(rpmfts_methods, (PyObject *) o, name);
}

/** \ingroup python
 */
static int rpmfts_init(rpmftsObject * s, PyObject *args, PyObject *kwds)
	/*@*/
{
if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_init(%p,%p,%p)\n", s, args, kwds);

    if (!PyArg_ParseTuple(args, ":rpmfts_init"))
	return -1;

    s->ftsp = NULL;
    s->fts = NULL;

    return 0;
}

/** \ingroup python
 */
static void rpmfts_free(/*@only@*/ rpmftsObject * s)
	/*@modifies s @*/
{
if (_rpmfts_debug)
fprintf(stderr, "%p -- fts %p\n", s, s->ftsp);

    if (s->ftsp) {
	(void) Fts_close(s->ftsp);
	s->ftsp = NULL;
	s->fts = NULL;
    }

    PyObject_Del((PyObject *)s);
}

/** \ingroup python
 */
static PyObject * rpmfts_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * s = PyType_GenericAlloc(subtype, nitems);

if (_rpmfts_debug)
fprintf(stderr, "*** rpmfts_alloc(%p,%d) ret %p\n", subtype, nitems, s);
    return s;
}

/** \ingroup python
 */
static rpmftsObject * rpmfts_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@*/
{
    rpmftsObject * s = PyObject_New(rpmftsObject, subtype);

    /* Perform additional initialization. */
    if (rpmfts_init(s, args, kwds) < 0) {
	rpmfts_free(s);
	return NULL;
    }

if (_rpmfts_debug)
fprintf(stderr, "%p ++ fts %p\n", s, s->ftsp);

    return s;
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmfts_doc[] =
"";

/** \ingroup python
 */
/*@-fullinitblock@*/
PyTypeObject rpmfts_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.fts",			/* tp_name */
	sizeof(rpmftsObject),		/* tp_size */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmfts_dealloc, 	/* tp_dealloc */
	(printfunc) rpmfts_print,	/* tp_print */
	(getattrfunc) rpmfts_getattr, 	/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmfts_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmfts_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmfts_init,		/* tp_init */
	(allocfunc) rpmfts_alloc,	/* tp_alloc */
	(newfunc) rpmfts_new,		/* tp_new */
	(destructor) rpmfts_free,	/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

rpmftsObject * rpmfts_Wrap(FTSENT * fts)
{
    rpmftsObject *s = PyObject_New(rpmftsObject, &rpmfts_Type);
    if (s == NULL)
	return NULL;
    return s;
}
