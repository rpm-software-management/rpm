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

#include "debug.h"

static int _rpmbc_debug = 1;

/** \ingroup python
 */    
static PyObject * 
rpmbc_Debug(/*@unused@*/ rpmbcObject * s, PyObject * args)
        /*@globals _Py_NoneStruct @*/
        /*@modifies _Py_NoneStruct @*/ 
{
    if (!PyArg_ParseTuple(args, "i:Debug", &_rpmbc_debug)) return NULL;
 
if (_rpmbc_debug < 0) 
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

/* ---------- */

static rpmbcObject *
rpmbc_new(void)
{
    rpmbcObject * s = PyObject_New(rpmbcObject, &rpmbc_Type);
    if (s)
	mp32nzero(&s->n);
    return s;
}

static void
rpmbc_dealloc(rpmbcObject * s)
	/*@modifies s @*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_dealloc(%p)\n", s);

    mp32nfree(&s->n);
    PyMem_DEL(s);
}

static int
rpmbc_print(rpmbcObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies s, fp, fileSystem @*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_print(%p)\n", s);
    mp32print(fp, s->n.size, s->n.data);
    return 0;
}

static PyObject *
rpmbc_getattr(rpmbcObject * s, char * name)
	/*@*/
{
    return Py_FindMethod(rpmbc_methods, (PyObject *)s, name);
}

static int
rpmbc_compare(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_compare(%p,%p)\n", a, b);
    return 0;
}

static PyObject *
rpmbc_repr(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_repr(%p)\n", a);
    return NULL;
}

/* ---------- */

static PyObject *
rpmbc_add(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;
    uint32 carry;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_add(%p,%p)\n", a, b);

    if ((z = rpmbc_new()) != NULL) {
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

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_subtract(%p,%p)\n", a, b);

    if ((z = rpmbc_new()) != NULL) {
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

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_multiply(%p,%p)\n", a, b);

    if ((z = rpmbc_new()) != NULL) {
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

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_divide(%p,%p)\n", a, b);

    if (mp32z(b->n.size, b->n.data)) {
	PyErr_SetString(PyExc_ZeroDivisionError, "rpmbc_divide by zero");
	return NULL;
    }

    if ((z = rpmbc_new()) == NULL
     || (wksp = malloc(b->n.size+1)) == NULL) {
	Py_XDECREF(z);
	return NULL;
    }

    mp32nsize(&z->n, a->n.size+1);
    mp32ndivmod(z->n.data, a->n.size, a->n.data, b->n.size, b->n.data, wksp);

    return (PyObject *)z;
}

static PyObject *
rpmbc_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_remainder(%p,%p)\n", a, b);

    if ((z = rpmbc_new()) != NULL) {
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
    rpmbcObject * d = NULL;
    rpmbcObject * r = NULL;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_divmod(%p,%p)\n", a, b);

    if (mp32z(b->n.size, b->n.data)) {
	PyErr_SetString(PyExc_ZeroDivisionError, "rpmbc_divmod by zero");
	return NULL;
    }

    if ((z = PyTuple_New(2)) == NULL
     || (d = rpmbc_new()) == NULL
     || (r = rpmbc_new()) == NULL) {
	Py_XDECREF(z);
	Py_XDECREF(d);
	Py_XDECREF(r);
	return NULL;
    }

    mp32nsize(&d->n, a->n.size+1);
    mp32nsize(&r->n, b->n.size+1);

    mp32ndivmod(d->n.data, a->n.size, a->n.data, b->n.size, b->n.data, r->n.data);

    (void) PyTuple_SetItem(z, 0, (PyObject *)d);
    (void) PyTuple_SetItem(z, 1, (PyObject *)r);
    return (PyObject *)z;
}

static PyObject *
rpmbc_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_power(%p,%p,%p)\n", a, b, c);
    return NULL;
}

static PyObject *
rpmbc_negative(rpmbcObject * a)
	/*@*/
{
    rpmbcObject * z;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_negative(%p)\n", a);

    if ((z = rpmbc_new()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32neg(z->n.size, z->n.data);
    }
    return (PyObject *)z;
}

static PyObject *
rpmbc_positive(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_positive(%p)\n", a);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_absolute(rpmbcObject * a)
	/*@*/
{
    rpmbcObject * z;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_absolute(%p)\n", a);

    if (mp32msbset(a->n.size, a->n.data) == 0) {
	Py_INCREF(a);
	return (PyObject *)a;
    }

    if ((z = rpmbc_new()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32neg(z->n.size, z->n.data);
    }
    return (PyObject *)z;
}

static int
rpmbc_nonzero(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_nonzero(%p)\n", a);
    return mp32nz(a->n.size, a->n.data);
}
		
static PyObject *
rpmbc_invert(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_invert(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 count = 0;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_lshift(%p,%p)\n", a, b);

    mp32lshift(a->n.size, a->n.data, count);

    return NULL;
}

static PyObject *
rpmbc_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 count = 0;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_rshift(%p,%p)\n", a, b);

    mp32rshift(a->n.size, a->n.data, count);

    return NULL;
}

static PyObject *
rpmbc_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_and(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_xor(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_xor(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_or(%p,%p)\n", a, b);
    return NULL;
}

static int
rpmbc_coerce(PyObject ** ap, PyObject ** bp)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_coerce(%p,%p)\n", ap, bp);
    return -1;
}

static PyObject *
rpmbc_int(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_int(%p)\n", a);

    if (mp32size(a->n.size, a->n.data) > 1) {
	PyErr_SetString(PyExc_ValueError, "rpmbc_int() arg too long to convert");
	return NULL;
    }
    return NULL;
}

static PyObject *
rpmbc_long(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_long(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_float(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_float(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_oct(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_oct(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_hex(rpmbcObject * a)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_hex(%p)\n", a);
    return NULL;
}

static PyObject *
rpmbc_inplace_add(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 carry;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_add(%p,%p)\n", a, b);

    carry = mp32addx(a->n.size, a->n.data, b->n.size, b->n.data);

    return NULL;
}

static PyObject *
rpmbc_inplace_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 carry;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_subtract(%p,%p)\n", a, b);

    carry = mp32subx(a->n.size, a->n.data, b->n.size, b->n.data);

    return NULL;
}

static PyObject *
rpmbc_inplace_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 * result = NULL;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_multiply(%p,%p)\n", a, b);

    mp32mul(result, a->n.size, a->n.data, b->n.size, b->n.data);

    return NULL;
}

static PyObject *
rpmbc_inplace_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_remainder(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_power(%p,%p,%p)\n", a, b, c);
    return NULL;
}

static PyObject *
rpmbc_inplace_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 count = 0;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_lshift(%p,%p)\n", a, b);

    mp32lshift(a->n.size, a->n.data, count);

    return NULL;
}

static PyObject *
rpmbc_inplace_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 count = 0;

if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_rshift(%p,%p)\n", a, b);

    mp32rshift(a->n.size, a->n.data, count);

    return NULL;
}

static PyObject *
rpmbc_inplace_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_and(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_xor(rpmbcObject * a, rpmbcObject * b)
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_xor(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_or(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_floor_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_floor_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_true_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_true_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_floor_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
fprintf(stderr, "*** rpmbc_inplace_floor_divide(%p,%p)\n", a, b);
    return NULL;
}

static PyObject *
rpmbc_inplace_true_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
if (_rpmbc_debug)
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

#ifdef	NOTYET
static int
rpmbc_length(rpmbcObject * s)
	/*@*/
{
    return rpmbcCount(s->ds);
}

static PyObject *
rpmbc_subscript(rpmbcObject * s, PyObject * key)
	/*@modifies s @*/
{
    int ix;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    ix = (int) PyInt_AsLong(key);
    rpmbcSetIx(s->ds, ix);
    return Py_BuildValue("s", rpmbcDNEVR(s->ds));
}

static PyMappingMethods rpmbc_as_mapping = {
        (inquiry) rpmbc_length,		/* mp_length */
        (binaryfunc) rpmbc_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};
#endif

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
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

/* ---------- */
