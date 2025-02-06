#ifndef H_RPMFILES_PY
#define H_RPMFILES_PY

#include <rpm/rpmfiles.h>

typedef struct rpmfileObject_s rpmfileObject;
typedef struct rpmfilesObject_s rpmfilesObject;
extern PyType_Spec rpmfile_Type_Spec;
extern PyType_Spec rpmfiles_Type_Spec;

#define rpmfileObject_Check(v)	((v)->ob_type == modstate->rpmfile_Type)
#define rpmfilesObject_Check(v)	((v)->ob_type == modstate->rpmfiles_Type)

PyObject * rpmfile_Wrap(rpmfiles files, int ix);
PyObject * rpmfiles_Wrap(PyTypeObject *subtype, rpmfiles files);

#endif
