#ifndef H_RPMRC_PY
#define H_RPMRC_PY

/** \ingroup python
 * \file python/rpmrc-py.h
 */

/** \ingroup python
 */
typedef struct rpmrcObject_s rpmrcObject;

/** \ingroup python
 */
struct rpmrcObject_s {
    PyDictObject dict;
    int state;
} ;

/*@unchecked@*/
extern PyTypeObject rpmrc_Type;

PyObject * rpmrc_AddMacro(PyObject * self, PyObject * args)
	/*@globals rpmGlobalMacroContext, _Py_NoneStruct @*/
	/*@modifies rpmGlobalMacroContext, _Py_NoneStruct @*/;
PyObject * rpmrc_DelMacro(PyObject * self, PyObject * args)
	/*@globals rpmGlobalMacroContext, _Py_NoneStruct @*/
	/*@modifies rpmGlobalMacroContext, _Py_NoneStruct @*/;

PyObject * rpmrc_Create(PyObject * self, PyObject * args, PyObject * kwds)
	/*@*/;

#endif
