/** \ingroup py_c
 * \file python/rpmps-py.c
 */

#include "system.h"

#include <rpmlib.h>

#include "rpmdebug-py.c"

#include "rpmps-py.h"

#include "debug.h"

/*@access rpmps @*/

/*@null@*/
static PyObject *
rpmps_Debug(/*@unused@*/ rpmpsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i", &_rpmps_debug)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmps_iter(rpmpsObject * s)
	/*@*/
{
if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_iter(%p)\n", s);
    s->ix = -1;
    Py_INCREF(s);
    return (PyObject *)s;
}

/*@null@*/
static PyObject *
rpmps_iternext(rpmpsObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result = NULL;
    rpmps ps = s->ps;

if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_iternext(%p) ps %p ix %d active %d\n", s, s->ps, s->ix, s->active);
    /* Reset loop indices on 1st entry. */
    if (!s->active) {
	s->ix = -1;
	s->active = 1;
    }

    /* If more to do, return a problem set string. */
    s->ix++;
    if (s->ix < ps->numProblems) {
	result = Py_BuildValue("s", rpmProblemString(ps->probs+s->ix));
    } else {
	s->active = 0;
    }

    return result;
}

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmps_methods[] = {
 {"Debug",	(PyCFunction)rpmps_Debug,	METH_VARARGS,
	NULL},
 {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

static void
rpmps_dealloc(rpmpsObject * s)
	/*@modifies s @*/
{
if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_dealloc(%p)\n", s);
    if (s) {
	s->ps = rpmpsFree(s->ps);
	PyObject_Del(s);
    }
}

static int
rpmps_print(rpmpsObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies s, fp, fileSystem @*/
{
if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_print(%p,%p,%x)\n", s, fp, flags);
    if (s && s->ps)
	rpmpsPrint(fp, s->ps);
    return 0;
}

static PyObject * rpmps_getattro(PyObject * o, PyObject * n)
	/*@*/
{
if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_getattro(%p,%p)\n", o, n);
    return PyObject_GenericGetAttr(o, n);
}

static int rpmps_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_setattro(%p,%p,%p)\n", o, n, v);
    return PyObject_GenericSetAttr(o, n, v);
}

static int
rpmps_length(rpmpsObject * s)
	/*@*/
{
    int rc;
    rc = rpmpsNumProblems(s->ps);
if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_length(%p) rc %d\n", s, rc);
    return rc;
}

/*@null@*/
static PyObject *
rpmps_subscript(rpmpsObject * s, PyObject * key)
	/*@modifies s @*/
{
    PyObject * result = NULL;
    rpmps ps;
    int ix;

    if (!PyInt_Check(key)) {
if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_subscript(%p[%s],%p[%s])\n", s, lbl(s), key, lbl(key));
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    ix = (int) PyInt_AsLong(key);
    /* XXX range check */
    ps = s->ps;
    if (ix < ps->numProblems) {
	result = Py_BuildValue("s", rpmProblemString(ps->probs + ix));
if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_subscript(%p,%p) %s\n", s, key, PyString_AsString(result));
    }

    return result;
}

static int
rpmps_ass_sub(rpmpsObject * s, PyObject * key, PyObject * value)
{
    rpmps ps;
    int ix;

    if (!PyArg_Parse(key, "i:ass_sub", &ix)) {
	PyErr_SetString(PyExc_TypeError, "rpmps key type must be integer");
	return -1;
    }

    /* XXX get rid of negative indices */
    if (ix < 0) ix = -ix;

    ps = s->ps;

if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_ass_sub(%p[%s],%p[%s],%p[%s]) ps %p[%d:%d:%d]\n", s, lbl(s), key, lbl(key), value, lbl(value), ps, ix, ps->numProblems, ps->numProblemsAlloced);

    if (value == NULL) {
	if (ix < ps->numProblems) {
	    rpmProblem op = ps->probs + ix;
	    
	    op->pkgNEVR = _free(op->pkgNEVR);
	    op->altNEVR = _free(op->altNEVR);
	    op->str1 = _free(op->str1);

	    if ((ix+1) == ps->numProblems)
		memset(op, 0, sizeof(*op));
	    else
		memmove(op, op+1, (ps->numProblems - ix) * sizeof(*op));
	    if (ps->numProblems > 0)
		ps->numProblems--;
	}
    } else {
	rpmProblem p = memset(alloca(sizeof(*p)), 0, sizeof(*p));

	if (!PyArg_ParseTuple(value, "ssOiisN:rpmps value tuple",
				&p->pkgNEVR, &p->altNEVR, &p->key,
				&p->type, &p->ignoreProblem, &p->str1,
				&p->ulong1))
	{
	    return -1;
	}

	if (ix >= ps->numProblems) {
	    /* XXX force append for indices out of range. */
	    rpmpsAppend(s->ps, p->type, p->pkgNEVR, p->key,
		p->str1, NULL, p->altNEVR, p->ulong1);
	} else {
	    rpmProblem op = ps->probs + ix;

	    op->pkgNEVR = _free(op->pkgNEVR);
	    op->altNEVR = _free(op->altNEVR);
	    op->str1 = _free(op->str1);

	    p->pkgNEVR = (p->pkgNEVR && *p->pkgNEVR ? xstrdup(p->pkgNEVR) : NULL);
	    p->altNEVR = (p->altNEVR && *p->altNEVR ? xstrdup(p->altNEVR) : NULL);
	    p->str1 = (p->str1 && *p->str1 ? xstrdup(p->str1) : NULL);

	    *op = *p;	/* structure assignment */
	}
    }

    return 0;
}

static PyMappingMethods rpmps_as_mapping = {
        (inquiry) rpmps_length,		/* mp_length */
        (binaryfunc) rpmps_subscript,	/* mp_subscript */
        (objobjargproc) rpmps_ass_sub,	/* mp_ass_subscript */
};

/** \ingroup py_c
 */
static int rpmps_init(rpmpsObject * s, PyObject *args, PyObject *kwds)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies s, rpmGlobalMacroContext @*/
{

if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_init(%p,%p,%p)\n", s, args, kwds);

    if (!PyArg_ParseTuple(args, ":rpmps_init"))
	return -1;

    s->ps = rpmpsCreate();
    s->active = 0;
    s->ix = -1;

    return 0;
}

/** \ingroup py_c
 */
static void rpmps_free(/*@only@*/ rpmpsObject * s)
	/*@modifies s @*/
{
if (_rpmps_debug)
fprintf(stderr, "%p -- ps %p\n", s, s->ps);
    s->ps = rpmpsFree(s->ps);

    PyObject_Del((PyObject *)s);
}

/** \ingroup py_c
 */
static PyObject * rpmps_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * s = PyType_GenericAlloc(subtype, nitems);

if (_rpmps_debug < 0)
fprintf(stderr, "*** rpmps_alloc(%p,%d) ret %p\n", subtype, nitems, s);
    return s;
}

/** \ingroup py_c
 */
static PyObject * rpmps_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/
{
    rpmpsObject * s = (void *) PyObject_New(rpmpsObject, subtype);

    /* Perform additional initialization. */
    if (rpmps_init(s, args, kwds) < 0) {
	rpmps_free(s);
	return NULL;
    }

if (_rpmps_debug)
fprintf(stderr, "%p ++ ps %p\n", s, s->ps);

    return (PyObject *)s;
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmps_doc[] =
"";

/*@-fullinitblock@*/
PyTypeObject rpmps_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.ps",			/* tp_name */
	sizeof(rpmpsObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmps_dealloc,	/* tp_dealloc */
	(printfunc) rpmps_print,	/* tp_print */
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmps_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	(getattrofunc) rpmps_getattro,	/* tp_getattro */
	(setattrofunc) rpmps_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT, 		/* tp_flags */
	rpmps_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	(richcmpfunc)0,			/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc) rpmps_iter,	/* tp_iter */
	(iternextfunc) rpmps_iternext,	/* tp_iternext */
	rpmps_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmps_init,		/* tp_init */
	(allocfunc) rpmps_alloc,	/* tp_alloc */
	(newfunc) rpmps_new,		/* tp_new */
	rpmps_free,			/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

/* ---------- */

rpmps psFromPs(rpmpsObject * s)
{
    return s->ps;
}

rpmpsObject *
rpmps_Wrap(rpmps ps)
{
    rpmpsObject * s = PyObject_New(rpmpsObject, &rpmps_Type);

    if (s == NULL)
	return NULL;
    s->ps = ps;
    s->active = 0;
    s->ix = -1;
    return s;
}
