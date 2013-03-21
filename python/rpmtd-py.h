#ifndef H_RPMTD_PY
#define H_RPMTD_PY

typedef struct rpmtdObject_s rpmtdObject;

extern PyTypeObject rpmtd_Type;

#define rpmtdObject_Check(v)	((v)->ob_type == &rpmtd_Type)

PyObject * rpmtd_ItemAsPyobj(rpmtd td, rpmTagClass tclass);
PyObject * rpmtd_AsPyobj(rpmtd td);

int rpmtdFromPyObject(PyObject *obj, rpmtd *td);

#endif
