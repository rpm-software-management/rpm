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

#include "rpmdebug-py.c"

#include "debug.h"

/*@unchecked@*/
static int _bc_debug = 1;

#define is_rpmbc(o)	((o)->ob_type == &rpmbc_Type)

#ifdef	NOTYET
static const char initialiser_name[] = "rpmbc";

static PyObject *
rpmbc_format(rpmbcObject * s, int base, unsigned char withname)
{
    PyStringObject * so;
    size_t i;
	int cmpres;
	int taglong;
	char *cp;
	char prefix[5];

    char * tcp = &prefix[0];

    if (s == NULL || !is_rpmbc(s)) {
	PyErr_BadInternalCall();
	return NULL;
    }

    assert(base >= 2 && base <= 36);

    if (withname)
	i = strlen(initialiser_name) + 2; /* e.g. 'rpmbc(' + ')' */
    else
	i = 0;

    if (mp32z(s->n.size, s->n.data))
	base = 10;	/* '0' in every base, right */
    else if (mp32msbset(s->n.size, s->n.data)) {
	*tcp++ = '-';
	i += 1;		/* space to hold '-' */
    }

	i += (int)mpz_sizeinbase(&s->mpz, base);

    if (base == 16) {
	*tcp++ = '0';
	*tcp++ = 'x';
	i += 2;		/* space to hold '0x' */
    } else if (base == 8) {
	*tcp++ = '0';
	i += 1;		/* space to hold the extra '0' */
    } else if (base > 10) {
	*tcp++ = '0' + base / 10;
	*tcp++ = '0' + base % 10;
	*tcp++ = '#';
	i += 3;		/* space to hold e.g. '12#' */
    } else if (base < 10) {
	*tcp++ = '0' + base;
	*tcp++ = '#';
	i += 2;		/* space to hold e.g. '6#' */
    }

	/*
	** the following code looks if we need a 'L' attached to the number
	** it will also attach an 'L' to the value -0x80000000
	*/
	taglong = 0;
	if (mpz_size(&s->mpz) > 1
	    || (long)mpz_get_ui(&s->mpz) < 0L) {
		taglong = 1;
		i += 1;		/* space to hold 'L' */
	}

    so = (PyStringObject *)PyString_FromStringAndSize((char *)0, i);
    if (so == NULL)
	return NULL;

    /* get the beginning of the string memory and start copying things */
    cp = PyString_AS_STRING(so);
    if (withname) {
	strcpy(cp, initialiser_name);
	cp += strlen(initialiser_name);
	*cp++ = '('; /*')'*/
    }

    /* copy the already prepared prefix; e.g. sign and base indicator */
    *tcp = '\0';
    strcpy(cp, prefix);
    cp += tcp - prefix;

	/* since' we have the sign already, let the lib think it's a positive
	   number */
	if (cmpres < 0)
		mpz_neg(&s->mpz,&s->mpz);	/* hack Hack HAck HACk HACK */
	(void)mpz_get_str(cp, base, &s->mpz);
	if (cmpres < 0)
		mpz_neg(&s->mpz,&s->mpz);	/* hack Hack HAck HACk HACK */
    cp += strlen(cp);

    if (taglong)
	*cp++ = 'L';
    if (withname)
	*cp++ = /*'('*/ ')';

    *cp = '\0';

    assert(cp - PyString_AS_STRING(so) <= i);

    if (cp - PyString_AS_STRING(so) != i)
	so->ob_size -= i - (cp - PyString_AS_STRING(so));

    return (PyObject *)so;
}
#endif

/* ---------- */

static void
rpmbc_dealloc(rpmbcObject * s)
	/*@modifies s @*/
{
if (_bc_debug < 0)
fprintf(stderr, "*** rpmbc_dealloc(%p)\n", s);

    mp32nfree(&s->n);
    PyObject_Del(s);
}

static int
rpmbc_print(rpmbcObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies fp, fileSystem @*/
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
static int rpmbc_init(rpmbcObject * z, PyObject *args, PyObject *kwds)
	/*@modifies z @*/
{
    PyObject * o = NULL;
    uint32 words = 0;
    long l = 0;

    if (!PyArg_ParseTuple(args, "|O:Cvt", &o)) return -1;

    if (o == NULL) {
	if (z->n.data == NULL)
	    mp32nsetw(&z->n, 0);
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
	mp32nsethex(&z->n, hex);
    } else {
	PyErr_SetString(PyExc_TypeError, "non-numeric coercion failed (rpmbc_init)");
	return -1;
    }

    if (words > 0) {
	mp32nsize(&z->n, words);
	switch (words) {
	case 2:
/*@-shiftimplementation @*/
	    z->n.data[0] = (l >> 32) & 0xffffffff;
/*@=shiftimplementation @*/
	    z->n.data[1] = (l      ) & 0xffffffff;
	    break;
	case 1:
	    z->n.data[0] = (l      ) & 0xffffffff;
	    break;
	}
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_init(%p[%s],%p[%s],%p[%s]):\t", z, lbl(z), args, lbl(args), kwds, lbl(kwds)), mp32println(stderr, z->n.size, z->n.data);

    return 0;
}

/** \ingroup python
 */
static void rpmbc_free(/*@only@*/ rpmbcObject * s)
	/*@modifies s @*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_free(%p[%s])\n", s, lbl(s));
    mp32nfree(&s->n);
    PyObject_Del(s);
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

    if (ns != NULL)
	mp32nzero(&((rpmbcObject *)ns)->n);

if (_bc_debug < 0)
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
        /*@*/ 
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
        /*@*/ 
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

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	carry = mp32addx(z->n.size, z->n.data, b->n.size, b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_add(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;
    uint32 carry;

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	carry = mp32subx(z->n.size, z->n.data, b->n.size, b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_subtract(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	/* XXX TODO: calculate zsize of result. */
	mp32nsize(&z->n, (a->n.size + b->n.size));
	mp32mul(z->n.data, a->n.size, a->n.data, b->n.size, b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_multiply(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z = NULL;
    uint32 * wksp;

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

    free(wksp);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_divide(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	uint32 * wksp = malloc(a->n.size+1);
	mp32nsize(&z->n, a->n.size);
	mp32nmod(z->n.data, a->n.size, a->n.data, b->n.size, b->n.data, wksp);
	free(wksp);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_remainder(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

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

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32neg(z->n.size, z->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_negative(%p):\t", a), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_positive(rpmbcObject * a)
	/*@*/
{
    Py_INCREF(a);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_positive(%p):\t", a), mp32println(stderr, a->n.size, a->n.data);

    return (PyObject *)a;
}

static PyObject *
rpmbc_absolute(rpmbcObject * a)
	/*@*/
{
    rpmbcObject * z;

    if (mp32msbset(a->n.size, a->n.data) == 0) {
	Py_INCREF(a);
	return (PyObject *)a;
    }

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32neg(z->n.size, z->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_absolute(%p):\t", a), mp32println(stderr, z->n.size, z->n.data);

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
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32not(z->n.size, z->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_invert(%p):\t", a), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;
    uint32 count = 0;

    /* XXX check shift count in range. */
    if ((z = rpmbc_New()) != NULL) {
	if (b->n.size == 1)
	    count = b->n.data[0];
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32lshift(z->n.size, z->n.data, count);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_lshift(%p[%s],%p[%s]):\t", a, lbl(a), b, lbl(b)), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;
    uint32 count = 0;

    /* XXX check shift count in range. */
    if ((z = rpmbc_New()) != NULL) {
	if (b->n.size == 1)
	    count = b->n.data[0];
	mp32ninit(&z->n, a->n.size, a->n.data);
	mp32rshift(z->n.size, z->n.data, count);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_rshift(%p[%s],%p[%s]):\t", a, lbl(a), b, lbl(b)), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	rpmbcObject * c, * d;

	if (a->n.size <= b->n.size) {
	    c = a;
	    d = b;
	} else {
	    c = b;
	    d = a;
	}
	mp32ninit(&z->n, c->n.size, c->n.data);
	mp32and(z->n.size, z->n.data, d->n.data + (d->n.size - c->n.size));
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_and(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_xor(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	rpmbcObject * c, * d;

	if (a->n.size <= b->n.size) {
	    c = a;
	    d = b;
	} else {
	    c = b;
	    d = a;
	}
	mp32ninit(&z->n, c->n.size, c->n.data);
	mp32xor(z->n.size, z->n.data, d->n.data + (d->n.size - c->n.size));
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_xor(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static PyObject *
rpmbc_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    rpmbcObject * z;

    if ((z = rpmbc_New()) != NULL) {
	rpmbcObject * c, * d;

	if (a->n.size <= b->n.size) {
	    c = a;
	    d = b;
	} else {
	    c = b;
	    d = a;
	}
	mp32ninit(&z->n, c->n.size, c->n.data);
	mp32or(z->n.size, z->n.data, d->n.data + (d->n.size - c->n.size));
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_or(%p,%p):\t", a, b), mp32println(stderr, z->n.size, z->n.data);

    return (PyObject *)z;
}

static int
rpmbc_coerce(PyObject ** pv, PyObject ** pw)
	/*@modifies *pv, *pw @*/
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
/*@-shiftimplementation @*/
	    z->n.data[0] = (l >> 32) & 0xffffffff;
/*@=shiftimplementation @*/
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
    if (mp32size(a->n.size, a->n.data) > 1) {
	PyErr_SetString(PyExc_ValueError, "rpmbc_int: arg too long to convert");
	return NULL;
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_int(%p):\t%08x\n", a, (int)(a->n.data ? a->n.data[0] : 0));

    return Py_BuildValue("i", (a->n.data ? a->n.data[0] : 0));
}

static PyObject *
rpmbc_long(rpmbcObject * a)
	/*@*/
{
    if (mp32size(a->n.size, a->n.data) > 1) {
	PyErr_SetString(PyExc_ValueError, "rpmbc_long() arg too long to convert");
	return NULL;
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_long(%p):\t%08lx\n", a, (long)(a->n.data ? a->n.data[0] : 0));

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
	/*@modifies a @*/
{
    uint32 carry;

    carry = mp32addx(a->n.size, a->n.data, b->n.size, b->n.data);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_add(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_subtract(rpmbcObject * a, rpmbcObject * b)
	/*@modifies a @*/
{
    uint32 carry;

    carry = mp32subx(a->n.size, a->n.data, b->n.size, b->n.data);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_subtract(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_multiply(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 * result = xmalloc((a->n.size + b->n.size) * sizeof(*result));

    mp32mul(result, a->n.size, a->n.data, b->n.size, b->n.data);

    free(a->n.data);
    a->n.size += b->n.size;
    a->n.data = result;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_multiply(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_divide(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 * result = xmalloc(a->n.size * sizeof(*result));
    uint32 * wksp = xmalloc(b->n.size+1);

    mp32ndivmod(result, a->n.size, a->n.data, b->n.size, b->n.data, wksp);

    free(wksp);
    free(a->n.data);
    a->n.data = result;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_divide(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_remainder(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    uint32 * result = xmalloc(a->n.size * sizeof(*result));
    uint32 * wksp = xmalloc(a->n.size+1);

    mp32nmod(result, a->n.size, a->n.data, b->n.size, b->n.data, wksp);

    free(wksp);
    free(a->n.data);
    a->n.data = result;

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_remainder(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_power(rpmbcObject * a, rpmbcObject * b, rpmbcObject * c)
	/*@*/
{
if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_power(%p,%p,%p):\t", a, b, c), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_lshift(rpmbcObject * a, rpmbcObject * b)
	/*@modifies a @*/
{
    uint32 count = 0;

    /* XXX check shift count in range. */
    if (b->n.size == 1)
	count = b->n.data[0];
    mp32lshift(a->n.size, a->n.data, count);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_lshift(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_rshift(rpmbcObject * a, rpmbcObject * b)
	/*@modifies a @*/
{
    uint32 count = 0;

    /* XXX check shift count in range. */
    if (b->n.size == 1)
	count = b->n.data[0];
    mp32rshift(a->n.size, a->n.data, count);

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_rshift(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_and(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    if (a->n.size <= b->n.size)
	mp32and(a->n.size, a->n.data, b->n.data + (b->n.size - a->n.size));
    else {
	memset(a->n.data, 0, (a->n.size - b->n.size) * sizeof(*a->n.data));
	mp32and(a->n.size, a->n.data + (a->n.size - b->n.size), b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_and(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_xor(rpmbcObject * a, rpmbcObject * b)
{
    if (a->n.size <= b->n.size)
	mp32xor(a->n.size, a->n.data, b->n.data + (b->n.size - a->n.size));
    else {
	memset(a->n.data, 0, (a->n.size - b->n.size) * sizeof(*a->n.data));
	mp32xor(a->n.size, a->n.data + (a->n.size - b->n.size), b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_xor(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
}

static PyObject *
rpmbc_inplace_or(rpmbcObject * a, rpmbcObject * b)
	/*@*/
{
    if (a->n.size <= b->n.size)
	mp32or(a->n.size, a->n.data, b->n.data + (b->n.size - a->n.size));
    else {
	memset(a->n.data, 0, (a->n.size - b->n.size) * sizeof(*a->n.data));
	mp32or(a->n.size, a->n.data + (a->n.size - b->n.size), b->n.data);
    }

if (_bc_debug)
fprintf(stderr, "*** rpmbc_inplace_or(%p,%p):\t", a, b), mp32println(stderr, a->n.size, a->n.data);

    Py_INCREF(a);
    return (PyObject *)a;
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
