#ifndef H_RPMFD_PY
#define H_RPMFD_PY

/** \ingroup py_c
 * \file python/rpmfd-py.h
 */

typedef struct rpmfdObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
/*@relnull@*/
    FD_t	fd;
} rpmfdObject;

/*@unchecked@*/
extern PyTypeObject rpmfd_Type;

/*@null@*/
rpmfdObject * rpmfd_Wrap(FD_t fd)
	/*@*/;

#endif
