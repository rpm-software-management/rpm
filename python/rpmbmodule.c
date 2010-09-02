#include "rpmsystem-py.h"

#include "spec-py.h"

#include "debug.h"

static char rpmb__doc__[] =
"";

/*
  Do any common preliminary work before python 2 vs python 3 module creation:
*/
static int prepareInitModule(void)
{
    if (PyType_Ready(&spec_Type) < 0) return 0;
    if (PyType_Ready(&specPkg_Type) < 0) return 0;

    return 1;
}

static int initModule(PyObject *m)
{
    Py_INCREF(&spec_Type);
    PyModule_AddObject(m, "spec", (PyObject *) &spec_Type);
    Py_INCREF(&specPkg_Type);
    PyModule_AddObject(m, "specPkg", (PyObject *) &specPkg_Type);

#define REGISTER_ENUM(val) PyModule_AddIntConstant(m, #val, val)
    REGISTER_ENUM(RPMBUILD_ISSOURCE);
    REGISTER_ENUM(RPMBUILD_ISPATCH);
    REGISTER_ENUM(RPMBUILD_ISICON);
    REGISTER_ENUM(RPMBUILD_ISNO);

    REGISTER_ENUM(RPMBUILD_NONE);
    REGISTER_ENUM(RPMBUILD_PREP);
    REGISTER_ENUM(RPMBUILD_BUILD);
    REGISTER_ENUM(RPMBUILD_INSTALL);
    REGISTER_ENUM(RPMBUILD_CHECK);
    REGISTER_ENUM(RPMBUILD_CLEAN);
    REGISTER_ENUM(RPMBUILD_FILECHECK);
    REGISTER_ENUM(RPMBUILD_PACKAGESOURCE);
    REGISTER_ENUM(RPMBUILD_PACKAGEBINARY);
    REGISTER_ENUM(RPMBUILD_RMSOURCE);
    REGISTER_ENUM(RPMBUILD_RMBUILD);
    REGISTER_ENUM(RPMBUILD_RMSPEC);

    REGISTER_ENUM(RPMBUILD_PKG_NONE);
    REGISTER_ENUM(RPMBUILD_PKG_NODIRTOKENS);

    REGISTER_ENUM(RPMSPEC_NONE);
    REGISTER_ENUM(RPMSPEC_ANYARCH);
    REGISTER_ENUM(RPMSPEC_FORCE);
    REGISTER_ENUM(RPMSPEC_NOLANG);

    return 1;
}

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_rpmb",     /* m_name */
    rpmb__doc__, /* m_doc */
    0,           /* m_size */
    NULL,        /* m_methods */
    NULL,        /* m_reload */
    NULL,        /* m_traverse */
    NULL,        /* m_clear */
    NULL         /* m_free */
};

PyObject * PyInit__rpm(void);	/* XXX eliminate gcc warning */
PyObject * PyInit__rpm(void)
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
void init_rpmb(void);	/* XXX eliminate gcc warning */
void init_rpmb(void)
{
    PyObject *m;
  
    if (!prepareInitModule())
        return;

    m = Py_InitModule3("_rpmb", NULL, rpmb__doc__);
    if (m) initModule(m);
}
#endif
