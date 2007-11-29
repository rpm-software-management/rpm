#ifndef H_RPMMACRO_PY
#define H_RPMMACRO_PY

/** \ingroup py_c
 * \file python/rpmmacro-py.h
 */

/** \ingroup py_c
 */

PyObject * rpmmacro_AddMacro(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmmacro_DelMacro(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * rpmmacro_ExpandMacro(PyObject * self, PyObject * args, PyObject * kwds);

#endif
