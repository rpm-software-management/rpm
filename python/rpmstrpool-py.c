#include "rpmsystem-py.h"
#include <rpm/rpmstrpool.h>
#include "rpmstrpool-py.h"

struct rpmstrPoolObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    rpmstrPool pool;
};

static char strpool_doc[] = "";

static void strpool_dealloc(rpmstrPoolObject *s)
{
    s->pool = rpmstrPoolFree(s->pool);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject *strpool_new(PyTypeObject *subtype,
			     PyObject *args, PyObject *kwds)
{
    return rpmstrPool_Wrap(subtype, NULL);
}

static PyObject *strpool_str2id(rpmstrPoolObject *s,
				PyObject *args, PyObject *kwds)
{
    char * kwlist[] = { "str", "create", NULL };
    const char *str = NULL;
    int create = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &str, &create))
	return NULL;

    return PyLong_FromLong(rpmstrPoolId(s->pool, str, create));
}

static PyObject *strpool_id2str(rpmstrPoolObject *s, PyObject *item)
{
    PyObject *ret = NULL;
    rpmsid id = 0;

    if (PyArg_Parse(item, "I", &id)) {
	const char *str = rpmstrPoolStr(s->pool, id);

	if (str)
	    ret = PyBytes_FromString(str);
	else 
	    PyErr_SetObject(PyExc_KeyError, item);
    }
    return ret;
}

static PyObject *strpool_freeze(rpmstrPoolObject *s,
				PyObject *args, PyObject *kwds)
{
    char * kwlist[] = { "keephash", NULL };
    int keephash = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, &keephash))
	return NULL;

    rpmstrPoolFreeze(s->pool, keephash);
    Py_RETURN_NONE;
}

static PyObject *strpool_unfreeze(rpmstrPoolObject *s)
{
    rpmstrPoolUnfreeze(s->pool);
    Py_RETURN_NONE;
}

static Py_ssize_t strpool_length(rpmstrPoolObject *s)
{
    return rpmstrPoolNumStr(s->pool);
}

static struct PyMethodDef strpool_methods[] = {
    { "str2id",	(PyCFunction)strpool_str2id,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "id2str", (PyCFunction)strpool_id2str,	METH_O,
	NULL },
    { "freeze", (PyCFunction)strpool_freeze,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "unfreeze", (PyCFunction)strpool_unfreeze, METH_NOARGS,
	NULL },
    { NULL,	NULL }
};

static PyMappingMethods strpool_as_mapping = {
    (lenfunc) strpool_length,		/* mp_length */
    (binaryfunc) strpool_id2str,	/* mp_subscript */
    (objobjargproc) 0,			/* mp_ass_subscript */
};

PyTypeObject rpmstrPool_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.strpool",			/* tp_name */
	sizeof(rpmstrPoolObject),	/* tp_size */
	0,				/* tp_itemsize */
	(destructor) strpool_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&strpool_as_mapping,		/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	strpool_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	strpool_methods,		/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	strpool_new,			/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

PyObject * rpmstrPool_Wrap(PyTypeObject *subtype, rpmstrPool pool)
{
    rpmstrPoolObject *s = (rpmstrPoolObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    /* permit referencing a pre-existing pool as well */
    s->pool = (pool != NULL) ? rpmstrPoolLink(pool) : rpmstrPoolCreate();

    return (PyObject *) s;
}

int poolFromPyObject(PyObject *item, rpmstrPool *pool)
{
    rpmstrPoolObject *p = NULL;
    if (PyArg_Parse(item, "O!", &rpmstrPool_Type, &p))
	*pool = p->pool;
    return (p != NULL);
}
