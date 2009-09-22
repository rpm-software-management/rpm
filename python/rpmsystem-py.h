#ifndef H_SYSTEM_PYTHON
#define	H_SYSTEM_PYTHON

#if defined(__APPLE__)
#include <sys/types.h>
#endif

#include "Python.h"

#include "../system.h"

#if ((PY_MAJOR_VERSION << 8) | (PY_MINOR_VERSION << 0)) < 0x0205
typedef ssize_t Py_ssize_t;
typedef Py_ssize_t (*lenfunc)(PyObject *);
#endif  

#endif	/* H_SYSTEM_PYTHON */
