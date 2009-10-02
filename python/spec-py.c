#include "rpmsystem-py.h"

#include "header-py.h"
#include "spec-py.h"

/** \ingroup python
 * \name Class: Rpmspec
 * \class Rpmspec
 * \brief A python rpm.spec object represents an RPM spec file set.
 * 
 *  The spec file is at the heart of RPM's packaging building process. Similar
 *  in concept to a makefile, it contains information required by RPM to build
 *  the package, as well as instructions telling RPM how to build it. The spec
 *  file also dictates exactly what files are a part of the package, and where
 *  they should be installed.
 *  
 *  The rpm.spec object represents a parsed specfile to aid extraction of data.
 *  
 *  For example
 * \code
 *  import rpm
 *  rpm.addMacro("_topdir","/path/to/topdir")
 *  s=rpm.spec("foo.spec")
 *  print s.prep()
 * \endcode
 *
 *  Macros set using add macro will be used allowing testing of conditional builds
 *
 */

struct specObject_s {
    PyObject_HEAD
    /*type specific fields */
    rpmSpec spec;
};

static void 
spec_dealloc(specObject * s) 
{
    if (s->spec) {
	s->spec=freeSpec(s->spec);
    }
    s->ob_type->tp_free((PyObject *)s);
}

/* XXX TODO return something sensible if spec exists but component (eg %clean)
 * does not. Possibly "" or None */

static PyObject * 
spec_get_buildroot(specObject * s) 
{
    rpmSpec spec = specFromSpec(s);
    if (spec != NULL && spec->buildRoot) {
        return Py_BuildValue("s", spec->buildRoot);
    }
    else {
        return NULL;
    }
}

static PyObject * 
spec_get_prep(specObject * s) 
{
    rpmSpec spec = specFromSpec(s);
    PyObject *res = NULL;
    if (spec != NULL && spec->prep) {
        res = Py_BuildValue("s",getStringBuf(spec->prep));
    }
    return res;
}

static PyObject * 
spec_get_build(specObject * s) 
{
    rpmSpec spec = specFromSpec(s);
    PyObject *res = NULL;
    if (spec != NULL && spec->build) {
        res = Py_BuildValue("s",getStringBuf(spec->build));
    }
    return res;
}

static PyObject * 
spec_get_install(specObject * s) 
{
    rpmSpec spec = specFromSpec(s);
    PyObject *res = NULL;
    if (spec != NULL && spec->install) {
        res = Py_BuildValue("s",getStringBuf(spec->install));
    }
    return res;
}

static PyObject * 
spec_get_clean(specObject * s) 
{
    rpmSpec spec = specFromSpec(s);
    PyObject *res = NULL;
    if (spec != NULL && spec->clean) {
        res = Py_BuildValue("s",getStringBuf(spec->clean));
    }
    return res;
}

static PyObject *
spec_get_sources(specObject *s)
{
    struct Source * source;
    PyObject *sourceList, *srcUrl;
    rpmSpec spec;
    char * fullSource;

    sourceList = PyList_New(0);
    spec = specFromSpec(s);
    if ( spec != NULL) {
        source = spec->sources;

         while (source != NULL) {
            fullSource = source->fullSource;
            srcUrl = Py_BuildValue("(sii)", fullSource, source->num, source->flags);
            PyList_Append(sourceList, srcUrl);
            source=source->next;
        } 

        return PyList_AsTuple(sourceList);
    }
    else {
        return NULL;
    }

}

static char spec_doc[] = "RPM Spec file object";

static PyMethodDef spec_Spec_methods[] = {
    {"sources",   (PyCFunction) spec_get_sources, METH_VARARGS,  NULL },
    {"prep",   (PyCFunction) spec_get_prep, METH_VARARGS,  NULL },
    {"build",   (PyCFunction) spec_get_build, METH_VARARGS,  NULL },
    {"install",   (PyCFunction) spec_get_install, METH_VARARGS,  NULL },
    {"clean",   (PyCFunction) spec_get_clean, METH_VARARGS,  NULL },
    {"buildRoot",   (PyCFunction) spec_get_buildroot, METH_VARARGS,  NULL },
    {NULL}  /* Sentinel */
};

static PyObject *spec_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
    rpmts ts = NULL;
    const char * specfile;
    rpmSpec spec = NULL;
    char * buildRoot = NULL;
    int recursing = 0;
    char * passPhrase = "";
    char *cookie = NULL;
    int anyarch = 1;
    int force = 1;
    char * kwlist[] = {"specfile", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:spec_new", kwlist,
				     &specfile))
	return NULL;

    /*
     * Just how hysterical can you get? We need to create a transaction
     * set to get back the results from parseSpec()...
     */
    ts = rpmtsCreate();
    if (parseSpec(ts, specfile,"/", buildRoot,recursing, passPhrase,
             	  cookie, anyarch, force) == 0) {
	spec = rpmtsSpec(ts);
    } else {
	 PyErr_SetString(pyrpmbError, "can't parse specfile\n");
    }
    rpmtsFree(ts);

    return spec ? spec_Wrap(subtype, spec) : NULL;
}

PyTypeObject spec_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                         /*ob_size*/
    "rpm.spec",               /*tp_name*/
    sizeof(specObject),        /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor) spec_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /*tp_flags*/
    spec_doc,                  /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    spec_Spec_methods,         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    spec_new,                  /* tp_new */
    0,                         /* tp_free */
    0,                         /* tp_is_gc */
};

rpmSpec specFromSpec(specObject *s) 
{
    return s->spec;
}

PyObject *
spec_Wrap(PyTypeObject *subtype, rpmSpec spec) 
{
    specObject * s = (specObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return PyErr_NoMemory();

    s->spec = spec; 
    return (PyObject *) s;
}
