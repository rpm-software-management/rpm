/** \ingroup python
 * \file python/rpmds-py.c
 */

#include "system.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include <rpmlib.h>

#include "header-py.h"
#include "rpmds-py.h"

#include "debug.h"

/*@access rpmds @*/

static PyObject *
rpmds_Debug(/*@unused@*/ rpmdsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    if (!PyArg_ParseTuple(args, "i", &_rpmds_debug)) return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
rpmds_Count(rpmdsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":Count")) return NULL;
    return Py_BuildValue("i", rpmdsCount(s->ds));
}

static PyObject *
rpmds_Ix(rpmdsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":Ix")) return NULL;
    return Py_BuildValue("i", rpmdsIx(s->ds));
}

static PyObject *
rpmds_DNEVR(rpmdsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":DNEVR")) return NULL;
    return Py_BuildValue("s", rpmdsDNEVR(s->ds));
}

static PyObject *
rpmds_N(rpmdsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":N")) return NULL;
    return Py_BuildValue("s", rpmdsN(s->ds));
}

static PyObject *
rpmds_EVR(rpmdsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":EVR")) return NULL;
    return Py_BuildValue("s", rpmdsEVR(s->ds));
}

static PyObject *
rpmds_Flags(rpmdsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":Flags")) return NULL;
    return Py_BuildValue("i", rpmdsFlags(s->ds));
}

static PyObject *
rpmds_TagN(rpmdsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":TagN")) return NULL;
    return Py_BuildValue("i", rpmdsTagN(s->ds));
}

static int
rpmds_compare(rpmdsObject * a, rpmdsObject * b)
	/*@*/
{
    return rpmdsCompare(a->ds, b->ds);
}

static PyObject *
rpmds_iter(rpmdsObject * s)
	/*@*/
{
    Py_INCREF(s);
    return (PyObject *)s;
}

static PyObject *
rpmds_iternext(rpmdsObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result = NULL;

    /* Reset loop indices on 1st entry. */
    if (!s->active) {
	rpmdsInit(s->ds);
	s->active = 1;
    }

    /* If more to do, return a (N, EVR, Flags) tuple. */
    if (rpmdsNext(s->ds) >= 0) {
	const char * N = rpmdsN(s->ds);
	const char * EVR = rpmdsEVR(s->ds);
	int Flags = rpmdsFlags(s->ds);

	result = PyTuple_New(3);
	PyTuple_SET_ITEM(result, 0, Py_BuildValue("s", N));
	if (EVR == NULL) {
	    Py_INCREF(Py_None);
	    PyTuple_SET_ITEM(result, 1, Py_None);
	    Py_INCREF(Py_None);
	    PyTuple_SET_ITEM(result, 2, Py_None);
	} else {
	    PyTuple_SET_ITEM(result, 1, Py_BuildValue("s", EVR));
	    PyTuple_SET_ITEM(result, 2, PyInt_FromLong(Flags));
	}
	    
    } else
	s->active = 0;

    return result;
}

static PyObject *
rpmds_Next(rpmdsObject * s, PyObject *args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result;

    if (!PyArg_ParseTuple(args, ":Next"))
	return NULL;

    result = rpmds_iternext(s);

    if (result == NULL) {
	Py_INCREF(Py_None);
        return Py_None;
    }
    return result;
}

static PyObject *
rpmds_SetNoPromote(rpmdsObject * s, PyObject * args)
	/*@modifies s @*/
{
    int nopromote;

    if (!PyArg_ParseTuple(args, "i:SetNoPromote", &nopromote))
	return NULL;
    return Py_BuildValue("i", rpmdsSetNoPromote(s->ds, nopromote));
}

static PyObject *
rpmds_Notify(rpmdsObject * s, PyObject * args)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    const char * where;
    int rc;

    if (!PyArg_ParseTuple(args, "si:Notify", &where, &rc))
	return NULL;
    rpmdsNotify(s->ds, where, rc);
    Py_INCREF(Py_None);
    return Py_None;
}

#ifdef	NOTYET
static PyObject *
rpmds_Problem(rpmdsObject * s, PyObject * args)
	/*@*/
{
    if (!PyArg_ParseTuple(args, ":Problem"))
	return NULL;
    Py_INCREF(Py_None);
    return Py_None;
}
#endif

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static struct PyMethodDef rpmds_methods[] = {
 {"Debug",	(PyCFunction)rpmds_Debug,	METH_VARARGS,
	NULL},
 {"Count",	(PyCFunction)rpmds_Count,	METH_VARARGS,
	"ds.Count -> Count	- Return no. of elements.\n" },
 {"Ix",		(PyCFunction)rpmds_Ix,		METH_VARARGS,
	"ds.Ix -> Ix		- Return current element index.\n" },
 {"DNEVR",	(PyCFunction)rpmds_DNEVR,	METH_VARARGS,
	"ds.DNEVR -> DNEVR	- Return current DNEVR.\n" },
 {"N",		(PyCFunction)rpmds_N,		METH_VARARGS,
	"ds.N -> N		- Return current N.\n" },
 {"EVR",	(PyCFunction)rpmds_EVR,		METH_VARARGS,
	"ds.EVR -> EVR		- Return current EVR.\n" },
 {"Flags",	(PyCFunction)rpmds_Flags,	METH_VARARGS,
	"ds.Flags -> Flags	- Return current Flags.\n" },
 {"TagN",	(PyCFunction)rpmds_TagN,	METH_VARARGS,
	"ds.TagN -> TagN	- Return current TagN.\n" },
 {"next",	(PyCFunction)rpmds_Next,	METH_VARARGS,
"ds.next() -> (N, EVR, Flags)\n\
- Retrieve next dependency triple.\n" }, 
 {"SetNoPromote",(PyCFunction)rpmds_SetNoPromote, METH_VARARGS,
	NULL},
 {"Notify",	(PyCFunction)rpmds_Notify,	METH_VARARGS,
	NULL},
#ifdef	NOTYET
 {"Problem",	(PyCFunction)rpmds_Problem,	METH_VARARGS,
	NULL},
#endif
 {NULL,		NULL}		/* sentinel */
};
/*@=fullinitblock@*/

/* ---------- */

static void
rpmds_dealloc(rpmdsObject * s)
	/*@modifies s @*/
{
    if (s) {
	s->ds = rpmdsFree(s->ds);
	PyObject_Del(s);
    }
}

static int
rpmds_print(rpmdsObject * s, FILE * fp, /*@unused@*/ int flags)
	/*@globals fileSystem @*/
	/*@modifies s, fp, fileSystem @*/
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
	/*@*/
{
    return Py_FindMethod(rpmds_methods, (PyObject *)s, name);
}

static int
rpmds_length(rpmdsObject * s)
	/*@*/
{
    return rpmdsCount(s->ds);
}

static PyObject *
rpmds_subscript(rpmdsObject * s, PyObject * key)
	/*@modifies s @*/
{
    int ix;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    ix = (int) PyInt_AsLong(key);
    rpmdsSetIx(s->ds, ix);
    return Py_BuildValue("s", rpmdsDNEVR(s->ds));
}

static PyMappingMethods rpmds_as_mapping = {
        (inquiry) rpmds_length,		/* mp_length */
        (binaryfunc) rpmds_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

/**
 */
/*@unchecked@*/ /*@observer@*/
static char rpmds_doc[] =
"";

/*@-fullinitblock@*/
PyTypeObject rpmds_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.ds",			/* tp_name */
	sizeof(rpmdsObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmds_dealloc,	/* tp_dealloc */
	(printfunc) rpmds_print,	/* tp_print */
	(getattrfunc) rpmds_getattr,	/* tp_getattr */
	(setattrfunc) 0,		/* tp_setattr */
	(cmpfunc) rpmds_compare,	/* tp_compare */
	(reprfunc) 0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmds_as_mapping,		/* tp_as_mapping */
	(hashfunc) 0,			/* tp_hash */
	(ternaryfunc) 0,		/* tp_call */
	(reprfunc) 0,			/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	rpmds_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	(getiterfunc) rpmds_iter,	/* tp_iter */
	(iternextfunc) rpmds_iternext,	/* tp_iternext */
	rpmds_methods,			/* tp_methods */
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
/*@=fullinitblock@*/

/* ---------- */

rpmds dsFromDs(rpmdsObject * s)
{
    return s->ds;
}

rpmdsObject *
rpmds_Wrap(rpmds ds)
{
    rpmdsObject * s = PyObject_New(rpmdsObject, &rpmds_Type);

    if (s == NULL)
	return NULL;
    s->ds = ds;
    s->active = 0;
    return s;
}

rpmdsObject *
rpmds_Single(/*@unused@*/ PyObject * s, PyObject * args)
{
    PyObject * to = NULL;
    int tagN = RPMTAG_PROVIDENAME;
    const char * N;
    const char * EVR = NULL;
    int Flags = 0;

    if (!PyArg_ParseTuple(args, "Os|si:Single", &to, &N, &EVR, &Flags))
	return NULL;
    if (to != NULL) {
	tagN = tagNumFromPyObject(to);
	if (tagN == -1) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return NULL;
	}
    }
    return rpmds_Wrap( rpmdsSingle(tagN, N, EVR, Flags) );
}

rpmdsObject *
hdr_dsFromHeader(PyObject * s, PyObject * args)
{
    hdrObject * ho = (hdrObject *)s;
    PyObject * to = NULL;
    rpmTag tagN = RPMTAG_REQUIRENAME;
    int scareMem = 0;

    if (!PyArg_ParseTuple(args, "|O:dsFromHeader", &to))
	return NULL;
    if (to != NULL) {
	tagN = tagNumFromPyObject(to);
	if (tagN == -1) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return NULL;
	}
    }
    return rpmds_Wrap( rpmdsNew(hdrGetHeader(ho), tagN, scareMem) );
}

rpmdsObject *
hdr_dsOfHeader(PyObject * s, PyObject * args)
{
    hdrObject * ho = (hdrObject *)s;
    int tagN = RPMTAG_PROVIDENAME;
    int Flags = RPMSENSE_EQUAL;

    if (!PyArg_ParseTuple(args, ":dsOfHeader"))
	return NULL;
    return rpmds_Wrap( rpmdsThis(hdrGetHeader(ho), tagN, Flags) );
}
