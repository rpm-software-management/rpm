#ifndef H_RPMFD_PY
#define H_RPMFD_PY

/** \ingroup python
 * \file python/rpmfd-py.h
 */

typedef struct rpmfdObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    FD_t	fd;
} rpmfdObject;

/*@unchecked@*/
extern PyTypeObject rpmfd_Type;

rpmfdObject * rpmfd_Wrap(FD_t fd)
	/*@*/;

#endif
