#ifndef H_RPMTE_PY
#define H_RPMTE_PY

#include "rpmte.h"

/** \ingroup python
 * \file python/rpmte-py.h
 */

typedef struct rpmteObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    rpmte	te;
} rpmteObject;

/*@unchecked@*/
extern PyTypeObject rpmte_Type;

rpmteObject * rpmte_Wrap(rpmte te)
	/*@*/;

#endif
