#include "rpmsystem-py.h"

#include "rpmts-py.h"
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
 *  rpm.rpmPushMacro("_topdir","/path/to/topdir")
 *  s=rpm.spec("foo.spec")
 *  print(s.prep)
 * \endcode
 *
 *  Macros set using add macro will be used allowing testing of conditional builds
 *
 */

static PyObject *makeHeader(Header h)
{
    return hdr_Wrap(hdr_Type, headerLink(h));
}

struct specPkgObject_s {
    PyObject_HEAD
    /*type specific fields */
    rpmSpecPkg pkg;
    specObject *source_spec;
};

static void specPkg_dealloc(specPkgObject * s)
{
    Py_DECREF(s->source_spec);
}

static PyObject *pkgGetSection(rpmSpecPkg pkg, int section)
{
    char *sect = rpmSpecPkgGetSection(pkg, section);
    if (sect != NULL) {
        PyObject *ps = utf8FromString(sect);
        free(sect);
        if (ps != NULL)
            return ps;
    }
    Py_RETURN_NONE;
}

static char specPkg_doc[] =
"Package data parsed from spec file.";

static PyObject * specpkg_get_header(specPkgObject *s, void *closure)
{
    return makeHeader(rpmSpecPkgHeader(s->pkg));
}

static PyObject * specpkg_get_fileFile(specPkgObject *s, void *closure)
{
    return  pkgGetSection(s->pkg, RPMBUILD_FILE_FILE);
}

static PyObject * specpkg_get_fileList(specPkgObject *s, void *closure)
{
    return  pkgGetSection(s->pkg, RPMBUILD_FILE_LIST);
}

static PyObject * specpkg_get_policyList(specPkgObject *s, void *closure)
{
    return  pkgGetSection(s->pkg, RPMBUILD_POLICY);
}

static PyGetSetDef specpkg_getseters[] = {
    { "header",     (getter) specpkg_get_header,     NULL, NULL },
    { "fileFile",   (getter) specpkg_get_fileFile,   NULL, NULL },
    { "fileList",   (getter) specpkg_get_fileList,   NULL, NULL },
    { "policyList", (getter) specpkg_get_policyList, NULL, NULL },
    { NULL }   /* sentinel */
};

static PyObject *disabled_new(PyTypeObject *type,
                              PyObject *args, PyObject *kwds)
{
    PyErr_SetString(PyExc_TypeError,
                    "TypeError: cannot create 'rpm.specpkg' instances");
    return NULL;
}

static PyType_Slot specPkg_Type_Slots[] = {
    {Py_tp_new, disabled_new},
    {Py_tp_dealloc, specPkg_dealloc},
    {Py_tp_getattro, PyObject_GenericGetAttr},
    {Py_tp_setattro, PyObject_GenericSetAttr},
    {Py_tp_doc, specPkg_doc},
    {Py_tp_getset, specpkg_getseters},
    {0, NULL},
};

PyTypeObject* specPkg_Type;
PyType_Spec specPkg_Type_Spec = {
    .name = "rpm.specpkg",
    .basicsize = sizeof(specPkgObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = specPkg_Type_Slots,
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
    freefunc free = PyType_GetSlot(Py_TYPE(s), Py_tp_free);
    free(s);
}

static PyObject * getSection(rpmSpec spec, int section)
{
    const char *sect = rpmSpecGetSection(spec, section);
    if (sect) {
	return utf8FromString(sect);
    }
    Py_RETURN_NONE;
}

static PyObject *
spec_get_parsed(specObject * s, void *closure)
{
    return getSection(s->spec, RPMBUILD_NONE);
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

static PyObject * spec_get_check(specObject * s, void *closure)
{
    return getSection(s->spec, RPMBUILD_CHECK);
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
	PyObject *srcUrl = Py_BuildValue("(Nii)",
				utf8FromString(rpmSpecSrcFilename(source, 1)),
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
	PyObject *po = specPkg_Wrap(specPkg_Type, pkg, s);
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
    {"parsed",    (getter) spec_get_parsed, NULL, NULL},
    {"prep",   (getter) spec_get_prep, NULL, NULL },
    {"build",   (getter) spec_get_build, NULL, NULL },
    {"install",   (getter) spec_get_install, NULL, NULL },
    {"check",	(getter) spec_get_check, NULL, NULL },
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

static struct PyMethodDef spec_methods[] = {
    { NULL, NULL }
};

static PyType_Slot spec_Type_Slots[] = {
    {Py_tp_dealloc, spec_dealloc},
    {Py_tp_doc, spec_doc},
    {Py_tp_methods, spec_methods},
    {Py_tp_getset, spec_getseters},
    {Py_tp_new, spec_new},
    {0, NULL},
};

PyTypeObject* spec_Type;
PyType_Spec spec_Type_Spec = {
    .name = "rpm.spec",
    .basicsize = sizeof(specObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_IMMUTABLETYPE,
    .slots = spec_Type_Slots,
};

PyObject *
spec_Wrap(PyTypeObject *subtype, rpmSpec spec) 
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    specObject *s = (specObject *)subtype_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->spec = spec; 
    return (PyObject *) s;
}

PyObject * specPkg_Wrap(PyTypeObject *subtype, rpmSpecPkg pkg, specObject *source)
{
    allocfunc subtype_alloc = (allocfunc)PyType_GetSlot(subtype, Py_tp_alloc);
    specPkgObject *s = (specPkgObject *)subtype_alloc(subtype, 0);
    if (s == NULL) return NULL;

    s->pkg = pkg;
    s->source_spec = source;
    Py_INCREF(s->source_spec);
    return (PyObject *) s;
}

