/** \ingroup python
 * \file python/rpmds-py.c
 */

#include "system.h"

#include "Python.h"

#include <rpmlib.h>
#include "rpmps.h"
#include "rpmds.h"

#include "header-py.h"
#include "rpmds-py.h"

#include "debug.h"

static PyObject *
rpmds_Debug(rpmdsObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "i", &_rpmds_debug)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmds_Count(rpmdsObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    return Py_BuildValue("i", rpmdsCount(s->ds));
}

static PyObject *
rpmds_Ix(rpmdsObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    return Py_BuildValue("i", rpmdsIx(s->ds));
}

static PyObject *
rpmds_DNEVR(rpmdsObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmdsDNEVR(s->ds)));
}

static PyObject *
rpmds_N(rpmdsObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmdsN(s->ds)));
}

static PyObject *
rpmds_EVR(rpmdsObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    return Py_BuildValue("s", xstrdup(rpmdsEVR(s->ds)));
}

static PyObject *
rpmds_Flags(rpmdsObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    return Py_BuildValue("i", rpmdsFlags(s->ds));
}

static PyObject *
rpmds_TagN(rpmdsObject * s, PyObject * args)
{
    if (!PyArg_ParseTuple(args, "")) return NULL;
    return Py_BuildValue("i", rpmdsTagN(s->ds));
}

#ifdef	NOTYET
static PyObject *
rpmds_Notify(rpmdsObject * s, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
rpmds_Next(rpmdsObject * s, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
rpmds_Init(rpmdsObject * s, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
rpmds_Compare(rpmdsObject * s, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
rpmds_Problem(rpmdsObject * s, PyObject * args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}
#endif


static struct PyMethodDef rpmds_methods[] = {
 {"Debug",	(PyCFunction)rpmds_Debug,	METH_VARARGS,	NULL},
 {"Count",	(PyCFunction)rpmds_Count,	METH_VARARGS,	NULL},
 {"Ix",		(PyCFunction)rpmds_Ix,		METH_VARARGS,	NULL},
 {"DNEVR",	(PyCFunction)rpmds_DNEVR,	METH_VARARGS,	NULL},
 {"N",		(PyCFunction)rpmds_N,		METH_VARARGS,	NULL},
 {"EVR",	(PyCFunction)rpmds_EVR,		METH_VARARGS,	NULL},
 {"Flags",	(PyCFunction)rpmds_Flags,	METH_VARARGS,	NULL},
 {"TagN",	(PyCFunction)rpmds_TagN,	METH_VARARGS,	NULL},
#ifdef	NOTYET
 {"Notify",	(PyCFunction)rpmds_Notify,	METH_VARARGS,	NULL},
 {"Next",	(PyCFunction)rpmds_Next,	METH_VARARGS,	NULL},
 {"Init",	(PyCFunction)rpmds_Init,	METH_VARARGS,	NULL},
 {"Compare",	(PyCFunction)rpmds_Compare,	METH_VARARGS,	NULL},
 {"Problem",	(PyCFunction)rpmds_Problem,	METH_VARARGS,	NULL},
#endif
 {NULL,		NULL}		/* sentinel */
};

/* ---------- */

static void
rpmds_dealloc(rpmdsObject * s)
{
    if (s && s->ds)
	s->ds = rpmdsFree(s->ds);
    PyMem_DEL(s);
}

static int
rpmds_print(rpmdsObject * s, FILE * fp, int flags)
{
    if (!(s && s->ds))
	return -1;

    rpmdsInit(s->ds);
    while (rpmdsNext(s->ds) >= 0)
	fprintf(fp, "%s\n", rpmdsDNEVR(s->ds));
    return 0;
}

static PyObject *
rpmds_getattr(rpmdsObject * s, char * name)
{
    return Py_FindMethod(rpmds_methods, (PyObject *)s, name);
}

static int
rpmds_length(rpmdsObject * s)
{
    return rpmdsCount(s->ds);
}

static PyObject *
rpmds_subscript(rpmdsObject * s, PyObject * key)
{
    int ix;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    ix = (int) PyInt_AsLong(key);
    rpmdsSetIx(s->ds, ix);
    return Py_BuildValue("s", xstrdup(rpmdsDNEVR(s->ds)));
}

static PyMappingMethods rpmds_as_mapping = {
        (inquiry) rpmds_length,		/* mp_length */
        (binaryfunc) rpmds_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

static PyTypeObject rpmds_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpmds",			/* tp_name */
	sizeof(rpmdsObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor)rpmds_dealloc,	/* tp_dealloc */
	(printfunc)rpmds_print,		/* tp_print */
	(getattrfunc)rpmds_getattr,	/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmds_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */

	/* Space for future expansion */
	0L,0L,0L,0L,
	NULL				/* Documentation string */
};

/* ---------- */

rpmds dsFromDs(rpmdsObject * s)
{
    return s->ds;
}

rpmdsObject *
rpmds_New(rpmds ds)
{
    rpmdsObject *s = PyObject_NEW(rpmdsObject, &rpmds_Type);
    if (s == NULL)
	return NULL;
    s->ds = ds;
    return s;
}

rpmdsObject *
hdr_dsFromHeader(PyObject * s, PyObject * args)
{
    hdrObject * ho;

    if (!PyArg_ParseTuple(args, "O!", &hdrType, &ho))
	return NULL;
    return rpmds_New( rpmdsNew(hdrGetHeader(ho), RPMTAG_REQUIRENAME, 0) );
}
