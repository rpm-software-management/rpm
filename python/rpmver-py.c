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
    Py_TYPE(s)->tp_free((PyObject *)s);
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

PyTypeObject rpmver_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.ver",			/* tp_name */
	sizeof(rpmverObject),	/* tp_size */
	0,				/* tp_itemsize */
	(destructor) ver_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	(reprfunc)ver_get_evr,		/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	ver_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	(richcmpfunc)ver_richcmp,	/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	ver_getseters,			/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	ver_new,			/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

PyObject * rpmver_Wrap(PyTypeObject *subtype, rpmver ver)
{
    rpmverObject *s = NULL;

    if (ver) {
	s = (rpmverObject *)subtype->tp_alloc(subtype, 0);
	if (s == NULL) return NULL;
	s->ver = ver;
    }

    return (PyObject *) s;
}
