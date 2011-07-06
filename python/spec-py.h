#ifndef RPMPYTHON_SPEC
#define RPMPYTHON_SPEC

#include <rpm/rpmbuild.h>

typedef struct specPkgObject_s specPkgObject;
typedef struct specObject_s specObject;

extern PyTypeObject spec_Type;
extern PyTypeObject specPkg_Type;

#define specObject_Check(v)	((v)->ob_type == &spec_Type)
#define specPkgObject_Check(v)	((v)->ob_type == &specPkg_Type)

PyObject * spec_Wrap(PyTypeObject *subtype, rpmSpec spec);
PyObject * specPkg_Wrap(PyTypeObject *subtype, rpmSpecPkg pkg);

#endif /* RPMPYTHON_SPEC */
