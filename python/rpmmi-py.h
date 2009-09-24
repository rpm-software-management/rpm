#ifndef H_RPMMI_PY
#define H_RPMMI_PY

typedef struct rpmmiObject_s rpmmiObject;

extern PyTypeObject rpmmi_Type;

#define rpmmiObject_Check(v)	((v)->ob_type == &rpmmi_Type)

PyObject * rpmmi_Wrap(PyTypeObject *subtype, rpmdbMatchIterator mi, PyObject *s);

#endif
