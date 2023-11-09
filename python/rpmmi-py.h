#ifndef H_RPMMI_PY
#define H_RPMMI_PY

typedef struct rpmmiObject_s rpmmiObject;
extern PyType_Spec rpmmi_Type_Spec;

PyObject * rpmmi_Wrap(PyTypeObject *subtype, rpmdbMatchIterator mi, PyObject *s);

#endif
