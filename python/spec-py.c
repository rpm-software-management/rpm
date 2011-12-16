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

    PyObject *ptr = CAPSULE_BUILD(h, "rpm._C_Header");
    PyObject *hdr = PyObject_CallMethod(rpmmod, "hdr", "(O)", ptr);
    Py_XDECREF(ptr);
    Py_XDECREF(rpmmod);
    return hdr;
}

struct specPkgObject_s {
    PyObject_HEAD
    /*type specific fields */
    rpmSpecPkg pkg;
};

static char specPkg_doc[] =
"";

static PyObject * specpkg_get_header(specPkgObject *s, void *closure)
{
    return makeHeader(rpmSpecPkgHeader(s->pkg));
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
	s->spec=rpmSpecFree(s->spec);
    }
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject * getSection(rpmSpec spec, int section)
{
    const char *sect = rpmSpecGetSection(spec, section);
    if (sect) {
	return Py_BuildValue("s", sect);
    }
    Py_RETURN_NONE;
}

static PyObject * 
spec_get_prep(specObject * s, void *closure) 
{
    return getSection(s->spec, RPMBUILD_PREP);
}

static PyObject * 
spec_get_build(specObject * s, void *closure) 
{
    return getSection(s->spec, RPMBUILD_BUILD);
}

static PyObject * spec_get_install(specObject * s, void *closure) 
{
    return getSection(s->spec, RPMBUILD_INSTALL);
}

static PyObject * spec_get_clean(specObject * s, void *closure) 
{
    return getSection(s->spec, RPMBUILD_CLEAN);
}

static PyObject * spec_get_sources(specObject *s, void *closure)
{
    PyObject *sourceList;
    rpmSpecSrc source;

    sourceList = PyList_New(0);
    if (!sourceList) {
        return NULL;
    }

    rpmSpecSrcIter iter = rpmSpecSrcIterInit(s->spec);
    while ((source = rpmSpecSrcIterNext(iter)) != NULL) {
	PyObject *srcUrl = Py_BuildValue("(sii)",
				rpmSpecSrcFilename(source, 1),
				rpmSpecSrcNum(source),
				rpmSpecSrcFlags(source)); 
        if (!srcUrl) {
            Py_DECREF(sourceList);
            return NULL;
        }
	PyList_Append(sourceList, srcUrl);
	Py_DECREF(srcUrl);
    } 
    rpmSpecSrcIterFree(iter);

    return sourceList;

}

static PyObject * spec_get_packages(specObject *s, void *closure)
{
    rpmSpecPkg pkg;
    PyObject *pkgList;
    rpmSpecPkgIter iter;

    pkgList = PyList_New(0);
    if (!pkgList) {
        return NULL;
    }

    iter = rpmSpecPkgIterInit(s->spec);

    while ((pkg = rpmSpecPkgIterNext(iter)) != NULL) {
	PyObject *po = specPkg_Wrap(&specPkg_Type, pkg);
        if (!po) {
            rpmSpecPkgIterFree(iter);
            Py_DECREF(pkgList);
            return NULL;
        }
	PyList_Append(pkgList, po);
	Py_DECREF(po);
    }
    rpmSpecPkgIterFree(iter);
    return pkgList;
}

static PyObject * spec_get_source_header(specObject *s, void *closure)
{
    return makeHeader(rpmSpecSourceHeader(s->spec));
}

static char spec_doc[] = "RPM Spec file object";

static PyGetSetDef spec_getseters[] = {
    {"sources",   (getter) spec_get_sources, NULL, NULL },
    {"prep",   (getter) spec_get_prep, NULL, NULL },
    {"build",   (getter) spec_get_build, NULL, NULL },
    {"install",   (getter) spec_get_install, NULL, NULL },
    {"clean",   (getter) spec_get_clean, NULL, NULL },
    {"packages", (getter) spec_get_packages, NULL, NULL },
    {"sourceHeader", (getter) spec_get_source_header, NULL, NULL },
    {NULL}  /* Sentinel */
};

static PyObject *spec_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
    char * kwlist[] = {"specfile", "flags", NULL};
    const char * specfile;
    rpmSpec spec = NULL;
    /* XXX This is a dumb default but anything else breaks compatibility... */
    rpmSpecFlags flags = (RPMSPEC_ANYARCH|RPMSPEC_FORCE);

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i:spec_new", kwlist,
				     &specfile, &flags))
	return NULL;

    spec = rpmSpecParse(specfile, flags, NULL);
    if (spec == NULL) {
	PyErr_SetString(PyExc_ValueError, "can't parse specfile\n");
	return NULL;
    }

    return spec_Wrap(subtype, spec);
}

static PyObject * spec_doBuild(specObject *self, PyObject *args, PyObject *kwds)
{
    char * kwlist[] = { "buildAmount", "pkgFlags", NULL };
    struct rpmBuildArguments_s ba = { 0 };

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i|i:spec_doBuild",
			kwlist, &ba.buildAmount, &ba.pkgFlags))
	return NULL;

    return PyBool_FromLong(rpmSpecBuild(self->spec, &ba) == RPMRC_OK);
}

static struct PyMethodDef spec_methods[] = {
    { "_doBuild", (PyCFunction)spec_doBuild, METH_VARARGS|METH_KEYWORDS, NULL },
    { NULL, NULL }
};

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
    spec_methods,	       /* tp_methods */
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

PyObject * specPkg_Wrap(PyTypeObject *subtype, rpmSpecPkg pkg) 
{
    specPkgObject * s = (specPkgObject *)subtype->tp_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->pkg = pkg;
    return (PyObject *) s;
}

