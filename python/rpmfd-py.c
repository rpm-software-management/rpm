/** \ingroup py_c
 * \file python/rpmfd-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include <glob.h>	/* XXX rpmio.h */
#include <dirent.h>	/* XXX rpmio.h */
#include <rpmio_internal.h>

#include <rpmlib.h>	/* XXX _free */

#include "header-py.h"	/* XXX pyrpmError */
#include "rpmfd-py.h"

#include "debug.h"

/*@access FD_t @*/

/*@unchecked@*/
static int _rpmfd_debug = 1;

/** \ingroup python
 * \name Class: Rpmfd
 * \class Rpmfd
 * \brief An python rpm.fd object represents an rpm I/O handle.
 */

/*@null@*/
static PyObject *
rpmfd_Debug(/*@unused@*/ rpmfdObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i", &_rpmfd_debug)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

/**
 */
typedef struct FDlist_t FDlist;

/**
 */
struct FDlist_t {
    FILE * f;
    FD_t fd;
    const char * note;
    FDlist * next;
} ;

/**
 */
static FDlist *fdhead = NULL;

/**
 */
static FDlist *fdtail = NULL;

/**
 */
static int closeCallback(FILE * f)
	/*@globals fdhead @*/
	/*@modifies fdhead @*/
{
    FDlist *node, *last;

    node = fdhead;
    last = NULL;
    while (node) {
	if (node->f == f)
	    break;
	last = node;
	node = node->next;
    }
/*@-branchstate@*/
    if (node) {
	if (last)
	    last->next = node->next;
	else
	    fdhead = node->next;
	node->note = _free (node->note);
	node->fd = fdLink(node->fd, "closeCallback");
	(void) Fclose (node->fd);
	while (node->fd)
	    node->fd = fdFree(node->fd, "closeCallback");
	node = _free (node);
    }
/*@=branchstate@*/
    return 0;
}

/**
 */
/*@null@*/
static PyObject *
rpmfd_Fopen(/*@unused@*/ PyObject * s, PyObject * args)
	/*@globals fdhead, fdtail @*/
	/*@modifies fdhead, fdtail @*/
{
    char * path;
    char * mode = "r.ufdio";
    FDlist *node;

    if (!PyArg_ParseTuple(args, "s|s", &path, &mode))
	return NULL;

    node = xmalloc (sizeof(FDlist));

    node->fd = Fopen(path, mode);
    node->fd = fdLink(node->fd, "doFopen");
    node->note = xstrdup (path);

    if (!node->fd) {
	(void) PyErr_SetFromErrno(pyrpmError);
	node = _free (node);
	return NULL;
    }

    if (Ferror(node->fd)) {
	const char *err = Fstrerror(node->fd);
	node = _free(node);
	if (err)
	    PyErr_SetString(pyrpmError, err);
	return NULL;
    }

    node->f = fdGetFp(node->fd);
    if (!node->f) {
	PyErr_SetString(pyrpmError, "FD_t has no FILE*");
	free(node);
	return NULL;
    }

    node->next = NULL;
/*@-branchstate@*/
    if (!fdhead) {
	fdhead = fdtail = node;
    } else if (fdtail) {
	fdtail->next = node;
    } else {
	fdhead = node;
    }
/*@=branchstate@*/
    fdtail = node;

    return PyFile_FromFile (node->f, path, mode, closeCallback);
}

/** \ingroup py_c
 */
/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmfd_methods[] = {
    {"Debug",	(PyCFunction)rpmfd_Debug,	METH_VARARGS,
	NULL},
    {"Fopen",	(PyCFunction)rpmfd_Fopen,	METH_VARARGS,
	NULL},
    {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

/** \ingroup py_c
 */
static void
rpmfd_dealloc(/*@only@*/ /*@null@*/ rpmfdObject * s)
	/*@modifies s @*/
{
    if (s) {
	Fclose(s->fd);
	s->fd = NULL;
	PyObject_Del(s);
    }
}

static PyObject * rpmfd_getattro(PyObject * o, PyObject * n)
	/*@*/
{
    return PyObject_GenericGetAttr(o, n);
}

static int rpmfd_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr(o, n, v);
}

/** \ingroup py_c
 */
static int rpmfd_init(rpmfdObject * s, PyObject *args, PyObject *kwds)
	/*@modifies s @*/
{
    char * path;
    char * mode = "r.ufdio";

if (_rpmfd_debug)
fprintf(stderr, "*** rpmfd_init(%p,%p,%p)\n", s, args, kwds);

    if (!PyArg_ParseTuple(args, "s|s:rpmfd_init", &path, &mode))
	return -1;

    s->fd = Fopen(path, mode);

    if (s->fd == NULL) {
	(void) PyErr_SetFromErrno(pyrpmError);
	return -1;
    }

    if (Ferror(s->fd)) {
	const char *err = Fstrerror(s->fd);
	if (s->fd)
	    Fclose(s->fd);
	if (err)
	    PyErr_SetString(pyrpmError, err);
	return -1;
    }
    return 0;
}

/** \ingroup py_c
 */
static void rpmfd_free(/*@only@*/ rpmfdObject * s)
	/*@modifies s @*/
{
if (_rpmfd_debug)
fprintf(stderr, "%p -- fd %p\n", s, s->fd);
    if (s->fd)
	Fclose(s->fd);

    PyObject_Del((PyObject *)s);
}

/** \ingroup py_c
 */
static PyObject * rpmfd_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * s = PyType_GenericAlloc(subtype, nitems);

if (_rpmfd_debug)
fprintf(stderr, "*** rpmfd_alloc(%p,%d) ret %p\n", subtype, nitems, s);
    return s;
}

/** \ingroup py_c
 */
/*@null@*/
static rpmfdObject * rpmfd_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@*/
{
    rpmfdObject * s = PyObject_New(rpmfdObject, subtype);

    /* Perform additional initialization. */
    if (rpmfd_init(s, args, kwds) < 0) {
	rpmfd_free(s);
	return NULL;
    }

if (_rpmfd_debug)
fprintf(stderr, "%p ++ fd %p\n", s, s->fd);

    return s;
}

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmfd_doc[] =
"";

/** \ingroup py_c
 */
/*@-fullinitblock@*/
PyTypeObject rpmfd_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.fd",			/* tp_name */
	sizeof(rpmfdObject),		/* tp_size */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmfd_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	(getattrofunc) rpmfd_getattro,	/* tp_getattro */
	(setattrofunc) rpmfd_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmfd_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmfd_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmfd_init,		/* tp_init */
	(allocfunc) rpmfd_alloc,	/* tp_alloc */
	(newfunc) rpmfd_new,		/* tp_new */
	(destructor) rpmfd_free,	/* tp_free */
	0,				/* tp_is_gc */
#endif
};
/*@=fullinitblock@*/

rpmfdObject * rpmfd_Wrap(FD_t fd)
{
    rpmfdObject *s = PyObject_New(rpmfdObject, &rpmfd_Type);
    if (s == NULL)
	return NULL;
    s->fd = fd;
    return s;
}
