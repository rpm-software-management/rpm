#include "rpmsystem-py.h"

#include <rpm/rpmsign.h>

static char rpms__doc__[] =
"";

static PyObject * addSign(PyObject * self, PyObject * args, PyObject *kwds)
{
    const char *path = NULL;
    char * kwlist[] = { "path", "keyid", "hashalgo", NULL };
    struct rpmSignArgs sig, *sigp = NULL;

    memset(&sig, 0, sizeof(sig));
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|si", kwlist,
				&path, &sig.keyid, &sig.hashalgo))
	return NULL;

    if (sig.keyid || sig.hashalgo)
	sigp = &sig;

    return PyBool_FromLong(rpmPkgSign(path, sigp) == 0);
}

static PyObject * delSign(PyObject * self, PyObject * args, PyObject *kwds)
{
    const char *path = NULL;
    char * kwlist[] = { "path", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &path))
	return NULL;

    return PyBool_FromLong(rpmPkgDelSign(path) == 0);
}

/*
  Do any common preliminary work before python 2 vs python 3 module creation:
*/
static int prepareInitModule(void)
{
    return 1;
}

static int initModule(PyObject *m)
{
    return 1;
}

static PyMethodDef modMethods[] = {
    { "addSign", (PyCFunction) addSign, METH_VARARGS|METH_KEYWORDS, NULL },
    { "delSign", (PyCFunction) delSign, METH_VARARGS|METH_KEYWORDS, NULL },
    { NULL },
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_rpms",     /* m_name */
    rpms__doc__, /* m_doc */
    0,           /* m_size */
    modMethods,  /* m_methods */
    NULL,        /* m_reload */
    NULL,        /* m_traverse */
    NULL,        /* m_clear */
    NULL         /* m_free */
};

PyObject * PyInit__rpms(void);	/* XXX eliminate gcc warning */
PyObject * PyInit__rpms(void)
{
    PyObject *m;

    if (!prepareInitModule())
        return NULL;
    m = PyModule_Create(&moduledef);
    if (m == NULL || !initModule(m)) {
        Py_XDECREF(m);
        m = NULL;
    }
    return m;
}
#else
void init_rpms(void);	/* XXX eliminate gcc warning */
void init_rpms(void)
{
    PyObject *m;
  
    if (!prepareInitModule())
        return;

    m = Py_InitModule3("_rpms", modMethods, rpms__doc__);
    if (m) initModule(m);
}
#endif
