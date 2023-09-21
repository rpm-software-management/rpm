#include "rpmsystem-py.h"

#include "rpmps-py.h"

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
    return utf8FromString(rpmProblemGetPkgNEVR(s->prob));
}

static PyObject *rpmprob_get_altnevr(rpmProblemObject *s, void *closure)
{
    return utf8FromString(rpmProblemGetAltNEVR(s->prob));
}

static PyObject *rpmprob_get_key(rpmProblemObject *s, void *closure)
{
    fnpyKey key = rpmProblemGetKey(s->prob);
    if (key) {
	return Py_BuildValue("O", key);
    } else {
	Py_RETURN_NONE;
    }
}

static PyObject *rpmprob_get_str(rpmProblemObject *s, void *closure)
{
    return utf8FromString(rpmProblemGetStr(s->prob));
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
    PyObject *res = utf8FromString(str);
    free(str);
    return res;
}

static void rpmprob_dealloc(rpmProblemObject *s)
{
    s->prob = rpmProblemFree(s->prob);
    freefunc free = PyType_GetSlot(Py_TYPE(s), Py_tp_free);
    free(s);
}

static PyObject *disabled_new(PyTypeObject *type,
                              PyObject *args, PyObject *kwds)
{
    PyErr_SetString(PyExc_TypeError,
                    "TypeError: cannot create 'rpm.prob' instances");
    return NULL;
}

static PyType_Slot rpmProblem_Type_Slots[] = {
    {Py_tp_new, disabled_new},
    {Py_tp_dealloc, rpmprob_dealloc},
    {Py_tp_str, rpmprob_str},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, rpmprob_doc},
    {Py_tp_getset, rpmprob_getseters},
    {0, NULL},
};

PyTypeObject* rpmProblem_Type;
PyType_Spec rpmProblem_Type_Spec = {
    .name = "rpm.prob",
    .basicsize = sizeof(rpmProblemObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = rpmProblem_Type_Slots,
};

PyObject *rpmprob_Wrap(PyTypeObject *subtype, rpmProblem prob)
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    rpmProblemObject *s = (rpmProblemObject *)subtype_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->prob = rpmProblemLink(prob);
    return (PyObject *) s;
}

PyObject *rpmps_AsList(rpmps ps)
{
    PyObject *problems;
    rpmpsi psi;
    rpmProblem prob;

    problems = PyList_New(0);
    if (!problems) {
        return NULL;
    }

    psi = rpmpsInitIterator(ps);

    while ((prob = rpmpsiNext(psi))) {
        PyObject *pyprob = rpmprob_Wrap(rpmProblem_Type, prob);
        if (!pyprob) {
            Py_DECREF(problems);
            rpmpsFreeIterator(psi);
            return NULL;
        }
        PyList_Append(problems, pyprob);
        Py_DECREF(pyprob);
    }
    rpmpsFreeIterator(psi);
    return problems;
}
