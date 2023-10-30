#ifndef H_SYSTEM_PYTHON
#define	H_SYSTEM_PYTHON

#if defined(__APPLE__)
#include <sys/types.h>
#endif

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

typedef struct {
    PyTypeObject* hdr_Type;
    PyTypeObject* rpmarchive_Type;
    PyTypeObject* rpmds_Type;
    PyTypeObject* rpmfd_Type;
    PyTypeObject* rpmfile_Type;
    PyTypeObject* rpmfiles_Type;
    PyTypeObject* rpmii_Type;
    PyTypeObject* rpmKeyring_Type;
    PyTypeObject* rpmPubkey_Type;
    PyTypeObject* rpmmi_Type;
    PyTypeObject* rpmProblem_Type;
    PyTypeObject* rpmstrPool_Type;
    PyTypeObject* rpmte_Type;
    PyTypeObject* rpmts_Type;
    PyTypeObject* rpmver_Type;
    PyTypeObject* spec_Type;
    PyTypeObject* specPkg_Type;

    PyObject* pyrpmError;
} rpmmodule_state_t;

extern rpmmodule_state_t *modstate;

/* Replacement for Py_Version from Python 3.11+
 * version of the Python runtime, packed into an int.
 * For simplicity this only contains "x.y.z", the final byte is 0.
 */
extern unsigned long python_version;

PyObject * utf8FromString(const char *s);

#ifndef Py_TPFLAGS_IMMUTABLETYPE
/*
 * Flag was added in Python 3.10.
 * If the current Python doesn't have it, rpm's type objects will be mutable.
 */
#define Py_TPFLAGS_IMMUTABLETYPE 0
#endif

#endif	/* H_SYSTEM_PYTHON */
