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

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmbc_methods[] = {
 {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

static void
rpmbc_dealloc(rpmbcObject * s)
	/*@modifies s @*/
{
}

static int
rpmbc_print(rpmbcObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies s, fp, fileSystem @*/
{
    return -1;
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
    return 0;
}

static PyObject *
rpmbc_repr(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

/* ---------- */

static PyObject *
rpmbc_add(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_divmod(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_negative(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_positive(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_absolute(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static int
rpmbc_nonzero(rpmbcObject * a)
	/*@*/
{
    return 0;
}
		
static PyObject *
rpmbc_invert(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_xor(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static int
rpmbc_coerce(PyObject ** ap, PyObject ** bp)
	/*@*/
{
    return -1;
}

static PyObject *
rpmbc_int(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_long(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_float(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_oct(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_hex(rpmbcObject * a)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_add(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_xor(rpmbcObject * a, rpmbcObject * b)
{
    return NULL;
}

static PyObject *
rpmbc_inplace_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_floor_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_true_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_floor_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    return NULL;
}

static PyObject *
rpmbc_inplace_true_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
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
