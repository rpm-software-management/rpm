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

/* Header objects are in another module, some hoop jumping required... */
static PyObject *makeHeader(Header h)
{
    PyObject *rpmmod = PyImport_ImportModuleNoBlock("rpm");
    if (rpmmod == NULL) return NULL;

    PyObject *ptr = PyCObject_FromVoidPtr(h, NULL);
    PyObject *hdr = PyObject_CallMethod(rpmmod, "hdr", "(O)", ptr);
    Py_XDECREF(ptr);
    Py_XDECREF(rpmmod);
    return hdr;
}

struct specPkgObject_s {
    PyObject_HEAD
    /*type specific fields */
    Package pkg;
};

static char specPkg_doc[] =
"";

static PyObject * specpkg_get_header(specPkgObject *s, void *closure)
{
    Package pkg = s->pkg;
    return makeHeader(pkg->header);
}

static PyGetSetDef specpkg_getseters[] = {
    { "header",	(getter) specpkg_get_header, NULL, NULL },
    { NULL } 	/* sentinel */
};

PyTypeObject specPkg_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.specpkg",			/* tp_name */
	sizeof(specPkgObject),		/* tp_size */
	0,				/* tp_itemsize */
	0, 				/* tp_dealloc */
	0,				/* tp_print */
	0, 				/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	specPkg_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	specpkg_getseters,		/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	0,				/* tp_new */
	0,				/* tp_free */
	0,				/* tp_is_gc */
};

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
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject * 
spec_get_buildroot(specObject * s, void *closure) 
{
    rpmSpec spec = s->spec;
    if (spec->buildRoot) {
        return Py_BuildValue("s", spec->buildRoot);
    }
    Py_RETURN_NONE;
}

static PyObject * 
spec_get_prep(specObject * s, void *closure) 
{
    rpmSpec spec = s->spec;
    if (spec->prep) {
        return Py_BuildValue("s",getStringBuf(spec->prep));
    }
    Py_RETURN_NONE;
}

static PyObject * 
spec_get_build(specObject * s, void *closure) 
{
    rpmSpec spec = s->spec;
    if (spec->build) {
        return Py_BuildValue("s",getStringBuf(spec->build));
    }
    Py_RETURN_NONE;
}

static PyObject * spec_get_install(specObject * s, void *closure) 
{
    rpmSpec spec = s->spec;
    if (spec->install) {
        return Py_BuildValue("s",getStringBuf(spec->install));
    }
    Py_RETURN_NONE;
}

static PyObject * spec_get_clean(specObject * s, void *closure) 
{
    rpmSpec spec = s->spec;
    if (spec != NULL && spec->clean) {
        return Py_BuildValue("s",getStringBuf(spec->clean));
    }
    Py_RETURN_NONE;
}

static PyObject * spec_get_sources(specObject *s, void *closure)
{
    rpmSpec spec = s->spec;
    PyObject *sourceList = PyList_New(0);
    struct Source *source;

    for (source = spec->sources; source; source = source->next) {
	PyObject *srcUrl = Py_BuildValue("(sii)", source->fullSource, 
					 source->num, source->flags);
	PyList_Append(sourceList, srcUrl);
	Py_DECREF(srcUrl);
    } 

    return sourceList;

}

static PyObject * spec_get_packages(specObject *s, void *closure)
{
    rpmSpec spec = s->spec;
    PyObject *pkgList = PyList_New(0);
    Package pkg;

    for (pkg = spec->packages; pkg; pkg = pkg->next) {
	PyObject *po = specPkg_Wrap(&specPkg_Type, pkg);
	PyList_Append(pkgList, po);
	Py_DECREF(po);
    }
    return pkgList;
}

static char spec_doc[] = "RPM Spec file object";

static PyGetSetDef spec_getseters[] = {
    {"sources",   (getter) spec_get_sources, NULL, NULL },
    {"prep",   (getter) spec_get_prep, NULL, NULL },
    {"build",   (getter) spec_get_build, NULL, NULL },
    {"install",   (getter) spec_get_install, NULL, NULL },
    {"clean",   (getter) spec_get_clean, NULL, NULL },
    {"buildRoot",   (getter) spec_get_buildroot, NULL, NULL },
    {"packages", (getter) spec_get_packages, NULL, NULL },
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
	 PyErr_SetString(PyExc_ValueError, "can't parse specfile\n");
    }
    rpmtsFree(ts);

    return spec ? spec_Wrap(subtype, spec) : NULL;
}

PyTypeObject spec_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
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
    0,			       /* tp_methods */
    0,                         /* tp_members */
    spec_getseters,            /* tp_getset */
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

PyObject *
spec_Wrap(PyTypeObject *subtype, rpmSpec spec) 
{
    specObject * s = (specObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->spec = spec; 
    return (PyObject *) s;
}

PyObject * specPkg_Wrap(PyTypeObject *subtype, Package pkg) 
{
    specPkgObject * s = (specPkgObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->pkg = pkg;
    return (PyObject *) s;
}

