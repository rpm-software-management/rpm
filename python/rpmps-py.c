/** \ingroup py_c
 * \file python/rpmps-py.c
 */

#include "system.h"

#include <rpmlib.h>

#include "header-py.h"
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

/*@null@*/
static PyObject *
rpmps_NumProblems(rpmpsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":NumProblems")) return NULL;
    return Py_BuildValue("i", rpmpsNumProblems(s->ps));
}

static PyObject *
rpmps_iter(rpmpsObject * s)
	/*@*/
{
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

    /* Reset loop indices on 1st entry. */
    if (!s->active) {
	s->ix = -1;
	s->active = 1;
    }

    /* If more to do, return a problem set tuple. */
    s->ix++;
    if (s->ix < s->ps->numProblems) {
	/* TODO */
	result = Py_BuildValue("i", s->ix);
    } else {
	s->active = 0;
    }

    return result;
}

/*@null@*/
static PyObject *
rpmps_Next(rpmpsObject * s, PyObject *args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result;

    if (!PyArg_ParseTuple(args, ":Next"))
	return NULL;

    result = rpmps_iternext(s);

    if (result == NULL) {
	Py_INCREF(Py_None);
        return Py_None;
    }
    return result;
}

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmps_methods[] = {
 {"Debug",	(PyCFunction)rpmps_Debug,	METH_VARARGS,
	NULL},
 {"NumProblems",(PyCFunction)rpmps_NumProblems,	METH_VARARGS,
	"ps.NumProblems -> NumProblems	- Return no. of elements.\n" },
 {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

static void
rpmps_dealloc(rpmpsObject * s)
	/*@modifies s @*/
{
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
    if (!(s && s->ps))
	return -1;

    for (s->ix = 0; s->ix < rpmpsNumProblems(s->ps); s->ix++) {
	fprintf(fp, "%d\n", s->ix);	/* TODO */
    }
    return 0;
}

static PyObject * rpmps_getattro(PyObject * o, PyObject * n)
	/*@*/
{
    return PyObject_GenericGetAttr(o, n);
}

static int rpmps_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr(o, n, v);
}

static int
rpmps_length(rpmpsObject * s)
	/*@*/
{
    return rpmpsNumProblems(s->ps);
}

/*@null@*/
static PyObject *
rpmps_subscript(rpmpsObject * s, PyObject * key)
	/*@modifies s @*/
{
    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    s->ix = (int) PyInt_AsLong(key);
    /* TODO */
    return Py_BuildValue("i", s->ix);
}

static PyMappingMethods rpmps_as_mapping = {
        (inquiry) rpmps_length,		/* mp_length */
        (binaryfunc) rpmps_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

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
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

/* ---------- */
