/** \ingroup python
 * \file python/rpmrc-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "structmember.h"

/*@unchecked@*/
extern PyTypeObject PyDictIter_Type;

#include <rpmcli.h>

#include "header-py.h"
#include "rpmal-py.h"
#include "rpmdb-py.h"
#include "rpmds-py.h"
#include "rpmfd-py.h"
#include "rpmfi-py.h"
#include "rpmmi-py.h"
#include "rpmrc-py.h"
#include "rpmte-py.h"
#include "rpmts-py.h"

#include "debug.h"

/*@unchecked@*/
static int _rc_debug = 0;

/** \ingroup python
 * \class Rpmrc
 * \brief A python rpm.rc object encapsulates rpmlib configuration.
 */

/** \ingroup python
 * \name Class: rpm.rc
 */
/*@{*/

/**
 */
static const char * lbl(void * s)
	/*@*/
{
    PyObject * o = s;

    if (o == NULL)	return "null";

    if (o->ob_type == &PyType_Type)	return o->ob_type->tp_name;

    if (o->ob_type == &PyClass_Type)	return "Class";
    if (o->ob_type == &PyComplex_Type)	return "Complex";
    if (o->ob_type == &PyDict_Type)	return "Dict";
    if (o->ob_type == &PyDictIter_Type)	return "DictIter";
    if (o->ob_type == &PyFile_Type)	return "File";
    if (o->ob_type == &PyFloat_Type)	return "Float";
    if (o->ob_type == &PyFunction_Type)	return "Function";
    if (o->ob_type == &PyInt_Type)	return "Int";
    if (o->ob_type == &PyList_Type)	return "List";
    if (o->ob_type == &PyLong_Type)	return "Long";
    if (o->ob_type == &PyMethod_Type)	return "Method";
    if (o->ob_type == &PyModule_Type)	return "Module";
    if (o->ob_type == &PyString_Type)	return "String";
    if (o->ob_type == &PyTuple_Type)	return "Tuple";
    if (o->ob_type == &PyType_Type)	return "Type";
    if (o->ob_type == &PyUnicode_Type)	return "Unicode";

    if (o->ob_type == &hdr_Type)	return "hdr";
    if (o->ob_type == &rpmal_Type)	return "rpmal";
    if (o->ob_type == &rpmdb_Type)	return "rpmdb";
    if (o->ob_type == &rpmds_Type)	return "rpmds";
    if (o->ob_type == &rpmfd_Type)	return "rpmfd";
    if (o->ob_type == &rpmfi_Type)	return "rpmfi";
    if (o->ob_type == &rpmmi_Type)	return "rpmmi";
    if (o->ob_type == &rpmrc_Type)	return "rpmrc";
    if (o->ob_type == &rpmte_Type)	return "rpmte";
    if (o->ob_type == &rpmts_Type)	return "rpmts";

    return "Unknown";
}

/**
 */
PyObject * rpmrc_AddMacro(/*@unused@*/ PyObject * self, PyObject * args)
{
    char * name, * val;

    if (!PyArg_ParseTuple(args, "ss:AddMacro", &name, &val))
	return NULL;

    addMacro(NULL, name, NULL, val, -1);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
PyObject * rpmrc_DelMacro(/*@unused@*/ PyObject * self, PyObject * args)
{
    char * name;

    if (!PyArg_ParseTuple(args, "s:DelMacro", &name))
	return NULL;

    delMacro(NULL, name);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static PyObject *
rpmrc_getstate(rpmrcObject *s, PyObject *args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":getstate"))
	return NULL;
    return PyInt_FromLong(s->state);
}

/**
 */
static PyObject *
rpmrc_setstate(rpmrcObject *s, PyObject *args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    int state;

    if (!PyArg_ParseTuple(args, "i:setstate", &state))
	return NULL;
    s->state = state;
    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
static void rpmrc_dealloc(PyObject * s)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_dealloc(%p[%s])\n", s, lbl(s));
    PyDict_Type.tp_dealloc(s);
}

/**
 */
static int rpmrc_print(PyObject * s, FILE *fp, int flags)
	/*@*/
{
/*@-formattype@*/
if (_rc_debug)
fprintf(stderr, "*** rpmrc_print(%p[%s],%p,%x)\n", s, lbl(s), fp, flags);
/*@=formattype@*/
    return PyDict_Type.tp_print(s, fp, flags);
}

/**
 */
static int rpmrc_compare(PyObject * a, PyObject * b)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_compare(%p[%s],%p[%s])\n", a, lbl(a), b, lbl(b));
    return PyDict_Type.tp_compare(a, b);
}

/**
 */
static PyObject * rpmrc_repr(PyObject * s)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_repr(%p[%s])\n", s, lbl(s));
    return PyDict_Type.tp_repr(s);
}

/**
 */
static long rpmrc_hash(PyObject * s)
	/*@*/
{
    /* XXX dict objects are unhashable */
if (_rc_debug)
fprintf(stderr, "*** rpmrc_hash(%p[%s])\n", s, lbl(s));
    return PyDict_Type.tp_hash(s);
}

/**
 */
static int
rpmrc_length(PyObject * s)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_length(%p[%s])\n", s, lbl(s));
    return PyDict_Type.tp_as_mapping->mp_length(s);
}

/**
 */
static PyObject *
rpmrc_subscript(PyObject * s, PyObject * key)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_subscript(%p[%s], %p[%s])\n", s, lbl(s), key, lbl(key));
    return PyDict_Type.tp_as_mapping->mp_subscript(s, key);
}

/**
 */
static int
rpmrc_ass_subscript(PyObject * s, PyObject * key, PyObject * value)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_ass_subscript(%p[%s], %p[%s], %p[%s])\n", s, lbl(s), key, lbl(key), value, lbl(value));
    return PyDict_Type.tp_as_mapping->mp_ass_subscript(s, key, value);
}

/*@unchecked@*/ /*@observer@*/
static PyMappingMethods rpmrc_as_mapping = {
    rpmrc_length,		/* mp_length */
    rpmrc_subscript,		/* mp_subscript */
    rpmrc_ass_subscript,		/* mp_ass_subscript */
};

/**
 */
static PyObject * rpmrc_getattro (PyObject *s, PyObject *name)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_getattro(%p[%s], \"%s\")\n", s, lbl(s), PyString_AS_STRING(name));
    return PyObject_GenericGetAttr(s, name);
}

/**
 */
static int rpmrc_setattro (PyObject *s, PyObject *name, PyObject * value)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_setattro(%p[%s], \"%s \", \"%s\")\n", s, lbl(s), PyString_AS_STRING(name), PyString_AS_STRING(value));
    return PyDict_Type.tp_setattro(s, name, value);
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmrc_doc[] =
"";

/**
 */
static int rpmrc_traverse(PyObject * s, visitproc visit, void *arg)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_traverse(%p[%s],%p,%p)\n", s, lbl(s), visit, arg);
    return PyDict_Type.tp_traverse(s, visit, arg);
}

/**
 */
static int rpmrc_clear(PyObject * s)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_clear(%p[%s])\n", s, lbl(s));
    return PyDict_Type.tp_clear(s);
}

/**
 */
static PyObject * rpmrc_richcompare(PyObject * v, PyObject * w, int op)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_richcompare(%p[%s],%p[%s],%x)\n", v, lbl(v), w, lbl(w), op);
    return PyDict_Type.tp_richcompare(v, w, op);
}

/**
 */
static PyObject * rpmrc_iter(PyObject * s)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_iter(%p[%s])\n", s, lbl(s));
    if (s->ob_type == &PyDictIter_Type)
	return PyDictIter_Type.tp_iter(s);
    return PyDict_Type.tp_iter(s);
}

/**
 */
static PyObject * rpmrc_iternext(PyObject * s)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_iternext(%p[%s])\n", s, lbl(s));
    if (s->ob_type == &PyDictIter_Type)
	return PyDictIter_Type.tp_iternext(s);
    return NULL;
}

/**
 */
static PyObject * rpmrc_next(PyObject * s, PyObject *args)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_next(%p[%s],%p)\n", s, lbl(s), args);
    if (s->ob_type == &PyDictIter_Type)
	return PyDictIter_Type.tp_methods[0].ml_meth(s, args);
    return NULL;
}

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static PyMemberDef rpmrc_members[] = {
    {"state", T_INT, offsetof(rpmrcObject, state), READONLY,
         "an int variable for demonstration purposes"},
    {0}
};
/*@=fullinitblock@*/

/** \ingroup python
 */
static int rpmrc_init(PyObject * s, PyObject *args, PyObject *kwds)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_init(%p[%s],%p,%p)\n", s, lbl(s), args, kwds);
    if (PyDict_Type.tp_init(s, args, kwds) < 0)
	return -1;
    ((rpmrcObject *)s)->state = 0;
    return 0;
}

/** \ingroup python
 */
static void rpmrc_free(PyObject * s)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_free(%p[%s])\n", s, lbl(s));
   _PyObject_GC_Del(s);
}

/** \ingroup python
 */
static PyObject * rpmrc_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * ns = PyType_GenericAlloc(subtype, nitems);

if (_rc_debug)
fprintf(stderr, "*** rpmrc_alloc(%p[%s},%d) ret %p[%s]\n", subtype, lbl(subtype), nitems, ns, lbl(ns));
    return (PyObject *) ns;
}

/** \ingroup python
 */
static PyObject * rpmrc_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@*/
{
    PyObject * ns;

    /* Derive an initialized dictionary of the appropriate size. */
    ns = PyDict_Type.tp_new(&rpmrc_Type, args, kwds);

    /* Perform additional initialization. */
    if (rpmrc_init(ns, args, kwds) < 0) {
	rpmrc_free(ns);
	return NULL;
    }

if (_rc_debug)
fprintf(stderr, "*** rpmrc_new(%p[%s],%p,%p) ret %p[%s]\n", subtype, lbl(subtype), args, kwds, ns, lbl(ns));
    return ns;
}

/**
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmrc_methods[] = {
    { "addMacro",	(PyCFunction) rpmrc_AddMacro, METH_VARARGS,
	NULL },
    { "delMacro",	(PyCFunction) rpmrc_DelMacro, METH_VARARGS,
	NULL },
    { "getstate",	(PyCFunction) rpmrc_getstate, METH_VARARGS,
	"getstate() -> state"},
    { "setstate",	(PyCFunction) rpmrc_setstate, METH_VARARGS,
	"setstate(state)"},
    { "next",		(PyCFunction) rpmrc_next,     METH_VARARGS,
	"next() -- get the next value, or raise StopIteration"},
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/** \ingroup python
 */
/*@-fullinitblock@*/
PyTypeObject rpmrc_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.rc",			/* tp_name */
	sizeof(rpmrcObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor) rpmrc_dealloc, 	/* tp_dealloc */
	rpmrc_print,			/* tp_print */
	0,			 	/* tp_getattr */
	0,				/* tp_setattr */
	rpmrc_compare,			/* tp_compare */
	rpmrc_repr,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmrc_as_mapping,		/* tp_as_mapping */
	rpmrc_hash,			/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	(getattrofunc) rpmrc_getattro,	/* tp_getattro */
	(setattrofunc) rpmrc_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmrc_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	rpmrc_traverse,			/* tp_traverse */
	rpmrc_clear,			/* tp_clear */
	rpmrc_richcompare,		/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	rpmrc_iter,			/* tp_iter */
	rpmrc_iternext,			/* tp_iternext */
	rpmrc_methods,			/* tp_methods */
	rpmrc_members,			/* tp_members */
	0,				/* tp_getset */
	&PyDict_Type,			/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	rpmrc_init,			/* tp_init */
	rpmrc_alloc,			/* tp_alloc */
	rpmrc_new,			/* tp_new */
	rpmrc_free,			/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

PyObject * rpmrc_Create(/*@unused@*/ PyObject * self, PyObject *args, PyObject *kwds)
{
    return rpmrc_new(&rpmrc_Type, args, kwds);
}

/*@}*/
