#ifndef RPMPYTHON_SPEC
#define RPMPYTHON_SPEC

#include <rpmbuild.h>

/** \ingroup py_c
 * \file python/spec-py.h
 */

typedef struct specObject_s {
    PyObject_HEAD
    /*type specific fields */
    rpmSpec spec;
} specObject;

extern PyTypeObject spec_Type;

/**
 */
rpmSpec specFromSpec(specObject * spec);

/**
 */
specObject * spec_Wrap(rpmSpec spec);

#endif /* RPMPYTHON_SPEC */
