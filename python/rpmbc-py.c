/** \ingroup python
 * \file python/rpmbc-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "rpmbc-py.h"

#if Py_TPFLAGS_HAVE_ITER        /* XXX backport to python-1.5.2 */
/*@unchecked@*/
extern PyTypeObject PyCode_Type;
/*@unchecked@*/
extern PyTypeObject PyDictIter_Type;
/*@unchecked@*/
extern PyTypeObject PyFrame_Type;

#include <rpmcli.h>	/* XXX debug only */

#include "header-py.h"  /* XXX debug only */
#include "rpmal-py.h"   /* XXX debug only */
#include "rpmds-py.h"   /* XXX debug only */
#include "rpmfd-py.h"   /* XXX debug only */
#include "rpmfi-py.h"   /* XXX debug only */
#include "rpmmi-py.h"   /* XXX debug only */
#include "rpmrc-py.h"   /* XXX debug only */
#include "rpmte-py.h"   /* XXX debug only */
#include "rpmts-py.h"   /* XXX debug only */
#endif

#include "debug.h"

/*@unchecked@*/
static int _bc_debug = 1;

#define is_rpmbc(o)	((o)->ob_type == &rpmbc_Type)

/**
 */
static const char * lbl(void * s)
	/*@*/
{
    PyObject * o = s;

    if (o == NULL)	return "null";

    if (o->ob_type == &PyType_Type)	return o->ob_type->tp_name;

    if (o->ob_type == &PyBuffer_Type)	return "Buffer";
    if (o->ob_type == &PyCFunction_Type)	return "CFunction";
    if (o->ob_type == &PyCObject_Type)	return "CObject";
    if (o->ob_type == &PyCell_Type)	return "Cell";
    if (o->ob_type == &PyClass_Type)	return "Class";
    if (o->ob_type == &PyCode_Type)	return "Code";
    if (o->ob_type == &PyComplex_Type)	return "Complex";
    if (o->ob_type == &PyDict_Type)	return "Dict";
    if (o->ob_type == &PyDictIter_Type)	return "DictIter";
    if (o->ob_type == &PyFile_Type)	return "File";
    if (o->ob_type == &PyFloat_Type)	return "Float";
    if (o->ob_type == &PyFrame_Type)	return "Frame";
    if (o->ob_type == &PyFunction_Type)	return "Function";
    if (o->ob_type == &PyInstance_Type)	return "Instance";
    if (o->ob_type == &PyInt_Type)	return "Int";
    if (o->ob_type == &PyList_Type)	return "List";
    if (o->ob_type == &PyLong_Type)	return "Long";
    if (o->ob_type == &PyMethod_Type)	return "Method";
    if (o->ob_type == &PyModule_Type)	return "Module";
    if (o->ob_type == &PyRange_Type)	return "Range";
    if (o->ob_type == &PySeqIter_Type)	return "SeqIter";
    if (o->ob_type == &PySlice_Type)	return "Slice";
    if (o->ob_type == &PyString_Type)	return "String";
    if (o->ob_type == &PyTuple_Type)	return "Tuple";
    if (o->ob_type == &PyType_Type)	return "Type";
    if (o->ob_type == &PyUnicode_Type)	return "Unicode";

    if (o->ob_type == &hdr_Type)	return "hdr";
    if (o->ob_type == &rpmal_Type)	return "rpmal";
    if (o->ob_type == &rpmbc_Type)	return "rpmbc";
    if (o->ob_type == &rpmds_Type)	return "rpmds";
    if (o->ob_type == &rpmfd_Type)	return "rpmfd";
    if (o->ob_type == &rpmfi_Type)	return "rpmfi";
    if (o->ob_type == &rpmmi_Type)	return "rpmmi";
    if (o->ob_type == &rpmrc_Type)	return "rpmrc";
    if (o->ob_type == &rpmte_Type)	return "rpmte";
    if (o->ob_type == &rpmts_Type)	return "rpmts";

    return "Unknown";
}

/* ---------- */

static void
rpmbc_dealloc(rpmbcObject * s)
	/*@modifies s @*/
{
if (_bc_debug < 0)
fprintf(stderr, "*** rpmbc_dealloc(%p)\n", s);

    mp32nfree(&s->n);
    PyObject_DEL(s);
}

static int
rpmbc_print(rpmbcObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies s, fp, fileSystem @*/
{
if (_bc_debug < 0)
fprintf(stderr, "*** rpmbc_print(%p)\n", s);
    mp32print(fp, s->n.size, s->n.data);
    return 0;
}

static int
rpmbc_compare(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_compare(%p,%p)\n", a, b);
    return 0;
}

static PyObject *
rpmbc_repr(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_repr(%p)\n", a);
    return NULL;
}

/** \ingroup python
 */
static int rpmbc_init(rpmbcObject * s, PyObject *args, PyObject *kwds)
	/*@*/
{
    PyObject * o = NULL;
    uint32 words = 0;
    long l = 0;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_init(%p[%s],%p[%s],%p[%s])\n", s, lbl(s), args, lbl(args), kwds, lbl(kwds));

    if (!PyArg_ParseTuple(args, "|O:Cvt", &o)) return -1;

    if (o == NULL) {
	if (s->n.data == NULL)
	    mp32nsetw(&s->n, 0);
    } else if (PyInt_Check(o)) {
	l = PyInt_AsLong(o);
	words = sizeof(l)/sizeof(words);
    } else if (PyLong_Check(o)) {
	l = PyLong_AsLong(o);
	words = sizeof(l)/sizeof(words);
    } else if (PyFloat_Check(o)) {
	double d = PyFloat_AsDouble(o);
	/* XXX TODO: check for overflow/underflow. */
	l = (long) (d + 0.5);
	words = sizeof(l)/sizeof(words);
    } else if (PyString_Check(o)) {
	const unsigned char * hex = PyString_AsString(o);
	/* XXX TODO: check for hex. */
	mp32nsethex(&s->n, hex);
    } else {
	PyErr_SetString(PyExc_TypeError, "non-numeric coercion failed (rpmbc_init)");
	return -1;
    }

    if (words > 0) {
	mp32nsize(&s->n, words);
	switch (words) {
	case 2:
	    s->n.data[0] = (l >> 32) & 0xffffffff;
	    s->n.data[1] = (l      ) & 0xffffffff;
	    break;
	case 1:
	    s->n.data[0] = (l      ) & 0xffffffff;
	    break;
	}
    }

    return 0;
}

/** \ingroup python
 */
static void rpmbc_free(rpmbcObject * s)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_free(%p[%s])\n", s, lbl(s));
    mp32nfree(&s->n);
   _PyObject_GC_Del((PyObject *)s);
}

/** \ingroup python
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

    mp32nzero(&((rpmbcObject *)ns)->n);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_new(%p[%s],%p[%s],%p[%s]) ret %p[%s]\n", subtype, lbl(subtype), args, lbl(args), kwds, lbl(kwds), ns, lbl(ns));
    return ns;
}

static rpmbcObject *
rpmbc_New(void)
{
    rpmbcObject * ns = PyObject_New(rpmbcObject, &rpmbc_Type);

    mp32nzero(&ns->n);
    return ns;
}

static rpmbcObject *
rpmbc_i2bc(PyObject * o)
{
    if (is_rpmbc(o)) {
	Py_INCREF(o);
	return (rpmbcObject *)o;
    }
    if (PyInt_Check(o) || PyLong_Check(o)) {
	rpmbcObject * ns = PyObject_New(rpmbcObject, &rpmbc_Type);
	PyObject * args = PyTuple_New(1);

	mp32nzero(&((rpmbcObject *)ns)->n);
	(void) PyTuple_SetItem(args, 0, o);
	rpmbc_init(ns, args, NULL);
	Py_DECREF(args);
	return ns;
    }

    PyErr_SetString(PyExc_TypeError, "number coercion (to rpmbcObject) failed");
    return NULL;
}

/* ---------- */

/** \ingroup python
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

/** \ingroup python
 */    
static PyObject * 
rpmbc_Gcd(/*@unused@*/ rpmbcObject * s, PyObject * args)
        /*@globals _Py_NoneStruct @*/
        /*@modifies _Py_NoneStruct @*/ 
{
    PyObject * op1;
    PyObject * op2;
    rpmbcObject * a = NULL;
    rpmbcObject * b = NULL;
    rpmbcObject * z = NULL;
    uint32 * wksp = NULL;

if (_bc_debug < 0) 
fprintf(stderr, "*** rpmbc_Gcd(%p)\n", s);
 
    if (!PyArg_ParseTuple(args, "OO:Gcd", &op1, &op2)) return NULL;

    if ((a = rpmbc_i2bc(op1)) != NULL
     && (b = rpmbc_i2bc(op2)) != NULL
     && (wksp = malloc(a->n.size)) != NULL
     && (z = rpmbc_New()) != NULL) {
	mp32nsize(&z->n, a->n.size);
	mp32gcd_w(a->n.size, a->n.data, b->n.data, z->n.data, wksp);
    }

    if (wksp != NULL) free(wksp);
    Py_DECREF(a);
    Py_DECREF(b);

    return (PyObject *)z;
}

/** \ingroup python
 */    
static PyObject * 
rpmbc_Sqrt(/*@unused@*/ rpmbcObject * s, PyObject * args)
        /*@globals _Py_NoneStruct @*/
        /*@modifies _Py_NoneStruct @*/ 
{
    PyObject * op1;
    rpmbcObject * a = NULL;
    rpmbcObject * z = NULL;

if (_bc_debug < 0) 
fprintf(stderr, "*** rpmbc_Sqrt(%p)\n", s);
 
    if (!PyArg_ParseTuple(args, "O:Sqrt", &op1)) return NULL;

    if ((a = rpmbc_i2bc(op1)) != NULL
     && (z = rpmbc_New()) != NULL) {
	mp32nsize(&z->n, a->n.size);
	mp32sqr(z->n.data, a->n.size, a->n.data);
    }

    Py_DECREF(a);

    return (PyObject *)z;
}

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmbc_methods[] = {
 {"Debug",	(PyCFunction)rpmbc_Debug,	METH_VARARGS,
	NULL},
 {"gcd",	(PyCFunction)rpmbc_Gcd,		METH_VARARGS,
	NULL},
 {"sqrt",	(PyCFunction)rpmbc_Sqrt,	METH_VARARGS,
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

static PyObject *
rpmbc_add(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;
    uint32 carry;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_add(%p,%p)\n", a, b);

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	carry = mp32addx(z->n.size, z->n.data, b->n.size, b->n.data);
    }
    return (PyObject *)z;
}

static PyObject *
rpmbc_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;
    uint32 carry;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_subtract(%p,%p)\n", a, b);

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	carry = mp32subx(z->n.size, z->n.data, b->n.size, b->n.data);
    }
    return (PyObject *)z;
}

static PyObject *
rpmbc_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_multiply(%p,%p)\n", a, b);

    if ((z = rpmbc_New()) != NULL) {
	/* XXX TODO: calculate zsize of result. */
	mp32nsize(&z->n, (a->n.size + b->n.size));
	mp32mul(z->n.data, a->n.size, a->n.data, b->n.size, b->n.data);
    }
    return (PyObject *)z;
}

static PyObject *
rpmbc_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z = NULL;
    uint32 * wksp;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_divide(%p,%p)\n", a, b);

    if (mp32z(b->n.size, b->n.data)) {
	PyErr_SetString(PyExc_ZeroDivisionError, "rpmbc_divide by zero");
	return NULL;
    }

    if ((z = rpmbc_New()) == NULL
     || (wksp = malloc(b->n.size+1)) == NULL) {
	Py_XDECREF(z);
	return NULL;
    }

    mp32nsize(&z->n, a->n.size);
    mp32ndivmod(z->n.data, a->n.size, a->n.data, b->n.size, b->n.data, wksp);

    return (PyObject *)z;
}

static PyObject *
rpmbc_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_remainder(%p,%p)\n", a, b);

    if ((z = rpmbc_New()) != NULL) {
	uint32 * wksp = malloc(a->n.size+1);
	mp32nsize(&z->n, a->n.size);
	mp32nmod(z->n.data, a->n.size, a->n.data, b->n.size, b->n.data, wksp);
	free(wksp);
    }
    return (PyObject *)z;
}

static PyObject *
rpmbc_divmod(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    PyObject * z = NULL;
    rpmbcObject * q = NULL;
    rpmbcObject * r = NULL;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_divmod(%p,%p)\n", a, b);

    if (mp32z(b->n.size, b->n.data)) {
	PyErr_SetString(PyExc_ZeroDivisionError, "rpmbc_divmod by zero");
	return NULL;
    }

    if ((z = PyTuple_New(2)) == NULL
     || (q = rpmbc_New()) == NULL
     || (r = rpmbc_New()) == NULL) {
	Py_XDECREF(z);
	Py_XDECREF(q);
	Py_XDECREF(r);
	return NULL;
    }

    mp32nsize(&q->n, a->n.size+1);
    mp32nsize(&r->n, b->n.size+1);

    mp32ndivmod(q->n.data, a->n.size, a->n.data, b->n.size, b->n.data, r->n.data);

    (void) PyTuple_SetItem(z, 0, (PyObject *)q);
    (void) PyTuple_SetItem(z, 1, (PyObject *)r);
    return (PyObject *)z;
}

static PyObject *
rpmbc_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_power(%p,%p,%p)\n", a, b, c);
    return NULL;
}

static PyObject *
rpmbc_negative(rpmbcObject * a)
	/*@*/
{
    rpmbcObject * z;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_negative(%p)\n", a);

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32neg(z->n.size, z->n.data);
    }
    return (PyObject *)z;
}

static PyObject *
rpmbc_positive(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_positive(%p)\n", a);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_absolute(rpmbcObject * a)
	/*@*/
{
    rpmbcObject * z;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_absolute(%p)\n", a);

    if (mp32msbset(a->n.size, a->n.data) == 0) {
	Py_INCREF(a);
	return (PyObject *)a;
    }

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32neg(z->n.size, z->n.data);
    }
    return (PyObject *)z;
}

static int
rpmbc_nonzero(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_nonzero(%p)\n", a);
    return mp32nz(a->n.size, a->n.data);
}
		
static PyObject *
rpmbc_invert(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_invert(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 count = 0;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_lshift(%p,%p)\n", a, b);

    mp32lshift(a->n.size, a->n.data, count);

    return NULL;
}

static PyObject *
rpmbc_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 count = 0;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_rshift(%p,%p)\n", a, b);

    mp32rshift(a->n.size, a->n.data, count);

    return NULL;
}

static PyObject *
rpmbc_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_and(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_xor(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_xor(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_or(%p,%p)\n", a, b);
    return NULL;
}

static int
rpmbc_coerce(PyObject ** pv, PyObject ** pw)
	/*@*/
{
    uint32 words = 0;
    long l = 0;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_coerce(%p[%s],%p[%s])\n", pv, lbl(*pv), pw, lbl(*pw));

    if (is_rpmbc(*pw)) {
	Py_INCREF(*pw);
    } else if (PyInt_Check(*pw)) {
	l = PyInt_AsLong(*pw);
	words = sizeof(l)/sizeof(words);
    } else if (PyLong_Check(*pw)) {
	l = PyLong_AsLong(*pw);
	words = sizeof(l)/sizeof(words);
    } else if (PyFloat_Check(*pw)) {
	double d = PyFloat_AsDouble(*pw);
	/* XXX TODO: check for overflow/underflow. */
	l = (long) (d + 0.5);
	words = sizeof(l)/sizeof(words);
    } else {
	PyErr_SetString(PyExc_TypeError, "non-numeric coercion failed (rpmbc_coerce)");
	return 1;
    }

    if (words > 0) {
	rpmbcObject * z = rpmbc_New();
	mp32nsize(&z->n, words);
	switch (words) {
	case 2:
	    z->n.data[0] = (l >> 32) & 0xffffffff;
	    z->n.data[1] = (l      ) & 0xffffffff;
	    break;
	case 1:
	    z->n.data[0] = (l      ) & 0xffffffff;
	    break;
	}
	(*pw) = (PyObject *) z;
    }

    Py_INCREF(*pv);
    return 0;
}

static PyObject *
rpmbc_int(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_int(%p)\n", a);

    if (mp32size(a->n.size, a->n.data) > 1) {
	PyErr_SetString(PyExc_ValueError, "rpmbc_int: arg too long to convert");
	return NULL;
    }
    return Py_BuildValue("i", (a->n.data ? a->n.data[0] : 0));
}

static PyObject *
rpmbc_long(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_long(%p)\n", a);
    if (mp32size(a->n.size, a->n.data) > 1) {
	PyErr_SetString(PyExc_ValueError, "rpmbc_int() arg too long to convert");
	return NULL;
    }
    return Py_BuildValue("l", (a->n.data ? a->n.data[0] : 0));
}

static PyObject *
rpmbc_float(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_float(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_oct(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_oct(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_hex(rpmbcObject * a)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_hex(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_inplace_add(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 carry;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_add(%p,%p)\n", a, b);

    carry = mp32addx(a->n.size, a->n.data, b->n.size, b->n.data);

    return NULL;
}

static PyObject *
rpmbc_inplace_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 carry;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_subtract(%p,%p)\n", a, b);

    carry = mp32subx(a->n.size, a->n.data, b->n.size, b->n.data);

    return NULL;
}

static PyObject *
rpmbc_inplace_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 * result = NULL;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_multiply(%p,%p)\n", a, b);

    mp32mul(result, a->n.size, a->n.data, b->n.size, b->n.data);

    return NULL;
}

static PyObject *
rpmbc_inplace_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_remainder(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_power(%p,%p,%p)\n", a, b, c);
    return NULL;
}

static PyObject *
rpmbc_inplace_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 count = 0;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_lshift(%p,%p)\n", a, b);

    mp32lshift(a->n.size, a->n.data, count);

    return NULL;
}

static PyObject *
rpmbc_inplace_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 count = 0;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_rshift(%p,%p)\n", a, b);

    mp32rshift(a->n.size, a->n.data, count);

    return NULL;
}

static PyObject *
rpmbc_inplace_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_and(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_xor(rpmbcObject * a, rpmbcObject * b)
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_xor(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_or(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_floor_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_floor_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_true_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_true_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_floor_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_floor_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_true_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_true_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyNumberMethods rpmbc_as_number = {
	(binaryfunc) rpmbc_add,			/* nb_add */
	(binaryfunc) rpmbc_subtract,		/* nb_subtract */
	(binaryfunc) rpmbc_multiply,		/* nb_multiply */
	(binaryfunc) rpmbc_divide,		/* nb_divide */
	(binaryfunc) rpmbc_remainder,		/* nb_remainder */
	(binaryfunc) rpmbc_divmod,		/* nb_divmod */
	(ternaryfunc) rpmbc_power,		/* nb_power */
	(unaryfunc) rpmbc_negative,		/* nb_negative */
	(unaryfunc) rpmbc_positive,		/* nb_positive */
	(unaryfunc) rpmbc_absolute,		/* nb_absolute */
	(inquiry) rpmbc_nonzero,		/* nb_nonzero */
	(unaryfunc) rpmbc_invert,		/* nb_invert */
	(binaryfunc) rpmbc_lshift,		/* nb_lshift */
	(binaryfunc) rpmbc_rshift,		/* nb_rshift */
	(binaryfunc) rpmbc_and,			/* nb_and */
	(binaryfunc) rpmbc_xor,			/* nb_xor */
	(binaryfunc) rpmbc_or,			/* nb_or */
	(coercion) rpmbc_coerce,		/* nb_coerce */
	(unaryfunc) rpmbc_int,			/* nb_int */
	(unaryfunc) rpmbc_long,			/* nb_long */
	(unaryfunc) rpmbc_float,		/* nb_float */
	(unaryfunc) rpmbc_oct,			/* nb_oct */
	(unaryfunc) rpmbc_hex,			/* nb_hex */

	/* Added in release 2.0 */
	(binaryfunc) rpmbc_inplace_add,		/* nb_inplace_add */
	(binaryfunc) rpmbc_inplace_subtract,	/* nb_inplace_subtract */
	(binaryfunc) rpmbc_inplace_multiply,	/* nb_inplace_multiply */
	(binaryfunc) rpmbc_inplace_divide,	/* nb_inplace_divide */
	(binaryfunc) rpmbc_inplace_remainder,	/* nb_inplace_remainder */
	(ternaryfunc) rpmbc_inplace_power,	/* nb_inplace_power */
	(binaryfunc) rpmbc_inplace_lshift,	/* nb_inplace_lshift */
	(binaryfunc) rpmbc_inplace_rshift,	/* nb_inplace_rshift */
	(binaryfunc) rpmbc_inplace_and,		/* nb_inplace_and */
	(binaryfunc) rpmbc_inplace_xor,		/* nb_inplace_xor */
	(binaryfunc) rpmbc_inplace_or,		/* nb_inplace_or */

	/* Added in release 2.2 */
	/* The following require the Py_TPFLAGS_HAVE_CLASS flag */
	(binaryfunc) rpmbc_floor_divide,	/* nb_floor_divide */
	(binaryfunc) rpmbc_true_divide,		/* nb_true_divide */
	(binaryfunc) rpmbc_inplace_floor_divide,/* nb_inplace_floor_divide */
	(binaryfunc) rpmbc_inplace_true_divide	/* nb_inplace_true_divide */

};

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
	(cmpfunc) rpmbc_compare,	/* tp_compare */
	(reprfunc) rpmbc_repr,		/* tp_repr */
	&rpmbc_as_number,		/* tp_as_number */
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
