/** \ingroup py_c
 * \file python/rpmfd-py.c
 */

#include "system.h"

#include <rpm/rpmio.h>	/* XXX fdGetFILE */

#include "header-py.h"	/* XXX pyrpmError */
#include "rpmfd-py.h"

#include "debug.h"


static int _rpmfd_debug = 1;

/** \ingroup python
 * \name Class: Rpmfd
 * \class Rpmfd
 * \brief An python rpm.fd object represents an rpm I/O handle.
 */

static PyObject *
rpmfd_Debug(rpmfdObject * s, PyObject * args, PyObject * kwds)
{
    char * kwlist[] = {"debugLevel", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &_rpmfd_debug))
	return NULL;

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
    char * note;
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
    if (node) {
	if (last)
	    last->next = node->next;
	else
	    fdhead = node->next;
	node->note = _free (node->note);
	node->fd = fdLink(node->fd, RPMDBG_M("closeCallback"));
	(void) Fclose (node->fd);
	while (node->fd)
	    node->fd = fdFree(node->fd, RPMDBG_M("closeCallback"));
	node = _free (node);
    }
    return 0;
}

/**
 */
static PyObject *
rpmfd_Fopen(PyObject * s, PyObject * args, PyObject * kwds)
{
    char * path;
    char * mode = "r.ufdio";
    FDlist *node;
    char * kwlist[] = {"path", "mode", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|s", kwlist, &path, &mode))
	return NULL;

    node = xmalloc (sizeof(FDlist));

    node->fd = Fopen(path, mode);
    node->fd = fdLink(node->fd, RPMDBG_M("doFopen"));
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

    node->f = fdGetFILE(node->fd);
    if (!node->f) {
	PyErr_SetString(pyrpmError, "FD_t has no FILE*");
	free(node);
	return NULL;
    }

    node->next = NULL;
    if (!fdhead) {
	fdhead = fdtail = node;
    } else if (fdtail) {
	fdtail->next = node;
    } else {
	fdhead = node;
    }
    fdtail = node;

    return PyFile_FromFile (node->f, path, mode, closeCallback);
}

/** \ingroup py_c
 */
static struct PyMethodDef rpmfd_methods[] = {
    {"Debug",	(PyCFunction)rpmfd_Debug,	METH_VARARGS|METH_KEYWORDS,
	NULL},
    {"Fopen",	(PyCFunction)rpmfd_Fopen,	METH_VARARGS|METH_KEYWORDS,
	NULL},
    {NULL,		NULL}		/* sentinel */
};

/* ---------- */

/** \ingroup py_c
 */
static void
rpmfd_dealloc(rpmfdObject * s)
{
    if (s) {
	Fclose(s->fd);
	s->fd = NULL;
	PyObject_Del(s);
    }
}

static PyObject * rpmfd_getattro(PyObject * o, PyObject * n)
{
    return PyObject_GenericGetAttr(o, n);
}

static int rpmfd_setattro(PyObject * o, PyObject * n, PyObject * v)
{
    return PyObject_GenericSetAttr(o, n, v);
}

/** \ingroup py_c
 */
static int rpmfd_init(rpmfdObject * s, PyObject *args, PyObject *kwds)
{
    char * path;
    char * mode = "r.ufdio";
    char * kwlist[] = {"path", "mode", NULL};

if (_rpmfd_debug)
fprintf(stderr, "*** rpmfd_init(%p,%p,%p)\n", s, args, kwds);

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|s:rpmfd_init", kwlist,
	    &path, &mode))
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
static void rpmfd_free(rpmfdObject * s)
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
{
    PyObject * s = PyType_GenericAlloc(subtype, nitems);

if (_rpmfd_debug)
fprintf(stderr, "*** rpmfd_alloc(%p,%d) ret %p\n", subtype, nitems, s);
    return s;
}

/** \ingroup py_c
 */
static rpmfdObject * rpmfd_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
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
static char rpmfd_doc[] =
"";

/** \ingroup py_c
 */
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
	(freefunc) rpmfd_free,		/* tp_free */
	0,				/* tp_is_gc */
};

rpmfdObject * rpmfd_Wrap(FD_t fd)
{
    rpmfdObject *s = PyObject_New(rpmfdObject, &rpmfd_Type);
    if (s == NULL)
	return NULL;
    s->fd = fd;
    return s;
}
