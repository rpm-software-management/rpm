/** \ingroup py_c
 * \file python/rpmbc-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "rpmbc-py.h"

#include "rpmdebug-py.c"

#include "debug.h"

/*@unchecked@*/
static int _bc_debug = 0;

#define is_rpmbc(o)	((o)->ob_type == &rpmbc_Type)

/*@unchecked@*/ /*@observer@*/
static const char initialiser_name[] = "rpm.bc";

/* ---------- */

static void
rpmbc_dealloc(rpmbcObject * s)
	/*@*/
{
if (_bc_debug < -1)
fprintf(stderr, "*** rpmbc_dealloc(%p)\n", s);

    PyObject_Del(s);
}

static int
rpmbc_print(rpmbcObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@*/
{
if (_bc_debug < -1)
fprintf(stderr, "*** rpmbc_print(%p)\n", s);
    return 0;
}

/** \ingroup py_c
 */
static int rpmbc_init(rpmbcObject * z, PyObject *args, PyObject *kwds)
	/*@*/
{
    PyObject * o = NULL;

    if (!PyArg_ParseTuple(args, "|O:Cvt", &o)) return -1;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_init(%p[%s],%p[%s],%p[%s])\n", z, lbl(z), args, lbl(args), kwds, lbl(kwds));

    return 0;
}

/** \ingroup py_c
 */
static void rpmbc_free(/*@only@*/ rpmbcObject * s)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_free(%p[%s])\n", s, lbl(s));
    PyObject_Del(s);
}

/** \ingroup py_c
 */
static PyObject * rpmbc_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * ns = PyType_GenericAlloc(subtype, nitems);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_alloc(%p[%s},%d) ret %p[%s]\n", subtype, lbl(subtype), nitems, ns, lbl(ns));
    return (PyObject *) ns;
}

static PyObject *
rpmbc_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@*/
{
    PyObject * ns = (PyObject *) PyObject_New(rpmbcObject, &rpmbc_Type);

if (_bc_debug < -1)
fprintf(stderr, "*** rpmbc_new(%p[%s],%p[%s],%p[%s]) ret %p[%s]\n", subtype, lbl(subtype), args, lbl(args), kwds, lbl(kwds), ns, lbl(ns));
    return ns;
}

static rpmbcObject *
rpmbc_New(void)
{
    rpmbcObject * ns = PyObject_New(rpmbcObject, &rpmbc_Type);

    return ns;
}

/* ---------- */

/** \ingroup py_c
 */
static PyObject *
rpmbc_Debug(/*@unused@*/ rpmbcObject * s, PyObject * args)
        /*@globals _Py_NoneStruct @*/
        /*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i:Debug", &_bc_debug)) return NULL;

if (_bc_debug < 0)
fprintf(stderr, "*** rpmbc_Debug(%p)\n", s);

    Py_INCREF(Py_None);
    return Py_None;
}

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmbc_methods[] = {
 {"Debug",	(PyCFunction)rpmbc_Debug,	METH_VARARGS,
	NULL},
 {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

static PyObject *
rpmbc_getattr(PyObject * s, char * name)
	/*@*/
{
    return Py_FindMethod(rpmbc_methods, s, name);
}

/* ---------- */

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmbc_doc[] =
"";

/*@-fullinitblock@*/
PyTypeObject rpmbc_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.bc",			/* tp_name */
	sizeof(rpmbcObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmbc_dealloc,	/* tp_dealloc */
	(printfunc) rpmbc_print,	/* tp_print */
	(getattrfunc) rpmbc_getattr,	/* tp_getattr */
	(setattrfunc) 0,		/* tp_setattr */
	(cmpfunc) 0,			/* tp_compare */
	(reprfunc) 0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc) 0,			/* tp_hash */
	(ternaryfunc) 0,		/* tp_call */
	(reprfunc) 0,			/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmbc_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc) 0,		/* tp_iter */
	(iternextfunc) 0,		/* tp_iternext */
	rpmbc_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmbc_init,		/* tp_init */
	(allocfunc) rpmbc_alloc,	/* tp_alloc */
	(newfunc) rpmbc_new,		/* tp_new */
	(destructor) rpmbc_free,	/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

/* ---------- */
