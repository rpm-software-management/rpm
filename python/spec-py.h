#ifndef RPMPYTHON_SPEC
#define RPMPYTHON_SPEC

#include <rpm/rpmbuild.h>

/** \ingroup py_c
 * \file python/spec-py.h
 */

typedef struct specObject_s specObject;

extern PyTypeObject spec_Type;

/**
 */
rpmSpec specFromSpec(specObject * spec);

/**
 */
PyObject * spec_Wrap(rpmSpec spec);

#endif /* RPMPYTHON_SPEC */
