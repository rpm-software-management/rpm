/** \ingroup py_c
 * \file python/system.h
 */

#ifndef H_SYSTEM_PYTHON
#define	H_SYSTEM_PYTHON

#if defined(__APPLE__)
#include <sys/types.h>
#endif

#include "Python.h"

#ifdef __LCLINT__
#undef  PyObject_HEAD
#define PyObject_HEAD   int _PyObjectHead;
#endif

#include "../system.h"

#endif	/* H_SYSTEM_PYTHON */
