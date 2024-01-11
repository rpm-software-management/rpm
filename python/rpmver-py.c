#include "rpmsystem-py.h"
#include "header-py.h"
#include <rpm/rpmver.h>
#include "rpmver-py.h"

struct rpmverObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    rpmver ver;
};

static char ver_doc[] = "";

static void ver_dealloc(rpmverObject *s)
{
    s->ver = rpmverFree(s->ver);
    freefunc free = PyType_GetSlot(Py_TYPE(s), Py_tp_free);
    free(s);
}

int verFromPyObject(PyObject *o, rpmver *ver)
{
    rpmver rv = NULL;
    if (PyUnicode_Check(o)) {
	PyObject *str = NULL;
	if (utf8FromPyObject(o, &str))
	    rv = rpmverParse(PyBytes_AsString(str));
	Py_XDECREF(str);
    } else if (PyTuple_Check(o)) {
	const char *e = NULL;
	const char *v = NULL;
	const char *r = NULL;
	if (PyArg_ParseTuple(o, "zsz", &e, &v, &r))
	    rv = rpmverNew(e, v, r);
    } else {
	PyErr_SetString(PyExc_TypeError, "EVR string or (E,V,R) tuple expected");
	return 0;
    }

    if (rv == NULL) {
	PyErr_SetString(PyExc_ValueError, "invalid version");
	return 0;
    }

    *ver = rv;
    return 1;
}

static PyObject *ver_new(PyTypeObject *subtype,
			     PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "evr", NULL };
    rpmver rv = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O&", kwlist,
		verFromPyObject, &rv))
	return NULL;


    if (rv == NULL && !PyErr_Occurred())
	PyErr_SetString(PyExc_ValueError, "invalid version");

    return rpmver_Wrap(subtype, rv);
}

static PyObject *ver_richcmp(rpmverObject *s, rpmverObject *o, int op)
{
    int v;

    if (!(verObject_Check(s) && verObject_Check(o)))
	Py_RETURN_NOTIMPLEMENTED;

    switch (op) {
    case Py_LT:
	v = rpmverCmp(s->ver, o->ver) < 0;
	break;
    case Py_LE:
	v = rpmverCmp(s->ver, o->ver) <= 0;
	break;
    case Py_EQ:
	v = rpmverCmp(s->ver, o->ver) == 0;
	break;
    case Py_NE:
	v = rpmverCmp(s->ver, o->ver) != 0;
	break;
    case Py_GE:
	v = rpmverCmp(s->ver, o->ver) >= 0;
	break;
    case Py_GT:
	v = rpmverCmp(s->ver, o->ver) > 0;
	break;
    default:
	Py_RETURN_NOTIMPLEMENTED;
    }
    return PyBool_FromLong(v);
}

static PyObject *ver_get_evr(rpmverObject *s)
{
    char *v = rpmverEVR(s->ver);
    PyObject *vo = utf8FromString(v);
    free(v);
    return vo;
}

static PyObject *ver_get_e(rpmverObject *s)
{
    return utf8FromString(rpmverE(s->ver));
}

static PyObject *ver_get_v(rpmverObject *s)
{
    return utf8FromString(rpmverV(s->ver));
}

static PyObject *ver_get_r(rpmverObject *s)
{
    return utf8FromString(rpmverR(s->ver));
}

static PyGetSetDef ver_getseters[] = {
    { "evr",	(getter)ver_get_evr, NULL, NULL },
    { "e",	(getter)ver_get_e, NULL, NULL },
    { "v",	(getter)ver_get_v, NULL, NULL },
    { "r",	(getter)ver_get_r, NULL, NULL },
    { NULL },
};

static PyType_Slot rpmver_Type_Slots[] = {
    {Py_tp_dealloc, ver_dealloc},
    {Py_tp_repr, ver_get_evr},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, ver_doc},
    {Py_tp_richcompare, ver_richcmp},
    {Py_tp_getset, ver_getseters},
    {Py_tp_new, ver_new},
    {0, NULL},
};

PyTypeObject* rpmver_Type;
PyType_Spec rpmver_Type_Spec = {
    .name = "rpm.ver",
    .basicsize = sizeof(rpmverObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmver_Type_Slots,
};

PyObject * rpmver_Wrap(PyTypeObject *subtype, rpmver ver)
{
    rpmverObject *s = NULL;

    if (ver) {
	allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype,
                                                            Py_tp_alloc);
	s = (rpmverObject *)subtype_alloc(subtype, 0);
	if (s == NULL) return NULL;
	s->ver = ver;
    }

    return (PyObject *) s;
}
