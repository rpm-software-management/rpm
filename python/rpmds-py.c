#include "rpmsystem-py.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmlib.h>		/* rpmvercmp */

#include "header-py.h"
#include "rpmds-py.h"
#include "rpmstrpool-py.h"

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
	result = rpmds_Wrap(Py_TYPE(s), rpmdsCurrent(s->ds));
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

static PyObject *rpmds_Instance(rpmdsObject * s)
{
    return Py_BuildValue("i", rpmdsInstance(s->ds));
}

static PyObject * rpmds_Rpmlib(rpmdsObject * s, PyObject *args, PyObject *kwds)
{
    rpmstrPool pool = NULL;
    rpmds ds = NULL;
    char * kwlist[] = {"pool", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O&:rpmds_Rpmlib", kwlist, 
		 &poolFromPyObject, &pool))
	return NULL;

    /* XXX check return code, permit arg (NULL uses system default). */
    rpmdsRpmlibPool(pool, &ds, NULL);

    return rpmds_Wrap(&rpmds_Type, ds);
}

static struct PyMethodDef rpmds_methods[] = {
 {"Count",	(PyCFunction)rpmds_Count,	METH_NOARGS,
	"Deprecated, use len(ds) instead.\n" },
 {"Ix",		(PyCFunction)rpmds_Ix,		METH_NOARGS,
	"ds.Ix -> Ix -- Return current element index.\n" },
 {"DNEVR",	(PyCFunction)rpmds_DNEVR,	METH_NOARGS,
	"ds.DNEVR -> DNEVR -- Return current DNEVR.\n" },
 {"N",		(PyCFunction)rpmds_N,		METH_NOARGS,
	"ds.N -> N -- Return current N.\n" },
 {"EVR",	(PyCFunction)rpmds_EVR,		METH_NOARGS,
	"ds.EVR -> EVR -- Return current EVR.\n" },
 {"Flags",	(PyCFunction)rpmds_Flags,	METH_NOARGS,
	"ds.Flags -> Flags -- Return current Flags.\n" },
 {"TagN",	(PyCFunction)rpmds_TagN,	METH_NOARGS,
  "ds.TagN -> TagN -- Return TagN (RPMTAG_*NAME)\n\n"
  "the type of all dependencies in this set.\n" },
 {"Color",	(PyCFunction)rpmds_Color,	METH_NOARGS,
	"ds.Color -> Color -- Return current Color.\n" },
 {"SetNoPromote",(PyCFunction)rpmds_SetNoPromote, METH_VARARGS|METH_KEYWORDS,
  "ds.SetNoPromote(noPromote) -- Set noPromote for this instance.\n\n"
  "If True non existing epochs are no longer equal to an epoch of 0."},
 {"Notify",	(PyCFunction)rpmds_Notify,	METH_VARARGS|METH_KEYWORDS,
  "ds.Notify(location, returnCode) -- Print debug info message\n\n if debugging is enabled."},
 {"Sort",	(PyCFunction)rpmds_Sort,	METH_NOARGS,
	NULL},
 {"Find",	(PyCFunction)rpmds_Find,	METH_O,
  "ds.find(other_ds) -- Return index of other_ds in ds"},
 {"Merge",	(PyCFunction)rpmds_Merge,	METH_O,
	NULL},
 {"Search",     (PyCFunction)rpmds_Search,      METH_O,
"ds.Search(element) -> matching ds index (-1 on failure)\n\
Check that element dependency range overlaps some member of ds.\n\
The current index in ds is positioned at overlapping member." },
 {"Rpmlib",     (PyCFunction)rpmds_Rpmlib,      METH_VARARGS|METH_KEYWORDS|METH_STATIC,
	"ds.Rpmlib -> nds -- Return internal rpmlib dependency set.\n"},
 {"Compare",	(PyCFunction)rpmds_Compare,	METH_O,
  "ds.compare(other) -- Compare current entries of self and other.\n\nReturns True if the entries match each other, False otherwise"},
 {"Instance",	(PyCFunction)rpmds_Instance,	METH_NOARGS,
  "ds.Instance() -- Return rpmdb key of corresponding package or 0."},
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

static int depflags(PyObject *o, rpmsenseFlags *senseFlags)
{
    int ok = 0;
    PyObject *str = NULL;
    rpmsenseFlags flags = RPMSENSE_ANY;

    if (PyInt_Check(o)) {
	ok = 1;
	flags = PyInt_AsLong(o);
    } else if (utf8FromPyObject(o, &str)) {
	ok = 1;
	for (const char *s = PyBytes_AsString(str); *s; s++) {
	    switch (*s) {
	    case '=':
		flags |= RPMSENSE_EQUAL;
		break;
	    case '<':
		flags |= RPMSENSE_LESS;
		break;
	    case '>':
		flags |= RPMSENSE_GREATER;
		break;
	    default:
		ok = 0;
		break;
	    }
	}
	Py_DECREF(str);
    }

    if (flags == (RPMSENSE_EQUAL|RPMSENSE_LESS|RPMSENSE_GREATER))
	ok = 0;

    if (ok)
	*senseFlags = flags;

    return ok;
}

static PyObject * rpmds_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
{
    PyObject *obj;
    rpmTagVal tagN = RPMTAG_REQUIRENAME;
    rpmds ds = NULL;
    Header h = NULL;
    rpmstrPool pool = NULL;
    char * kwlist[] = {"obj", "tag", "pool", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO&|O&:rpmds_new", kwlist, 
	    	 &obj, tagNumFromPyObject, &tagN,
		 &poolFromPyObject, &pool))
	return NULL;

    if (PyTuple_Check(obj)) {
	const char *name = NULL;
	const char *evr = NULL;
	rpmsenseFlags flags = RPMSENSE_ANY;
	/* TODO: if flags are specified, evr should be required too */
	if (PyArg_ParseTuple(obj, "s|O&s", &name, depflags, &flags, &evr)) {
	    ds = rpmdsSinglePool(pool, tagN, name, evr, flags);
	} else {
	    PyErr_SetString(PyExc_ValueError, "invalid dependency tuple");
	    return NULL;
	}
    } else if (hdrFromPyObject(obj, &h)) {
	if (tagN == RPMTAG_NEVR) {
	    ds = rpmdsThisPool(pool, h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
	} else {
	    ds = rpmdsNewPool(pool, h, tagN, 0);
	}
    } else {
	PyErr_SetString(PyExc_TypeError, "header or tuple expected");
	return NULL;
    }
    
    return rpmds_Wrap(subtype, ds);
}

static char rpmds_doc[] =
    "rpm.ds (dependendcy set) gives a more convenient access to dependencies\n\n"
    "It can hold multiple entries of Name Flags and EVR.\n"
    "It typically represents all dependencies of one kind of a package\n"
    "e.g. all Requires or all Conflicts.\n"
    ;

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

