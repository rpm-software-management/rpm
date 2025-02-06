#ifndef H_RPMII_PY
#define H_RPMII_PY

typedef struct rpmiiObject_s rpmiiObject;
extern PyType_Spec rpmii_Type_Spec;

#define rpmiiObject_Check(v)	((v)->ob_type == modstate->rpmii_Type)

PyObject * rpmii_Wrap(PyTypeObject *subtype, rpmdbIndexIterator ii, PyObject *s);

#endif
