#ifndef H_SYSTEM_PYTHON
#define	H_SYSTEM_PYTHON

#if defined(__APPLE__)
#include <sys/types.h>
#endif

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

PyObject * utf8FromString(const char *s);

#ifndef Py_TPFLAGS_IMMUTABLETYPE
/*
 * Flag was added in Python 3.10.
 * If the current Python doesn't have it, rpm's type objects will be mutable.
 */
#define Py_TPFLAGS_IMMUTABLETYPE 0
#endif

#endif	/* H_SYSTEM_PYTHON */
