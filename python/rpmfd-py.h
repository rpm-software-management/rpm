#ifndef H_RPMFD_PY
#define H_RPMFD_PY

#include <rpm/rpmio.h>

typedef struct rpmfdObject_s rpmfdObject;

extern PyTypeObject* rpmfd_Type;
extern PyType_Spec rpmfd_Type_Spec;

#define rpmfdObject_Check(v)	((v)->ob_type == rpmfd_Type)

FD_t rpmfdGetFd(rpmfdObject *fdo);

int rpmfdFromPyObject(PyObject *obj, rpmfdObject **fdop);


#endif
