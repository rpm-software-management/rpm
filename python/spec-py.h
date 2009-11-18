#ifndef RPMPYTHON_SPEC
#define RPMPYTHON_SPEC

#include <rpm/rpmbuild.h>

typedef struct specObject_s specObject;

extern PyTypeObject spec_Type;

#define specObject_Check(v)	((v)->ob_type == &spec_Type)

PyObject * spec_Wrap(PyTypeObject *subtype, rpmSpec spec);

#endif /* RPMPYTHON_SPEC */
