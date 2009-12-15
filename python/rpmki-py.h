#ifndef H_RPMKI_PY
#define H_RPMKI_PY

typedef struct rpmkiObject_s rpmkiObject;

extern PyTypeObject rpmki_Type;

#define rpmkiObject_Check(v)	((v)->ob_type == &rpmki_Type)

PyObject * rpmki_Wrap(PyTypeObject *subtype, rpmdbKeyIterator ki, PyObject *s);

#endif
