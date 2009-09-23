#ifndef H_RPMFD_PY
#define H_RPMFD_PY

#include <rpm/rpmio.h>

int rpmFdFromPyObject(PyObject *obj, FD_t *fdp);

#endif
