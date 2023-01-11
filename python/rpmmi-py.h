#ifndef H_RPMMI_PY
#define H_RPMMI_PY

typedef struct rpmmiObject_s rpmmiObject;

extern PyTypeObject* rpmmi_Type;
extern PyType_Spec rpmmi_Type_Spec;

#define rpmmiObject_Check(v)	((v)->ob_type == rpmmi_Type)

PyObject * rpmmi_Wrap(PyTypeObject *subtype, rpmdbMatchIterator mi, PyObject *s);

#endif
