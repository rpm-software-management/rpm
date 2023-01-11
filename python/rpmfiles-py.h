#ifndef H_RPMFILES_PY
#define H_RPMFILES_PY

#include <rpm/rpmfiles.h>

typedef struct rpmfileObject_s rpmfileObject;
typedef struct rpmfilesObject_s rpmfilesObject;

extern PyTypeObject* rpmfile_Type;
extern PyType_Spec rpmfile_Type_Spec;
extern PyTypeObject* rpmfiles_Type;
extern PyType_Spec rpmfiles_Type_Spec;

#define rpmfileObject_Check(v)	((v)->ob_type == rpmfile_Type)
#define rpmfilesObject_Check(v)	((v)->ob_type == rpmfiles_Type)

PyObject * rpmfile_Wrap(rpmfiles files, int ix);
PyObject * rpmfiles_Wrap(PyTypeObject *subtype, rpmfiles files);

#endif
