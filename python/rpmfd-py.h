#ifndef H_RPMFD_PY
#define H_RPMFD_PY

#include <rpm/rpmio.h>

typedef struct rpmfdObject_s rpmfdObject;

extern PyTypeObject rpmfd_Type;

int rpmFdFromPyObject(PyObject *obj, FD_t *fdp);


#endif
