#ifndef H_RPMTD_PY
#define H_RPMTD_PY

PyObject * rpmtd_ItemAsPyobj(rpmtd td, rpmTagClass tclass);
PyObject * rpmtd_AsPyobj(rpmtd td);

int rpmtdFromPyObject(PyObject *obj, rpmtd *td);

#endif
