#include "rpmsystem-py.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmlib.h>		/* rpmvercmp */

#include "header-py.h"
#include "rpmds-py.h"

#include "debug.h"

struct rpmdsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    int		active;
    rpmds	ds;
};

static PyObject *
rpmds_Count(rpmdsObject * s)
{
    DEPRECATED_METHOD("use len(ds) instead");
    return Py_BuildValue("i", PyMapping_Size((PyObject *)s));
}

static PyObject *
rpmds_Ix(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsIx(s->ds));
}

static PyObject *
rpmds_DNEVR(rpmdsObject * s)
{
    return Py_BuildValue("s", rpmdsDNEVR(s->ds));
}

static PyObject *
rpmds_N(rpmdsObject * s)
{
    return Py_BuildValue("s", rpmdsN(s->ds));
}

static PyObject *
rpmds_EVR(rpmdsObject * s)
{
    return Py_BuildValue("s", rpmdsEVR(s->ds));
}

static PyObject *
rpmds_Flags(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsFlags(s->ds));
}

static PyObject *
rpmds_BT(rpmdsObject * s)
{
    return Py_BuildValue("i", (int) rpmdsBT(s->ds));
}

static PyObject *
rpmds_TagN(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsTagN(s->ds));
}

static PyObject *
rpmds_Color(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsColor(s->ds));
}

static PyObject *
rpmds_Refs(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsRefs(s->ds));
}

static PyObject *
rpmds_iternext(rpmdsObject * s)
{
    PyObject * result = NULL;

    /* Reset loop indices on 1st entry. */
    if (!s->active) {
	s->ds = rpmdsInit(s->ds);
	s->active = 1;
    }

    /* If more to do, return a (N, EVR, Flags) tuple. */
    if (rpmdsNext(s->ds) >= 0) {
	const char * N = rpmdsN(s->ds);
	const char * EVR = rpmdsEVR(s->ds);
	rpmTag tagN = rpmdsTagN(s->ds);
	rpmsenseFlags Flags = rpmdsFlags(s->ds);

	result = rpmds_Wrap(Py_TYPE(s), rpmdsSingle(tagN, N, EVR, Flags) );
    } else
	s->active = 0;

    return result;
}

static PyObject *
rpmds_SetNoPromote(rpmdsObject * s, PyObject * args, PyObject * kwds)
{
    int nopromote;
    char * kwlist[] = {"noPromote", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i:SetNoPromote", kwlist,
	    &nopromote))
	return NULL;

    return Py_BuildValue("i", rpmdsSetNoPromote(s->ds, nopromote));
}

static PyObject *
rpmds_Notify(rpmdsObject * s, PyObject * args, PyObject * kwds)
{
    const char * where;
    int rc;
    char * kwlist[] = {"location", "returnCode", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "si:Notify", kwlist,
	    &where, &rc))
	return NULL;

    rpmdsNotify(s->ds, where, rc);
    Py_RETURN_NONE;
}

/* XXX rpmdsFind uses bsearch on s->ds, so a sort is needed. */
static PyObject *
rpmds_Sort(rpmdsObject * s)
{
    /* XXX sort on (N,EVR,F) here. */
    Py_RETURN_NONE;
}

static PyObject *
rpmds_Find(rpmdsObject * s, PyObject * arg)
{
    rpmdsObject * o;

    if (!PyArg_Parse(arg, "O!:Find", &rpmds_Type, &o))
	return NULL;

    /* XXX make sure ods index is valid, real fix in lib/rpmds.c. */
    if (rpmdsIx(o->ds) == -1)	rpmdsSetIx(o->ds, 0);

    return Py_BuildValue("i", rpmdsFind(s->ds, o->ds));
}

static PyObject *
rpmds_Merge(rpmdsObject * s, PyObject * arg)
{
    rpmdsObject * o;

    if (!PyArg_Parse(arg, "O!:Merge", &rpmds_Type, &o))
	return NULL;

    return Py_BuildValue("i", rpmdsMerge(&s->ds, o->ds));
}
static PyObject *
rpmds_Search(rpmdsObject * s, PyObject * arg)
{
    rpmdsObject * o;

    if (!PyArg_Parse(arg, "O!:Merge", &rpmds_Type, &o))
        return NULL;

    return Py_BuildValue("i", rpmdsSearch(s->ds, o->ds));
}

static PyObject *rpmds_Compare(rpmdsObject * s, PyObject * o)
{
    rpmdsObject * ods;

    if (!PyArg_Parse(o, "O!:Compare", &rpmds_Type, &ods))
	return NULL;

    return PyBool_FromLong(rpmdsCompare(s->ds, ods->ds));
}

static PyObject * rpmds_Rpmlib(rpmdsObject * s)
{
    rpmds ds = NULL;
    int xx;

    /* XXX check return code, permit arg (NULL uses system default). */
    xx = rpmdsRpmlib(&ds, NULL);

    return rpmds_Wrap(&rpmds_Type, ds);
}

static struct PyMethodDef rpmds_methods[] = {
 {"Count",	(PyCFunction)rpmds_Count,	METH_NOARGS,
	"Deprecated, use len(ds) instead.\n" },
 {"Ix",		(PyCFunction)rpmds_Ix,		METH_NOARGS,
	"ds.Ix -> Ix		- Return current element index.\n" },
 {"DNEVR",	(PyCFunction)rpmds_DNEVR,	METH_NOARGS,
	"ds.DNEVR -> DNEVR	- Return current DNEVR.\n" },
 {"N",		(PyCFunction)rpmds_N,		METH_NOARGS,
	"ds.N -> N		- Return current N.\n" },
 {"EVR",	(PyCFunction)rpmds_EVR,		METH_NOARGS,
	"ds.EVR -> EVR		- Return current EVR.\n" },
 {"Flags",	(PyCFunction)rpmds_Flags,	METH_NOARGS,
	"ds.Flags -> Flags	- Return current Flags.\n" },
 {"BT",		(PyCFunction)rpmds_BT,		METH_NOARGS,
	"ds.BT -> BT	- Return build time.\n" },
 {"TagN",	(PyCFunction)rpmds_TagN,	METH_NOARGS,
	"ds.TagN -> TagN	- Return current TagN.\n" },
 {"Color",	(PyCFunction)rpmds_Color,	METH_NOARGS,
	"ds.Color -> Color	- Return current Color.\n" },
 {"Refs",	(PyCFunction)rpmds_Refs,	METH_NOARGS,
	"ds.Refs -> Refs	- Return current Refs.\n" },
 {"SetNoPromote",(PyCFunction)rpmds_SetNoPromote, METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Notify",	(PyCFunction)rpmds_Notify,	METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Sort",	(PyCFunction)rpmds_Sort,	METH_NOARGS,
	NULL},
 {"Find",	(PyCFunction)rpmds_Find,	METH_O,
	NULL},
 {"Merge",	(PyCFunction)rpmds_Merge,	METH_O,
	NULL},
 {"Search",     (PyCFunction)rpmds_Search,      METH_O,
"ds.Search(element) -> matching ds index (-1 on failure)\n\
- Check that element dependency range overlaps some member of ds.\n\
The current index in ds is positioned at overlapping member upon success.\n" },
 {"Rpmlib",     (PyCFunction)rpmds_Rpmlib,      METH_NOARGS|METH_STATIC,
	"ds.Rpmlib -> nds       - Return internal rpmlib dependency set.\n"},
 {"Compare",	(PyCFunction)rpmds_Compare,	METH_O,
	NULL},
 {NULL,		NULL}		/* sentinel */
};

/* ---------- */

static void
rpmds_dealloc(rpmdsObject * s)
{
    s->ds = rpmdsFree(s->ds);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static Py_ssize_t rpmds_length(rpmdsObject * s)
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
    return Py_BuildValue("s", rpmdsDNEVR(s->ds));
}

static PyMappingMethods rpmds_as_mapping = {
        (lenfunc) rpmds_length,		/* mp_length */
        (binaryfunc) rpmds_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

static int rpmds_init(rpmdsObject * s, PyObject *args, PyObject *kwds)
{
    s->active = 0;
    return 0;
}

static PyObject * rpmds_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
{
    PyObject *obj;
    rpmTag tagN = RPMTAG_REQUIRENAME;
    rpmds ds = NULL;
    Header h = NULL;
    char * kwlist[] = {"obj", "tag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO&:rpmds_new", kwlist, 
	    	 &obj, tagNumFromPyObject, &tagN))
	return NULL;

    if (PyTuple_Check(obj)) {
	const char *name = NULL;
	const char *evr = NULL;
	rpmsenseFlags flags = RPMSENSE_ANY;
	if (PyArg_ParseTuple(obj, "s|is", &name, &flags, &evr)) {
	    ds = rpmdsSingle(tagN, name, evr, flags);
	}
    } else if (hdrFromPyObject(obj, &h)) {
	if (tagN == RPMTAG_NEVR) {
	    ds = rpmdsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
	} else {
	    ds = rpmdsNew(h, tagN, 0);
	}
    } else {
	PyErr_SetString(PyExc_TypeError, "header or tuple expected");
	return NULL;
    }
    
    return rpmds_Wrap(subtype, ds);
}

static char rpmds_doc[] =
"";

PyTypeObject rpmds_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.ds",			/* tp_name */
	sizeof(rpmdsObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmds_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	0,				/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmds_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	rpmds_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc) rpmds_iternext,	/* tp_iternext */
	rpmds_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc) rpmds_init,		/* tp_init */
	0,				/* tp_alloc */
	(newfunc) rpmds_new,		/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

/* ---------- */

rpmds dsFromDs(rpmdsObject * s)
{
    return s->ds;
}

PyObject * rpmds_Wrap(PyTypeObject *subtype, rpmds ds)
{
    rpmdsObject * s = (rpmdsObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->ds = ds;
    s->active = 0;
    return (PyObject *) s;
}

