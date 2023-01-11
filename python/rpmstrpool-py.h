#ifndef H_RPMSTRPOOL_PY
#define H_RPMSTRPOOL_PY

#include <rpm/rpmtypes.h>

typedef struct rpmstrPoolObject_s rpmstrPoolObject;

extern PyTypeObject* rpmstrPool_Type;
extern PyType_Spec rpmstrPool_Type_Spec;

PyObject * rpmstrPool_Wrap(PyTypeObject *subtype, rpmstrPool pool);

int poolFromPyObject(PyObject *item, rpmstrPool *pool);

#endif
