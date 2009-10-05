#include "rpmsystem-py.h"

#include "rpmps-py.h"

#include "debug.h"

struct rpmProblemObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    rpmProblem	prob;
};

struct rpmpsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmps	ps;
    rpmpsi	psi;
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

PyTypeObject rpmProblem_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.prob",			/* tp_name */
	sizeof(rpmProblemObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor)0,			/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
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

static PyObject *rpmprob_Wrap(PyTypeObject *subtype, rpmProblem prob)
{
    rpmProblemObject * s = (rpmProblemObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return PyErr_NoMemory();

    s->prob = prob;
    return (PyObject *) s;
}

static int
rpmps_append(rpmpsObject * s, PyObject * value)
{
    char *pkgNEVR, *altNEVR, *str1;
    unsigned long ulong1;
    int ignoreProblem;
    rpmProblemType type;
    fnpyKey key;

    if (!PyArg_ParseTuple(value, "ssOiisN:rpmps value tuple",
			&pkgNEVR, &altNEVR, &key,
			&type, &ignoreProblem, &str1,
			&ulong1))
    {
    	return -1;
    }
    rpmpsAppend(s->ps, type, pkgNEVR, key, str1, NULL, altNEVR, ulong1);
    return 0;
}

static PyObject *
rpmps_iternext(rpmpsObject * s)
{
    PyObject * result = NULL;

    /* Reset loop indices on 1st entry. */
    if (s->psi == NULL) {
    	s->psi = rpmpsInitIterator(s->ps);
    }

    if (rpmpsNextIterator(s->psi) >= 0) {
	result = rpmprob_Wrap(&rpmProblem_Type, rpmpsGetProblem(s->psi));
    } else {
	s->psi = rpmpsFreeIterator(s->psi);
    }

    return result;
}

static struct PyMethodDef rpmps_methods[] = {
  {"append",	(PyCFunction)rpmps_append,	METH_VARARGS, NULL},
 {NULL,		NULL}		/* sentinel */
};

static void
rpmps_dealloc(rpmpsObject * s)
{
    s->ps = rpmpsFree(s->ps);
    s->ob_type->tp_free((PyObject *)s);
}

static int
rpmps_length(rpmpsObject * s)
{
    int rc;
    rc = rpmpsNumProblems(s->ps);
    return rc;
}

static PyObject *
rpmps_subscript(rpmpsObject * s, PyObject * key)
{
    PyObject * result = NULL;
    rpmpsi psi;
    int ix, i;

    if (!PyInt_Check(key)) {
	PyErr_SetString(PyExc_TypeError, "integer expected");
	return NULL;
    }

    ix = (int) PyInt_AsLong(key);
    /* XXX range check */

    psi = rpmpsInitIterator(s->ps);
    while ((i = rpmpsNextIterator(psi)) >= 0) {
	if (i == ix) {
	    char * ps = rpmProblemString(rpmpsGetProblem(psi));
	    result = Py_BuildValue("s", ps);
	    free(ps);
	    break;
	}
    }
    psi = rpmpsFreeIterator(psi);

    return result;
}

static PyMappingMethods rpmps_as_mapping = {
        (lenfunc) rpmps_length,		/* mp_length */
        (binaryfunc) rpmps_subscript,	/* mp_subscript */
};

static PyObject * rpmps_new(PyTypeObject * subtype, PyObject *args, PyObject *kwds)
{
    rpmps ps = rpmpsCreate();
    return rpmps_Wrap(subtype, ps);
}

static char rpmps_doc[] =
"";

PyTypeObject rpmps_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/* ob_size */
	"rpm.ps",			/* tp_name */
	sizeof(rpmpsObject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmps_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0,			/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	(cmpfunc)0,			/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	&rpmps_as_mapping,		/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmps_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	(richcmpfunc)0,			/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc) rpmps_iternext,	/* tp_iternext */
	rpmps_methods,			/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	(newfunc) rpmps_new,		/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

rpmps psFromPs(rpmpsObject * s)
{
    return s->ps;
}

PyObject * rpmps_Wrap(PyTypeObject *subtype, rpmps ps)
{
    rpmpsObject * s = (rpmpsObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return PyErr_NoMemory();

    s->ps = ps; /* XXX refcounts? */
    s->psi = NULL;
    return (PyObject *) s;
}
