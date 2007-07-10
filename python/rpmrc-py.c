/** \ingroup py_c
 * \file python/rpmrc-py.c
 */

#include "system.h"

#include "structmember.h"

#include "rpmdebug-py.c"

#include <rpmcli.h>

#include "rpmrc-py.h"

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
PyObject *
rpmrc_AddMacro(/*@unused@*/ PyObject * self, PyObject * args, PyObject * kwds)
{
    char * name, * val;
    char * kwlist[] = {"name", "value", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss:AddMacro", kwlist,
	    &name, &val))
	return NULL;

    addMacro(NULL, name, NULL, val, -1);

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
PyObject *
rpmrc_DelMacro(/*@unused@*/ PyObject * self, PyObject * args, PyObject * kwds)
{
    char * name;
    char * kwlist[] = {"name", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:DelMacro", kwlist, &name))
	return NULL;

    delMacro(NULL, name);

    Py_INCREF(Py_None);
    return Py_None;
}

#if Py_TPFLAGS_HAVE_ITER	/* XXX backport to python-1.5.2 */
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

#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 4
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
/*@null@*/
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
/*@null@*/
/* XXX: does this _actually_ take any arguments?  I don't think it does... */
static PyObject * rpmrc_next(PyObject * s, PyObject *args)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_next(%p[%s],%p)\n", s, lbl(s), args);
    if (s->ob_type == &PyDictIter_Type)
	return PyDictIter_Type.tp_methods[0].ml_meth(s, args);
    return NULL;
}
#else
#define	rpmrc_iter	0
#define	rpmrc_iternext	0
#endif

/** \ingroup py_c
 */
static int rpmrc_init(PyObject * s, PyObject *args, PyObject *kwds)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_init(%p[%s],%p,%p)\n", s, lbl(s), args, kwds);
    if (PyDict_Type.tp_init(s, args, kwds) < 0)
	return -1;
    return 0;
}

/** \ingroup py_c
 */
static void rpmrc_free(PyObject * s)
	/*@*/
{
if (_rc_debug)
fprintf(stderr, "*** rpmrc_free(%p[%s])\n", s, lbl(s));
   _PyObject_GC_Del(s);
}

/** \ingroup py_c
 */
static PyObject * rpmrc_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * ns = PyType_GenericAlloc(subtype, nitems);

if (_rc_debug)
fprintf(stderr, "*** rpmrc_alloc(%p[%s},%d) ret %p[%s]\n", subtype, lbl(subtype), nitems, ns, lbl(ns));
    return (PyObject *) ns;
}

/** \ingroup py_c
 */
/*@null@*/
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
#endif

/**
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmrc_methods[] = {
    { "addMacro",	(PyCFunction) rpmrc_AddMacro, METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "delMacro",	(PyCFunction) rpmrc_DelMacro, METH_VARARGS|METH_KEYWORDS,
	NULL },
#if Py_TPFLAGS_HAVE_ITER && PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 4
    { "next",		(PyCFunction) rpmrc_next,     METH_VARARGS,
	"next() -- get the next value, or raise StopIteration"},
#endif
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/** \ingroup py_c
 */
/*@-fullinitblock@*/
#if Py_TPFLAGS_HAVE_ITER
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
	rpmrc_traverse,			/* tp_traverse */
	rpmrc_clear,			/* tp_clear */
	rpmrc_richcompare,		/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	rpmrc_iter,			/* tp_iter */
	rpmrc_iternext,			/* tp_iternext */
	rpmrc_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	&PyDict_Type,			/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmrc_init,		/* tp_init */
	(allocfunc) rpmrc_alloc,	/* tp_alloc */
	(newfunc) rpmrc_new,		/* tp_new */
	(freefunc) rpmrc_free,		/* tp_free */
	0,				/* tp_is_gc */
};
#else
PyTypeObject rpmrc_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.rc",			/* tp_name */
	sizeof(rpmrcObject),		/* tp_size */
	0,				/* tp_itemsize */
	0,			 	/* tp_dealloc */
	0,				/* tp_print */
	0,			 	/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	0,				/* tp_flags */
	0				/* tp_doc */
};
#endif
/*@=fullinitblock@*/

#if Py_TPFLAGS_HAVE_ITER
PyObject * rpmrc_Create(/*@unused@*/ PyObject * self, PyObject *args, PyObject *kwds)
{
    return rpmrc_new(&rpmrc_Type, args, kwds);
}
#endif

/*@}*/
