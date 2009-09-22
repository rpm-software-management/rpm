#ifndef H_RPMMI_PY
#define H_RPMMI_PY

typedef struct rpmmiObject_s rpmmiObject;

extern PyTypeObject rpmmi_Type;

PyObject * rpmmi_Wrap(rpmdbMatchIterator mi, PyObject *s);

#endif
