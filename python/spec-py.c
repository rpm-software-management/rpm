/** \ingroup py_c
 * \file python/spec-py.c
 */

#include "system.h"

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
 *  ts=rpm.ts()
 *  s=ts.parseSpec("foo.spec")
 *  print s.prep()
 * \endcode
 *
 *  Macros set using add macro will be used allowing testing of conditional builds
 *
 */


static void 
spec_dealloc(specObject * s) 
    /*@modifies s @*/
{
        if (s->spec) {
            s->spec=freeSpec(s->spec);
        }
        PyObject_Del(s);
}

static int
spec_print(specObject * s)
{
    return 0;
}

/* XXX TODO return something sensible if spec exists but component (eg %clean)
 * does not. Possibly "" or None */

static PyObject * 
spec_get_buildroot(specObject * s) 
    /*@*/
{
    Spec spec = specFromSpec(s);
    if (spec != NULL && spec->buildRootURL) {
        return Py_BuildValue("s", spec->buildRootURL);
    }
    else {
        return NULL;
    }
}

static PyObject * 
spec_get_prep(specObject * s) 
    /*@*/
{
    Spec spec = specFromSpec(s);
    if (spec != NULL && spec->prep) {
        StringBuf sb = newStringBuf();
        sb=spec->prep;
        return Py_BuildValue("s",getStringBuf(sb));
    }
     else {
         return NULL;
     }
}

static PyObject * 
spec_get_build(specObject * s) 
    /*@*/
{
    Spec spec = specFromSpec(s);
    if (spec != NULL && spec->build) {
        StringBuf sb = newStringBuf();
        sb=spec->build;
        return Py_BuildValue("s",getStringBuf(sb));
    }
     else {
         return NULL;
     }
}

static PyObject * 
spec_get_install(specObject * s) 
    /*@*/
{
    Spec spec = specFromSpec(s);
    if (spec != NULL && spec->install) {
        StringBuf sb = newStringBuf();
        sb=spec->install;
        return Py_BuildValue("s",getStringBuf(sb));
    }
     else {
         return NULL;
     }
}

static PyObject * 
spec_get_clean(specObject * s) 
    /*@*/
{
    Spec spec = specFromSpec(s);
    if (spec != NULL && spec->clean) {
        StringBuf sb = newStringBuf();
        sb=spec->clean;
        return Py_BuildValue("s",getStringBuf(sb));
    }
     else {
         return NULL;
     }
}

static PyObject *
spec_get_sources(specObject *s)
    /*@*/
{
    struct Source * source;
    PyObject *sourceList, *srcUrl;
    Spec spec;
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

/**
 */
 /*@unchecked@*/ /*@observer@*/
static char spec_doc[] = "RPM Spec file object";

/*@-fullinitblock@*/
/*@unchecked@*/ /*@observer@*/
static PyMethodDef spec_Spec_methods[] = {
    {"sources",   (PyCFunction) spec_get_sources, METH_VARARGS,  NULL },
    {"prep",   (PyCFunction) spec_get_prep, METH_VARARGS,  NULL },
    {"build",   (PyCFunction) spec_get_build, METH_VARARGS,  NULL },
    {"install",   (PyCFunction) spec_get_install, METH_VARARGS,  NULL },
    {"clean",   (PyCFunction) spec_get_clean, METH_VARARGS,  NULL },
    {"buildRoot",   (PyCFunction) spec_get_buildroot, METH_VARARGS,  NULL },
    {NULL}  /* Sentinel */
};
/*@=fullinitblock@*/

/*@-fullinitblock@*/
PyTypeObject spec_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                         /*ob_size*/
    "rpm.spec",               /*tp_name*/
    sizeof(specObject),        /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor) spec_dealloc, /*tp_dealloc*/
    (printfunc) spec_print,    /*tp_print*/
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
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
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
    0,                         /* tp_new */
    0,                         /* tp_free */
    0,                         /* tp_is_gc */
};
/*@=fullinitblock@*/

Spec specFromSpec(specObject *s) 
{
    return s->spec;
}

specObject *
spec_Wrap(Spec spec) 
{
    specObject * s = PyObject_New(specObject, &spec_Type);
    if (s == NULL)
        return NULL;
    s->spec = spec; 
    return s;
}
