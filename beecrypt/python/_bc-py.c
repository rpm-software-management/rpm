/** \ingroup py_c  
 * \file python/_bc-py.c
 */

#define	_REENTRANT	1	/* XXX config.h collides with pyconfig.h */
#include "config.h"

#include "Python.h"
#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "beecrypt/python/mpw-py.h"
#include "beecrypt/python/rng-py.h"

#ifdef __LCLINT__
#undef	PyObject_HEAD
#define	PyObject_HEAD	int _PyObjectHead
#endif

/**
 */
PyObject * py_bcError;

/**
 */
static PyMethodDef _bcModuleMethods[] = {
    { NULL }
} ;

/**
 */
static char _bc__doc__[] =
"";

void init_bc(void);	/* XXX eliminate gcc warning */
/**
 */
void init_bc(void)
{
    PyObject *d, *m;
#ifdef	NOTYET
    PyObject *o, *dict;
    int i;
#endif

    if (PyType_Ready(&mpw_Type) < 0) return;
    if (PyType_Ready(&rng_Type) < 0) return;

    m = Py_InitModule3("_bc", _bcModuleMethods, _bc__doc__);
    if (m == NULL)
	return;

    d = PyModule_GetDict(m);

    py_bcError = PyErr_NewException("_bc.error", NULL, NULL);
    if (py_bcError != NULL)
	PyDict_SetItemString(d, "error", py_bcError);

    Py_INCREF(&mpw_Type);
    PyModule_AddObject(m, "mpw", (PyObject *) &mpw_Type);

    Py_INCREF(&rng_Type);
    PyModule_AddObject(m, "rng", (PyObject *) &rng_Type);

#ifdef	NOTYET
    dict = PyDict_New();

    for (i = 0; i < _bcTagTableSize; i++) {
	tag = PyInt_FromLong(_bcTagTable[i].val);
	PyDict_SetItemString(d, (char *) _bcTagTable[i].name, tag);
	Py_DECREF(tag);
        PyDict_SetItem(dict, tag, o=PyString_FromString(_bcTagTable[i].name + 7));
	Py_DECREF(o);
    }

    while (extensions->name) {
	if (extensions->type == HEADER_EXT_TAG) {
            (const struct headerSprintfExtension *) ext = extensions;
            PyDict_SetItemString(d, (char *) extensions->name, o=PyCObject_FromVoidPtr(ext, NULL));
	    Py_DECREF(o);
            PyDict_SetItem(dict, tag, o=PyString_FromString(ext->name + 7));
	    Py_DECREF(o);    
        }
        extensions++;
    }

    PyDict_SetItemString(d, "tagnames", dict);
    Py_DECREF(dict);

#define REGISTER_ENUM(val) \
    PyDict_SetItemString(d, #val, o=PyInt_FromLong( val )); \
    Py_DECREF(o);

#endif
    
}
