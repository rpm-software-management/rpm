/** \ingroup python
 * \file python/rpmte-py.c
 */

#include "system.h"

#include "Python.h"

#include <rpmlib.h>
#include "rpmps.h"

#include "rpmds.h"
#include "rpmfi.h"
#include "rpmte.h"

#include "header-py.h"	/* XXX tagNumFromPyObject */
#include "rpmds-py.h"
#include "rpmfi-py.h"
#include "rpmte-py.h"

#include "debug.h"

/** \ingroup python
 * \name Class: rpm.te
 * \class rpm.te
 * \brief An python rpm.te object represents an element of a RPM transaction set.
 */

static PyObject *
rpmte_Debug(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "i", &_rpmte_debug)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmte_N(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":N")) return NULL;
    return Py_BuildValue("s", rpmteN(s->te));
}

static PyObject *
rpmte_E(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":E")) return NULL;
    return Py_BuildValue("s", rpmteE(s->te));
}

static PyObject *
rpmte_V(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":V")) return NULL;
    return Py_BuildValue("s", rpmteV(s->te));
}

static PyObject *
rpmte_R(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":R")) return NULL;
    return Py_BuildValue("s", rpmteR(s->te));
}

static PyObject *
rpmte_A(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":A")) return NULL;
    return Py_BuildValue("s", rpmteA(s->te));
}

static PyObject *
rpmte_O(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":O")) return NULL;
    return Py_BuildValue("s", rpmteO(s->te));
}

static PyObject *
rpmte_NEVR(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":NEVR")) return NULL;
    return Py_BuildValue("s", rpmteNEVR(s->te));
}

static PyObject *
rpmte_MultiLib(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":MultiLib")) return NULL;
    return Py_BuildValue("i", rpmteMultiLib(s->te));
}

static PyObject *
rpmte_Depth(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":Depth")) return NULL;
    return Py_BuildValue("i", rpmteDepth(s->te));
}

static PyObject *
rpmte_Npreds(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":Npreds")) return NULL;
    return Py_BuildValue("i", rpmteNpreds(s->te));
}

static PyObject *
rpmte_Degree(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":Degree")) return NULL;
    return Py_BuildValue("i", rpmteDegree(s->te));
}

static PyObject *
rpmte_AddedKey(rpmteObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":Degree")) return NULL;
    return Py_BuildValue("i", rpmteDegree(s->te));
}

static PyObject *
rpmte_DS(rpmteObject * s, PyObject * args)
{
    PyObject * TagN = NULL;
    rpmds ds;
    rpmTag tag;

    if (!PyArg_ParseTuple(args, "O:DS", &TagN)) return NULL;

    tag = tagNumFromPyObject(TagN);
    if (tag == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    ds = rpmteDS(s->te, tag);
    if (ds == NULL) {
	PyErr_SetString(PyExc_TypeError, "invalid ds tag");
	return NULL;
    }
    return (PyObject *) rpmds_Wrap(ds);
}

static PyObject *
rpmte_FI(rpmteObject * s, PyObject * args)
{
    PyObject * TagN = NULL;
    rpmfi fi;
    rpmTag tag;

    if (!PyArg_ParseTuple(args, "O:FI", &TagN)) return NULL;

    tag = tagNumFromPyObject(TagN);
    if (tag == -1) {
	PyErr_SetString(PyExc_TypeError, "unknown tag type");
	return NULL;
    }

    fi = rpmteFI(s->te, tag);
    if (fi == NULL) {
	PyErr_SetString(PyExc_TypeError, "invalid fi tag");
	return NULL;
    }
    return (PyObject *) rpmfi_Wrap(fi);
}

/** \ingroup python
 */
static struct PyMethodDef rpmte_methods[] = {
    {"Debug",	(PyCFunction)rpmte_Debug,	METH_VARARGS,
        NULL},
    {"N",	(PyCFunction)rpmte_N,		METH_VARARGS,
        NULL},
    {"E",	(PyCFunction)rpmte_E,		METH_VARARGS,
        NULL},
    {"V",	(PyCFunction)rpmte_V,		METH_VARARGS,
        NULL},
    {"R",	(PyCFunction)rpmte_R,		METH_VARARGS,
        NULL},
    {"A",	(PyCFunction)rpmte_A,		METH_VARARGS,
        NULL},
    {"O",	(PyCFunction)rpmte_O,		METH_VARARGS,
        NULL},
    {"NEVR",	(PyCFunction)rpmte_NEVR,	METH_VARARGS,
        NULL},
    {"MultiLib",(PyCFunction)rpmte_MultiLib,	METH_VARARGS,
        NULL},
    {"Depth",	(PyCFunction)rpmte_Depth,	METH_VARARGS,
        NULL},
    {"Npreds",	(PyCFunction)rpmte_Npreds,	METH_VARARGS,
        NULL},
    {"Degree",	(PyCFunction)rpmte_Degree,	METH_VARARGS,
        NULL},
    {"AddedKey",(PyCFunction)rpmte_AddedKey,	METH_VARARGS,
        NULL},
    {"DS",	(PyCFunction)rpmte_DS,		METH_VARARGS,
        NULL},
    {"FI",	(PyCFunction)rpmte_FI,		METH_VARARGS,
        NULL},
    {NULL,		NULL}		/* sentinel */
};

/* ---------- */

/** \ingroup python
 */
static PyObject * rpmte_getattr(rpmteObject * o, char * name)
{
    return Py_FindMethod(rpmte_methods, (PyObject *) o, name);
}

/**
 */
static char rpmte_doc[] =
"";

/** \ingroup python
 */
PyTypeObject rpmte_Type = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"rpm.te",			/* tp_name */
	sizeof(rpmteObject),		/* tp_size */
	0,				/* tp_itemsize */
	(destructor)0,		 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc) rpmte_getattr, 	/* tp_getattr */
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
	rpmte_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmte_methods,			/* tp_methods */
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

rpmteObject * rpmte_Wrap(rpmte te)
{
    rpmteObject *s = PyObject_NEW(rpmteObject, &rpmte_Type);
    if (s == NULL)
	return NULL;
    s->te = te;
    return s;
}
