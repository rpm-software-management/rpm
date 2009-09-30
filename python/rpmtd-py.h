#ifndef H_RPMTD_PY
#define H_RPMTD_PY

typedef struct rpmtdObject_s rpmtdObject;

extern PyTypeObject rpmtd_Type;

PyObject * rpmtd_AsPyobj(rpmtd td);

#endif
