#ifndef H_RPMTE_PY
#define H_RPMTE_PY

#include <rpm/rpmte.h>

/** \ingroup py_c
 * \file python/rpmte-py.h
 */

typedef struct rpmteObject_s rpmteObject;

extern PyTypeObject rpmte_Type;

rpmteObject * rpmte_Wrap(rpmte te);

#endif
