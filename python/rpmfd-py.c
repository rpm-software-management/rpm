/** \ingroup python
 * \file python/rpmfd-py.c
 */

#include "system.h"

#include "Python.h"

#include <glob.h>	/* XXX rpmio.h */
#include <dirent.h>	/* XXX rpmio.h */
#include <rpmio_internal.h>
#include <rpmlib.h>	/* XXX _free */

#include "header-py.h"	/* XXX pyrpmError */
#include "rpmfd-py.h"

#include "debug.h"

extern int _rpmio_debug;

/** \ingroup python
 * \name Class: rpm.fd
 * \class rpm.fd
 * \brief An python rpm.fd object represents an rpm I/O handle.
 */

static PyObject *
rpmfd_Debug(rpmfdObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "i", &_rpmio_debug)) return NULL;
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
        node->fd = fdLink(node->fd, "closeCallback");
        Fclose (node->fd);
        while (node->fd)
            node->fd = fdFree(node->fd, "closeCallback");
        node = _free (node);
    }
    return 0; 
}

/**
 */
static PyObject *
rpmfd_Fopen(PyObject * self, PyObject * args)
{
    char * path, * mode;
    FDlist *node;
    
    if (!PyArg_ParseTuple(args, "ss", &path, &mode))
	return NULL;
    
    node = xmalloc (sizeof(FDlist));
    
    node->fd = Fopen(path, mode);
    node->fd = fdLink(node->fd, "doFopen");
    node->note = xstrdup (path);

    if (!node->fd) {
	PyErr_SetFromErrno(pyrpmError);
        node = _free (node);
	return NULL;
    }
    
    if (Ferror(node->fd)) {
	const char *err = Fstrerror(node->fd);
        node = _free(node);
	if (err) {
	    PyErr_SetString(pyrpmError, err);
	    return NULL;
	}
    }
    node->f = fdGetFp(node->fd);
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

/** \ingroup python
 */
static struct PyMethodDef rpmfd_methods[] = {
    {"Debug",	(PyCFunction)rpmfd_Debug,	METH_VARARGS,
        NULL},
    {"Fopen",	(PyCFunction)rpmfd_Fopen,	METH_VARARGS,
        NULL},
    {NULL,		NULL}		/* sentinel */
};

/* ---------- */

/** \ingroup python
 */
static PyObject * rpmfd_getattr(rpmfdObject * o, char * name)
{
    return Py_FindMethod(rpmfd_methods, (PyObject *) o, name);
}

/**
 */
static char rpmfd_doc[] =
"";

/** \ingroup python
 */
PyTypeObject rpmfd_Type = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"rpm.fd",			/* tp_name */
	sizeof(rpmfdObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor)0,		 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmfd_getattr, 	/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
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
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
#endif
};

rpmfdObject * rpmfd_Wrap(FD_t fd)
{
    rpmfdObject *s = PyObject_NEW(rpmfdObject, &rpmfd_Type);
    if (s == NULL)
	return NULL;
    s->fd = fd;
    return s;
}
