#ifndef H_RPMFI_PY
#define H_RPMFI_PY

#include <rpm/rpmfi.h>

typedef struct rpmfiObject_s rpmfiObject;

extern PyTypeObject rpmfi_Type;

#define rpmfiObject_Check(v)	((v)->ob_type == &rpmfi_Type)

rpmfi fiFromFi(rpmfiObject * fi);

PyObject * rpmfi_Wrap(PyTypeObject *subtype, rpmfi fi);

#endif
