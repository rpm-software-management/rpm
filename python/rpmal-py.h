#ifndef H_RPMAL_PY
#define H_RPMAL_PY

/** \ingroup python
 * \file python/rpmal-py.h
 */

typedef struct rpmalObject_s {
    PyObject_HEAD
    rpmal	al;
} rpmalObject;

extern PyTypeObject rpmal_Type;

rpmalObject * rpmal_Wrap(rpmal al);

#endif
