#ifndef H_RPMSTRPOOL_PY
#define H_RPMSTRPOOL_PY

#include <rpm/rpmtypes.h>

typedef struct rpmstrPoolObject_s rpmstrPoolObject;
extern PyType_Spec rpmstrPool_Type_Spec;

PyObject * rpmstrPool_Wrap(PyTypeObject *subtype, rpmstrPool pool);

int poolFromPyObject(rpmmodule_state_t *modstate, PyObject *item, rpmstrPool *pool);

#endif
