#ifndef H_RNG_PY
#define H_RNG_PY

/** \ingroup py_c  
 * \file python/rng-py.h
 */
#include "beecrypt/beecrypt.h"
#include "beecrypt/mpprime.h"

/**
 */
typedef struct rngObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    randomGeneratorContext rngc;
    mpbarrett b;
} rngObject;

/**
 */
/*@unchecked@*/
extern PyTypeObject rng_Type;
#define is_rng(o)	((o)->ob_type == &rng_Type)

#endif
