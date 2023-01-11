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
    freefunc free = PyType_GetSlot(Py_TYPE(s), Py_tp_free);
    free(s);
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
	    ret = utf8FromString(str);
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


static PyType_Slot rpmstrPool_Type_Slots[] = {
    {Py_tp_dealloc, strpool_dealloc},
    {Py_mp_length, strpool_length},
    {Py_mp_subscript, strpool_id2str},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, strpool_doc},
    {Py_tp_methods, strpool_methods},
    {Py_tp_new, strpool_new},
    {0, NULL},
};

PyTypeObject* rpmstrPool_Type;
PyType_Spec rpmstrPool_Type_Spec = {
    .name = "rpm.strpool",
    .basicsize = sizeof(rpmstrPoolObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmstrPool_Type_Slots,
};

PyObject * rpmstrPool_Wrap(PyTypeObject *subtype, rpmstrPool pool)
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    rpmstrPoolObject *s = (rpmstrPoolObject *)subtype_alloc(subtype, 0);
    if (s == NULL) return NULL;

    /* permit referencing a pre-existing pool as well */
    s->pool = (pool != NULL) ? rpmstrPoolLink(pool) : rpmstrPoolCreate();

    return (PyObject *) s;
}

int poolFromPyObject(PyObject *item, rpmstrPool *pool)
{
    rpmstrPoolObject *p = NULL;
    if (PyArg_Parse(item, "O!", rpmstrPool_Type, &p))
	*pool = p->pool;
    return (p != NULL);
}
