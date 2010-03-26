#include "rpmsystem-py.h"

#include "rpmps-py.h"

#include "debug.h"

struct rpmProblemObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    rpmProblem	prob;
};

static char rpmprob_doc[] =
"";

static PyObject *rpmprob_get_type(rpmProblemObject *s, void *closure)
{
    return Py_BuildValue("i", rpmProblemGetType(s->prob));
}

static PyObject *rpmprob_get_pkgnevr(rpmProblemObject *s, void *closure)
{
    return Py_BuildValue("s", rpmProblemGetPkgNEVR(s->prob));
}

static PyObject *rpmprob_get_altnevr(rpmProblemObject *s, void *closure)
{
    return Py_BuildValue("s", rpmProblemGetAltNEVR(s->prob));
}

static PyObject *rpmprob_get_key(rpmProblemObject *s, void *closure)
{
    fnpyKey key = rpmProblemGetKey(s->prob);
    if (key) {
    	return Py_BuildValue("O", rpmProblemGetKey(s->prob));
    } else {
	Py_RETURN_NONE;
    }
}

static PyObject *rpmprob_get_str(rpmProblemObject *s, void *closure)
{
    return Py_BuildValue("s", rpmProblemGetStr(s->prob));
}

static PyObject *rpmprob_get_num(rpmProblemObject *s, void *closure)
{
    return Py_BuildValue("L", rpmProblemGetDiskNeed(s->prob));
}

static PyGetSetDef rpmprob_getseters[] = {
    { "type",		(getter)rpmprob_get_type, NULL, NULL },
    { "pkgNEVR",	(getter)rpmprob_get_pkgnevr, NULL, NULL },
    { "altNEVR",	(getter)rpmprob_get_altnevr, NULL, NULL },
    { "key",		(getter)rpmprob_get_key, NULL, NULL },
    { "_str",		(getter)rpmprob_get_str, NULL, NULL },
    { "_num",		(getter)rpmprob_get_num, NULL, NULL },
    { NULL }
};

static PyObject *rpmprob_str(rpmProblemObject *s)
{
    char *str = rpmProblemString(s->prob);
    PyObject *res = Py_BuildValue("s", str);
    free(str);
    return res;
}

static void rpmprob_dealloc(rpmProblemObject *s)
{
    s->prob = rpmProblemFree(s->prob);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

PyTypeObject rpmProblem_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.prob",			/* tp_name */
	sizeof(rpmProblemObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor)rpmprob_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	0,				/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)rpmprob_str,		/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmprob_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	(richcmpfunc)0,			/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	rpmprob_getseters,		/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	(newfunc)0,			/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

PyObject *rpmprob_Wrap(PyTypeObject *subtype, rpmProblem prob)
{
    rpmProblemObject * s = (rpmProblemObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->prob = rpmProblemLink(prob);
    return (PyObject *) s;
}

PyObject *rpmps_AsList(rpmps ps)
{
    PyObject *problems = PyList_New(0);
    rpmpsi psi = rpmpsInitIterator(ps);
    rpmProblem prob;

    while ((prob = rpmpsiNext(psi))) {
        PyObject *pyprob = rpmprob_Wrap(&rpmProblem_Type, prob);
        PyList_Append(problems, pyprob);
        Py_DECREF(pyprob);
    }
    rpmpsFreeIterator(psi);
    return problems;
}
