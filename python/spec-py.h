#ifndef RPMPYTHON_SPEC
#define RPMPYTHON_SPEC

#include <rpmbuild.h>

/** \ingroup py_c
 * \file python/spec-py.h
 */

typedef struct specObject_s {
    PyObject_HEAD
    /*type specific fields */
/*@null@*/
    Spec spec;
} specObject;

extern PyTypeObject spec_Type;

/**
 */
/*@null@*/
Spec specFromSpec(specObject * spec)
/*@*/;

/**
 */
/*@null@*/
specObject * spec_Wrap(Spec spec)
/*@*/;

#endif /* RPMPYTHON_SPEC */
