#ifndef H_RPMAL_PY
#define H_RPMAL_PY

#include "rpmal.h"

/** \ingroup python
 * \file python/rpmal-py.h
 */

typedef struct rpmalObject_s {
    PyObject_HEAD
    rpmal	al;
} rpmalObject;

/*@unchecked@*/
extern PyTypeObject rpmal_Type;

rpmalObject * rpmal_Wrap(rpmal al)
	/*@*/;

#endif
