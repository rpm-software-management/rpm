/** \ingroup py_c
 * \file python/rpmds-py.c
 */

#include "system.h"

#include <rpmlib.h>

#include "header-py.h"
#include "rpmds-py.h"

#include "debug.h"

/*@access rpmds @*/

/**
 * Split EVR into epoch, version, and release components.
 * @param evr		[epoch:]version[-release] string
 * @retval *ep		pointer to epoch
 * @retval *vp		pointer to version
 * @retval *rp		pointer to release
 */
static
void rpmds_ParseEVR(char * evr,
		/*@exposed@*/ /*@out@*/ const char ** ep,
		/*@exposed@*/ /*@out@*/ const char ** vp,
		/*@exposed@*/ /*@out@*/ const char ** rp)
	/*@modifies *ep, *vp, *rp @*/
	/*@requires maxSet(ep) >= 0 /\ maxSet(vp) >= 0 /\ maxSet(rp) >= 0 @*/
{
    const char *epoch;
    const char *version;		/* assume only version is present */
    const char *release;
    char *s, *se;

    s = evr;
    while (*s && xisdigit(*s)) s++;	/* s points to epoch terminator */
    se = strrchr(s, '-');		/* se points to version terminator */

    if (*s == ':') {
	epoch = evr;
	*s++ = '\0';
	version = s;
	/*@-branchstate@*/
	if (*epoch == '\0') epoch = "0";
	/*@=branchstate@*/
    } else {
	epoch = NULL;	/* XXX disable epoch compare if missing */
	version = evr;
    }
    if (se) {
/*@-boundswrite@*/
	*se++ = '\0';
/*@=boundswrite@*/
	release = se;
    } else {
	release = NULL;
    }

    if (ep) *ep = epoch;
    if (vp) *vp = version;
    if (rp) *rp = release;
}

/*@null@*/
static PyObject *
rpmds_Debug(/*@unused@*/ rpmdsObject * s, PyObject * args, PyObject * kwds)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    char * kwlist[] = {"debugLevel", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &_rpmds_debug))
	return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

/*@null@*/
static PyObject *
rpmds_Count(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmdsCount(s->ds));
}

/*@null@*/
static PyObject *
rpmds_Ix(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmdsIx(s->ds));
}

/*@null@*/
static PyObject *
rpmds_DNEVR(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmdsDNEVR(s->ds));
}

/*@null@*/
static PyObject *
rpmds_N(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmdsN(s->ds));
}

/*@null@*/
static PyObject *
rpmds_EVR(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("s", rpmdsEVR(s->ds));
}

/*@null@*/
static PyObject *
rpmds_Flags(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmdsFlags(s->ds));
}

/*@null@*/
static PyObject *
rpmds_BT(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("i", (int) rpmdsBT(s->ds));
}

/*@null@*/
static PyObject *
rpmds_TagN(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmdsTagN(s->ds));
}

/*@null@*/
static PyObject *
rpmds_Color(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmdsColor(s->ds));
}

/*@null@*/
static PyObject *
rpmds_Refs(rpmdsObject * s)
	/*@*/
{
    return Py_BuildValue("i", rpmdsRefs(s->ds));
}

/**
 */
static int compare_values(const char *str1, const char *str2)
{
    if (!str1 && !str2)
	return 0;
    else if (str1 && !str2)
	return 1;
    else if (!str1 && str2)
	return -1;
    return rpmvercmp(str1, str2);
}

static int
rpmds_compare(rpmdsObject * a, rpmdsObject * b)
	/*@*/
{
    char *aEVR = xstrdup(rpmdsEVR(a->ds));
    const char *aE, *aV, *aR;
    char *bEVR = xstrdup(rpmdsEVR(b->ds));
    const char *bE, *bV, *bR;
    int rc;

    /* XXX W2DO? should N be compared? */
    rpmds_ParseEVR(aEVR, &aE, &aV, &aR);
    rpmds_ParseEVR(bEVR, &bE, &bV, &bR);

    rc = compare_values(aE, bE);
    if (!rc) {
	rc = compare_values(aV, bV);
	if (!rc)
	    rc = compare_values(aR, bR);
    }

    aEVR = _free(aEVR);
    bEVR = _free(bEVR);

    return rc;
}

static PyObject *
rpmds_richcompare(rpmdsObject * a, rpmdsObject * b, int op)
	/*@*/
{
    int rc;

    switch (op) {
    case Py_NE:
	/* XXX map ranges overlap boolean onto '!=' python syntax. */
	rc = rpmdsCompare(a->ds, b->ds);
	rc = (rc < 0 ? -1 : (rc == 0 ? 1 : 0));
	break;
    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE:
    case Py_EQ:
	/*@fallthrough@*/
    default:
	rc = -1;
	break;
    }
    return Py_BuildValue("i", rc);
}

static PyObject *
rpmds_iter(rpmdsObject * s)
	/*@*/
{
    Py_INCREF(s);
    return (PyObject *)s;
}

/*@null@*/
static PyObject *
rpmds_iternext(rpmdsObject * s)
	/*@modifies s @*/
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
	int tagN = rpmdsTagN(s->ds);
	int Flags = rpmdsFlags(s->ds);

/*@-branchstate@*/
	if (N != NULL) N = xstrdup(N);
	if (EVR != NULL) EVR = xstrdup(EVR);
/*@=branchstate@*/
	result = rpmds_Wrap( rpmdsSingle(tagN, N, EVR, Flags) );
    } else
	s->active = 0;

    return result;
}

/*@null@*/
static PyObject *
rpmds_Next(rpmdsObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies s, _Py_NoneStruct @*/
{
    PyObject * result;

    result = rpmds_iternext(s);

    if (result == NULL) {
	Py_INCREF(Py_None);
        return Py_None;
    }
    return result;
}

/*@null@*/
static PyObject *
rpmds_SetNoPromote(rpmdsObject * s, PyObject * args, PyObject * kwds)
	/*@modifies s @*/
{
    int nopromote;
    char * kwlist[] = {"noPromote", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i:SetNoPromote", kwlist,
	    &nopromote))
	return NULL;

    return Py_BuildValue("i", rpmdsSetNoPromote(s->ds, nopromote));
}

/*@null@*/
static PyObject *
rpmds_Notify(rpmdsObject * s, PyObject * args, PyObject * kwds)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    const char * where;
    int rc;
    char * kwlist[] = {"location", "returnCode", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "si:Notify", kwlist,
	    &where, &rc))
	return NULL;

    rpmdsNotify(s->ds, where, rc);
    Py_INCREF(Py_None);
    return Py_None;
}

/* XXX rpmdsFind uses bsearch on s->ds, so a sort is needed. */
/*@null@*/
static PyObject *
rpmds_Sort(rpmdsObject * s)
	/*@globals _Py_NoneStruct @*/
	/*@modifies _Py_NoneStruct @*/
{
    /* XXX sort on (N,EVR,F) here. */
    Py_INCREF(Py_None);
    return Py_None;
}

/*@null@*/
static PyObject *
rpmds_Find(rpmdsObject * s, PyObject * args, PyObject * kwds)
	/*@modifies s @*/
{
    PyObject * to = NULL;
    rpmdsObject * o;
    int rc;
    char * kwlist[] = {"element", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:Find", kwlist, &to))
	return NULL;

    /* XXX ds type check needed. */
    o = (rpmdsObject *)to;

    /* XXX make sure ods index is valid, real fix in lib/rpmds.c. */
    if (rpmdsIx(o->ds) == -1)	rpmdsSetIx(o->ds, 0);

    rc = rpmdsFind(s->ds, o->ds);
    return Py_BuildValue("i", rc);
}

/*@null@*/
static PyObject *
rpmds_Merge(rpmdsObject * s, PyObject * args, PyObject * kwds)
	/*@modifies s @*/
{
    PyObject * to = NULL;
    rpmdsObject * o;
    char * kwlist[] = {"element", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:Merge", kwlist, &to))
	return NULL;

    /* XXX ds type check needed. */
    o = (rpmdsObject *)to;
    return Py_BuildValue("i", rpmdsMerge(&s->ds, o->ds));
}

#ifdef	NOTYET
static PyObject *
rpmds_Compare(rpmdsObject * s, PyObject * args, PyObject * kwds)
	/*@modifies s @*/
{
    PyObject * to = NULL;
    rpmdsObject * o;
    char * kwlist[] = {"other", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:Compare", kwlist, &to))
	return NULL;

    /* XXX ds type check needed. */
    o = (rpmdsObject *)to;
    return Py_BuildValue("i", rpmdsCompare(s->ds, o->ds));
}

/*@null@*/
static PyObject *
rpmds_Problem(rpmdsObject * s)
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
 {"Debug",	(PyCFunction)rpmds_Debug,	METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Count",	(PyCFunction)rpmds_Count,	METH_NOARGS,
	"ds.Count -> Count	- Return no. of elements.\n" },
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
 {"next",	(PyCFunction)rpmds_Next,	METH_NOARGS,
"ds.next() -> (N, EVR, Flags)\n\
- Retrieve next dependency triple.\n" },
 {"SetNoPromote",(PyCFunction)rpmds_SetNoPromote, METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Notify",	(PyCFunction)rpmds_Notify,	METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Sort",	(PyCFunction)rpmds_Sort,	METH_NOARGS,
	NULL},
 {"Find",	(PyCFunction)rpmds_Find,	METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Merge",	(PyCFunction)rpmds_Merge,	METH_VARARGS|METH_KEYWORDS,
	NULL},
#ifdef	NOTYET
 {"Compare",	(PyCFunction)rpmds_Compare,	METH_VARARGS|METH_KEYWORDS,
	NULL},
 {"Problem",	(PyCFunction)rpmds_Problem,	METH_NOARGS,
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

    s->ds = rpmdsInit(s->ds);
    while (rpmdsNext(s->ds) >= 0)
	fprintf(fp, "%s\n", rpmdsDNEVR(s->ds));
    return 0;
}

static PyObject * rpmds_getattro(PyObject * o, PyObject * n)
	/*@*/
{
    return PyObject_GenericGetAttr(o, n);
}

static int rpmds_setattro(PyObject * o, PyObject * n, PyObject * v)
	/*@*/
{
    return PyObject_GenericSetAttr(o, n, v);
}

static int
rpmds_length(rpmdsObject * s)
	/*@*/
{
    return rpmdsCount(s->ds);
}

/*@null@*/
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
    /* XXX make sure that DNEVR exists. */
    rpmdsSetIx(s->ds, ix-1);
    (void) rpmdsNext(s->ds);
    return Py_BuildValue("s", rpmdsDNEVR(s->ds));
}

static PyMappingMethods rpmds_as_mapping = {
        (inquiry) rpmds_length,		/* mp_length */
        (binaryfunc) rpmds_subscript,	/* mp_subscript */
        (objobjargproc)0,		/* mp_ass_subscript */
};

/** \ingroup py_c
 */
static int rpmds_init(rpmdsObject * s, PyObject *args, PyObject *kwds)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies s, rpmGlobalMacroContext @*/
{
    hdrObject * ho = NULL;
    PyObject * to = NULL;
    int tagN = RPMTAG_REQUIRENAME;
    int flags = 0;
    char * kwlist[] = {"header", "tag", "flags", NULL};

if (_rpmds_debug < 0)
fprintf(stderr, "*** rpmds_init(%p,%p,%p)\n", s, args, kwds);

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!|Oi:rpmds_init", kwlist, 
	    &hdr_Type, &ho, &to, &flags))
	return -1;

    if (to != NULL) {
	tagN = tagNumFromPyObject(to);
	if (tagN == -1) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return -1;
	}
    }
    s->ds = rpmdsNew(hdrGetHeader(ho), tagN, flags);
    s->active = 0;

    return 0;
}

/** \ingroup py_c
 */
static void rpmds_free(/*@only@*/ rpmdsObject * s)
	/*@modifies s @*/
{
if (_rpmds_debug)
fprintf(stderr, "%p -- ds %p\n", s, s->ds);
    s->ds = rpmdsFree(s->ds);

    PyObject_Del((PyObject *)s);
}

/** \ingroup py_c
 */
static PyObject * rpmds_alloc(PyTypeObject * subtype, int nitems)
	/*@*/
{
    PyObject * s = PyType_GenericAlloc(subtype, nitems);

if (_rpmds_debug < 0)
fprintf(stderr, "*** rpmds_alloc(%p,%d) ret %p\n", subtype, nitems, s);
    return s;
}

/** \ingroup py_c
 */
/*@null@*/
static PyObject * rpmds_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/
{
    rpmdsObject * s = (void *) PyObject_New(rpmdsObject, subtype);

    /* Perform additional initialization. */
    if (rpmds_init(s, args, kwds) < 0) {
	rpmds_free(s);
	return NULL;
    }

if (_rpmds_debug)
fprintf(stderr, "%p ++ ds %p\n", s, s->ds);

    return (PyObject *)s;
}

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
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc) rpmds_compare,	/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmds_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	(getattrofunc) rpmds_getattro,	/* tp_getattro */
	(setattrofunc) rpmds_setattro,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT |		/* tp_flags */
	    Py_TPFLAGS_HAVE_RICHCOMPARE,
	rpmds_doc,			/* tp_doc */
#if Py_TPFLAGS_HAVE_ITER
	0,				/* tp_traverse */
	0,				/* tp_clear */
	(richcmpfunc) rpmds_richcompare,/* tp_richcompare */
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
	(initproc) rpmds_init,		/* tp_init */
	(allocfunc) rpmds_alloc,	/* tp_alloc */
	(newfunc) rpmds_new,		/* tp_new */
	rpmds_free,			/* tp_free */
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
rpmds_Single(/*@unused@*/ PyObject * s, PyObject * args, PyObject * kwds)
{
    PyObject * to = NULL;
    int tagN = RPMTAG_PROVIDENAME;
    const char * N;
    const char * EVR = NULL;
    int Flags = 0;
    char * kwlist[] = {"to", "name", "evr", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Os|si:Single", kwlist,
	    &to, &N, &EVR, &Flags))
	return NULL;

    if (to != NULL) {
	tagN = tagNumFromPyObject(to);
	if (tagN == -1) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return NULL;
	}
    }
    if (N != NULL) N = xstrdup(N);
    if (EVR != NULL) EVR = xstrdup(EVR);
    return rpmds_Wrap( rpmdsSingle(tagN, N, EVR, Flags) );
}

rpmdsObject *
hdr_dsFromHeader(PyObject * s, PyObject * args, PyObject * kwds)
{
    hdrObject * ho = (hdrObject *)s;
    PyObject * to = NULL;
    rpmTag tagN = RPMTAG_REQUIRENAME;
    int flags = 0;
    char * kwlist[] = {"to", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oi:dsFromHeader", kwlist,
	    &to, &flags))
	return NULL;

    if (to != NULL) {
	tagN = tagNumFromPyObject(to);
	if (tagN == -1) {
	    PyErr_SetString(PyExc_KeyError, "unknown header tag");
	    return NULL;
	}
    }
    return rpmds_Wrap( rpmdsNew(hdrGetHeader(ho), tagN, flags) );
}

rpmdsObject *
hdr_dsOfHeader(PyObject * s)
{
    hdrObject * ho = (hdrObject *)s;
    int tagN = RPMTAG_PROVIDENAME;
    int Flags = RPMSENSE_EQUAL;

    return rpmds_Wrap( rpmdsThis(hdrGetHeader(ho), tagN, Flags) );
}
