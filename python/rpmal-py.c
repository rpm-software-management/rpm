/** \ingroup python
 * \file python/rpmal-py.c
 */

#include "system.h"

#include "Python.h"

#include <rpmlib.h>
#include "rpmps.h"
#include "rpmal.h"

#include "rpmal-py.h"
#include "rpmds-py.h"
#include "rpmfi-py.h"

#include "debug.h"

static PyObject *
rpmal_Debug(rpmalObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "i", &_rpmal_debug)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmal_Add(rpmalObject * s, PyObject * args)
{
    rpmdsObject * dso;
    rpmfiObject * fio;
    PyObject * key;
    alKey pkgKey;

    if (!PyArg_ParseTuple(args, "iOO!O!:Add", &pkgKey, &key, &rpmds_Type, &dso, &rpmfi_Type, &fio))
	return NULL;

    /* XXX errors */
    pkgKey = rpmalAdd(&s->al, pkgKey, key, dso->ds, fio->fi);

    return Py_BuildValue("i", pkgKey);
}

static PyObject *
rpmal_Del(rpmalObject * s, PyObject * args)
{
    alKey pkgKey;

    if (!PyArg_ParseTuple(args, "i:Del", &pkgKey))
	return NULL;

    rpmalDel(s->al, pkgKey);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmal_AddProvides(rpmalObject * s, PyObject * args)
{
    rpmdsObject * dso;
    alKey pkgKey;

    if (!PyArg_ParseTuple(args, "iOO!O!:AddProvides", &pkgKey, &rpmds_Type, &dso))
	return NULL;

    rpmalAddProvides(s->al, pkgKey, dso->ds);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmal_MakeIndex(rpmalObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, ":MakeIndex"))
	return NULL;

    rpmalMakeIndex(s->al);

    Py_INCREF(Py_None);
    return Py_None;
}

static struct PyMethodDef rpmal_methods[] = {
 {"Debug",	(PyCFunction)rpmal_Debug,	METH_VARARGS,
	NULL},
 {"add",	(PyCFunction)rpmal_Add,		METH_VARARGS,
	NULL},
 {"delete",	(PyCFunction)rpmal_Del,		METH_VARARGS,
	NULL},
 {"addProvides",(PyCFunction)rpmal_AddProvides,	METH_VARARGS,
	NULL},
 {"makeIndex",(PyCFunction)rpmal_MakeIndex,	METH_VARARGS,
	NULL},
 {NULL,		NULL}		/* sentinel */
};

/* ---------- */

static void
rpmal_dealloc(rpmalObject * s)
{
    if (s) {
	s->al = rpmalFree(s->al);
	PyMem_DEL(s);
    }
}

static PyObject *
rpmal_getattr(rpmalObject * s, char * name)
{
    return Py_FindMethod(rpmal_methods, (PyObject *)s, name);
}

/**
 */
static char rpmal_doc[] =
"";

PyTypeObject rpmal_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.al",			/* tp_name */
	sizeof(rpmalObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor)rpmal_dealloc,	/* tp_dealloc */
	(printfunc)0,			/* tp_print */
	(getattrfunc)rpmal_getattr,	/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmal_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc)0,			/* tp_iter */
	(iternextfunc)0,		/* tp_iternext */
	rpmal_methods,			/* tp_methods */
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

/* ---------- */

rpmalObject *
rpmal_Wrap(rpmal al)
{
    rpmalObject *s = PyObject_NEW(rpmalObject, &rpmal_Type);
    if (s == NULL)
	return NULL;
    s->al = al;
    return s;
}
