
#include "rpmsystem-py.h"
#include <rpm/rpmstring.h>
#include "header-py.h"	/* XXX for utf8FromPyObject() only */
#include "rpmfd-py.h"

struct rpmfdObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    FD_t fd;
    char *mode;
    char *flags;
};

FD_t rpmfdGetFd(rpmfdObject *fdo)
{
    return fdo->fd;
}

int rpmfdFromPyObject(PyObject *obj, rpmfdObject **fdop)
{
    rpmfdObject *fdo = NULL;

    if (rpmfdObject_Check(obj)) {
	Py_INCREF(obj);
	fdo = (rpmfdObject *) obj;
    } else {
	fdo = (rpmfdObject *) PyObject_CallFunctionObjArgs((PyObject *)&rpmfd_Type,
                                                           obj, NULL);
    }
    if (fdo == NULL) return 0;

    if (Ferror(fdo->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(fdo->fd));
	Py_DECREF(fdo);
	return 0;
    }
    *fdop = fdo;
    return 1;
}

static PyObject *err_closed(void)
{
    PyErr_SetString(PyExc_ValueError, "I/O operation on closed file");
    return NULL;
}

static FD_t openPath(const char *path, const char *mode)
{
    FD_t fd;
    Py_BEGIN_ALLOW_THREADS
    fd = Fopen(path, mode);
    Py_END_ALLOW_THREADS;
    return fd;
}

static FD_t openFd(FD_t ofd, const char *mode)
{
    FD_t fd;
    Py_BEGIN_ALLOW_THREADS
    fd = Fdopen(ofd, mode);
    Py_END_ALLOW_THREADS;
    return fd;
}

static int rpmfd_init(rpmfdObject *s, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "obj", "mode", "flags", NULL };
    const char *mode = "r";
    const char *flags = "ufdio";
    char *rpmio_mode = NULL;
    PyObject *fo = NULL;
    FD_t fd = NULL;
    int fdno;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|ss", kwlist, 
				     &fo, &mode, &flags))
	return -1;

    rpmio_mode = rstrscat(NULL, mode, ".", flags, NULL);

    if (PyBytes_Check(fo)) {
	fd = openPath(PyBytes_AsString(fo), rpmio_mode);
    } else if (PyUnicode_Check(fo)) {
	PyObject *enc = NULL;
	int rc = PyUnicode_FSConverter(fo, &enc);
	if (rc) {
	    fd = openPath(PyBytes_AsString(enc), rpmio_mode);
	    Py_DECREF(enc);
	}
    } else if (rpmfdObject_Check(fo)) {
	rpmfdObject *fdo = (rpmfdObject *)fo;
	fd = openFd(fdDup(Fileno(fdo->fd)), rpmio_mode);
    } else if ((fdno = PyObject_AsFileDescriptor(fo)) >= 0) {
	fd = openFd(fdDup(fdno), rpmio_mode);
    } else {
	PyErr_SetString(PyExc_TypeError, "path or file object expected");
    }

    if (fd != NULL) {
	Fclose(s->fd); /* in case __init__ was called again */
	free(s->mode);
	free(s->flags);
	s->fd = fd;
	s->mode = rstrdup(mode);
	s->flags = rstrdup(flags);
    } else {
	PyErr_SetString(PyExc_IOError, Fstrerror(fd));
    }

    free(rpmio_mode);
    return (fd == NULL) ? -1 : 0;
}

static PyObject *rpmfd_open(PyObject *cls, PyObject *args, PyObject *kwds)
{
    return PyObject_Call(cls, args, kwds);
}

static PyObject *do_close(rpmfdObject *s)
{
    /* mimic python fileobject: close on closed file is not an error */
    if (s->fd) {
	Py_BEGIN_ALLOW_THREADS
	Fclose(s->fd);
	Py_END_ALLOW_THREADS
	s->fd = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *rpmfd_close(rpmfdObject *s)
{
    return do_close(s);
}

static void rpmfd_dealloc(rpmfdObject *s)
{
    PyObject *res = do_close(s);
    Py_XDECREF(res);
    free(s->mode);
    free(s->flags);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject *rpmfd_fileno(rpmfdObject *s)
{
    int fno;
    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS
    fno = Fileno(s->fd);
    Py_END_ALLOW_THREADS

    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    return Py_BuildValue("i", fno);
}

static PyObject *rpmfd_flush(rpmfdObject *s)
{
    int rc;

    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS
    rc = Fflush(s->fd);
    Py_END_ALLOW_THREADS

    if (rc || Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *rpmfd_isatty(rpmfdObject *s)
{
    int fileno;
    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS
    fileno = Fileno(s->fd);
    Py_END_ALLOW_THREADS

    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    return PyBool_FromLong(isatty(fileno));
}

static PyObject *rpmfd_seek(rpmfdObject *s, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "offset", "whence", NULL };
    off_t offset;
    int whence = SEEK_SET;
    int rc = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "L|i", kwlist, 
				     &offset, &whence)) 
	return NULL;

    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS
    rc = Fseek(s->fd, offset, whence);
    Py_END_ALLOW_THREADS
    if (rc < 0 || Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *rpmfd_tell(rpmfdObject *s)
{
    off_t offset;
    Py_BEGIN_ALLOW_THREADS
    offset = Ftell(s->fd);
    Py_END_ALLOW_THREADS
    return Py_BuildValue("L", offset);
    
}

static PyObject *rpmfd_read(rpmfdObject *s, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "size", NULL };
    char buf[BUFSIZ];
    ssize_t chunksize = sizeof(buf);
    ssize_t left = -1;
    ssize_t nb = 0;
    PyObject *res = NULL;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|l", kwlist, &left))
	return NULL;

    if (s->fd == NULL) return err_closed();

    /* ConcatAndDel() doesn't work on NULL string, meh */
    res = PyBytes_FromStringAndSize(NULL, 0);
    do {
	if (left >= 0 && left < chunksize)
	    chunksize = left;

	Py_BEGIN_ALLOW_THREADS 
	nb = Fread(buf, 1, chunksize, s->fd);
	Py_END_ALLOW_THREADS 

	if (nb > 0) {
	    PyObject *tmp = PyBytes_FromStringAndSize(buf, nb);
	    PyBytes_ConcatAndDel(&res, tmp);
	    left -= nb;
	}
    } while (nb > 0);

    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	Py_XDECREF(res);
	return NULL;
    } else {
	return res;
    }
}

static PyObject *rpmfd_write(rpmfdObject *s, PyObject *args, PyObject *kwds)
{

    const char *buf = NULL;
    ssize_t size = 0;
    char *kwlist[] = { "buffer", NULL };
    ssize_t rc = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, &buf, &size)) {
	return NULL;
    }

    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS 
    rc = Fwrite(buf, 1, size, s->fd);
    Py_END_ALLOW_THREADS 

    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    return Py_BuildValue("n", rc);
}

static char rpmfd_doc[] = "";

static struct PyMethodDef rpmfd_methods[] = {
    { "open",	(PyCFunction) rpmfd_open,	METH_VARARGS|METH_KEYWORDS|METH_CLASS,
	NULL },
    { "close",	(PyCFunction) rpmfd_close,	METH_NOARGS,
	NULL },
    { "fileno",	(PyCFunction) rpmfd_fileno,	METH_NOARGS,
	NULL },
    { "flush",	(PyCFunction) rpmfd_flush,	METH_NOARGS,
	NULL },
    { "isatty",	(PyCFunction) rpmfd_isatty,	METH_NOARGS,
	NULL },
    { "read",	(PyCFunction) rpmfd_read,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "seek",	(PyCFunction) rpmfd_seek,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "tell",	(PyCFunction) rpmfd_tell,	METH_NOARGS,
	NULL },
    { "write",	(PyCFunction) rpmfd_write,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { NULL, NULL }
};

static PyObject *rpmfd_get_closed(rpmfdObject *s)
{
    return PyBool_FromLong((s->fd == NULL));
}

static PyObject *rpmfd_get_name(rpmfdObject *s)
{
    /* XXX: rpm returns non-paths with [mumble], python files use <mumble> */
    return utf8FromString(Fdescr(s->fd));
}

static PyObject *rpmfd_get_mode(rpmfdObject *s)
{
    return utf8FromString(s->mode);
}

static PyObject *rpmfd_get_flags(rpmfdObject *s)
{
    return utf8FromString(s->flags);
}

static PyGetSetDef rpmfd_getseters[] = {
    { "closed", (getter)rpmfd_get_closed, NULL, NULL },
    { "name", (getter)rpmfd_get_name, NULL, NULL },
    { "mode", (getter)rpmfd_get_mode, NULL, NULL },
    { "flags", (getter)rpmfd_get_flags, NULL, NULL },
    { NULL },
};

PyTypeObject rpmfd_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.fd",			/* tp_name */
	sizeof(rpmfdObject),		/* tp_size */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmfd_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	0,				/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmfd_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmfd_methods,			/* tp_methods */
	0,				/* tp_members */
	rpmfd_getseters,		/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc)rpmfd_init,		/* tp_init */
	(allocfunc)0,			/* tp_alloc */
	PyType_GenericNew,		/* tp_new */
	(freefunc)0,			/* tp_free */
	0,				/* tp_is_gc */
};
