/** \ingroup py_c
 * \file python/rpmrng-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "rpmrng-py.h"

#include "rpmdebug-py.c"

#include "debug.h"

/*@unchecked@*/
static int _rng_debug = 0;

/*@unchecked@*/ /*@observer@*/
static const char initialiser_name[] = "rpm.rng";

/* ---------- */

static void
rng_dealloc(rngObject * s)
	/*@modifies s @*/
{
if (_rng_debug < -1)
fprintf(stderr, "*** rng_dealloc(%p)\n", s);

    randomGeneratorContextFree(&s->rngc);
    mpbfree(&s->b);
    PyObject_Del(s);
}

static int
rng_print(rngObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies fp, fileSystem @*/
{
if (_rng_debug < -1)
fprintf(stderr, "*** rng_print(%p)\n", s);
    return 0;
}

/** \ingroup py_c
 */
static int rng_init(rngObject * s, PyObject *args, PyObject *kwds)
	/*@modifies s @*/
{
    PyObject * o = NULL;
    const randomGenerator* rng = NULL;

    if (!PyArg_ParseTuple(args, "|O:Cvt", &o)) return -1;

    if (o) {
	/* XXX "FIPS 186" or "Mersenne Twister" */
	if (PyString_Check(o))
	    rng = randomGeneratorFind(PyString_AsString(o));
    }

    if (rng == NULL)
	rng = randomGeneratorDefault();

    if (randomGeneratorContextInit(&s->rngc, rng) != 0)
	return -1;
    mpbzero(&s->b);

if (_rng_debug)
fprintf(stderr, "*** rng_init(%p[%s],%p[%s],%p[%s])\n", s, lbl(s), args, lbl(args), kwds, lbl(kwds));

    return 0;
}

/** \ingroup py_c
 */
static void rng_free(/*@only@*/ rngObject * s)
	/*@modifies s @*/
{
if (_rng_debug)
fprintf(stderr, "*** rng_free(%p[%s])\n", s, lbl(s));
    randomGeneratorContextFree(&s->rngc);
    mpbfree(&s->b);
    PyObject_Del(s);
}

/** \ingroup py_c
 */
static PyObject * rng_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * ns = PyType_GenericAlloc(subtype, nitems);

if (_rng_debug)
fprintf(stderr, "*** rng_alloc(%p[%s},%d) ret %p[%s]\n", subtype, lbl(subtype), nitems, ns, lbl(ns));
    return (PyObject *) ns;
}

static PyObject *
rng_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@*/
{
    PyObject * ns = (PyObject *) PyObject_New(rngObject, &rng_Type);

if (_rng_debug < -1)
fprintf(stderr, "*** rng_new(%p[%s],%p[%s],%p[%s]) ret %p[%s]\n", subtype, lbl(subtype), args, lbl(args), kwds, lbl(kwds), ns, lbl(ns));
    return ns;
}

static rngObject *
rng_New(void)
{
    rngObject * ns = PyObject_New(rngObject, &rng_Type);

    return ns;
}

/* ---------- */

/** \ingroup py_c
 */
static PyObject *
rng_Debug(/*@unused@*/ rngObject * s, PyObject * args)
        /*@globals _Py_NoneStruct @*/
        /*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i:Debug", &_rng_debug)) return NULL;

if (_rng_debug < 0)
fprintf(stderr, "*** rng_Debug(%p)\n", s);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup py_c
 */
static PyObject *
rng_Seed(rngObject * s, PyObject * args)
        /*@*/
{
    if (!PyArg_ParseTuple(args, ":Seed")) return NULL;

if (_rng_debug < 0)
fprintf(stderr, "*** rng_Seed(%p)\n", s);

    Py_INCREF(Py_None);
    return Py_None;
}

/** \ingroup py_c
 */
static PyObject *
rng_Next(rngObject * s, PyObject * args)
        /*@*/
{
    PyObject * bo = NULL;
    randomGeneratorContext* rc = &s->rngc;
    mpbarrett* b = &s->b;
    mpwObject *z;

    if (!PyArg_ParseTuple(args, "|O:Next", &bo)) return NULL;

    if (bo) {
	if (is_mpw(bo) && (z = (mpwObject*)bo)->n.size > 0) {
	    b = alloca(sizeof(*b));
	    mpbzero(b);
	    /* XXX z probably needs normalization here. */
	    mpbset(b, z->n.size, z->n.data);
	} else
	    ;	/* XXX error? */
    }

    z = mpw_New();
    if (b == NULL || b->size == 0 || b->modl == NULL) {
	mpw val;
	rc->rng->next(rc->param, (byte*) &val, sizeof(val));
	mpnsetw(&z->n, val);
    } else {
	mpw* wksp = alloca(b->size*sizeof(*wksp));
	mpnsize(&z->n, b->size);
	mpbrnd_w(b, rc, z->n.data, wksp);
    }

if (_rng_debug)
fprintf(stderr, "*** rng_Next(%p) %p[%d]\t", s, z->n.data, z->n.size), mpprintln(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rng_methods[] = {
 {"Debug",	(PyCFunction)rng_Debug,	METH_VARARGS,
	NULL},
 {"seed",	(PyCFunction)rng_Seed,	METH_VARARGS,
	NULL},
 {"next",	(PyCFunction)rng_Next,	METH_VARARGS,
	NULL},
 {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

static PyObject *
rng_getattr(PyObject * s, char * name)
	/*@*/
{
    return Py_FindMethod(rng_methods, s, name);
}

/* ---------- */

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rng_doc[] =
"";

/*@-fullinitblock@*/
PyTypeObject rng_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.rng",			/* tp_name */
	sizeof(rngObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rng_dealloc,	/* tp_dealloc */
	(printfunc) rng_print,		/* tp_print */
	(getattrfunc) rng_getattr,	/* tp_getattr */
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
	rng_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc) 0,		/* tp_iter */
	(iternextfunc) 0,		/* tp_iternext */
	rng_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rng_init,		/* tp_init */
	(allocfunc) rng_alloc,		/* tp_alloc */
	(newfunc) rng_new,		/* tp_new */
	(destructor) rng_free,		/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

/* ---------- */
