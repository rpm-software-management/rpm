#ifndef H_MPW_PY
#define H_MPW_PY

/** \ingroup py_c  
 * \file python/mpw-py.h
 */
#include "beecrypt/mp.h"

/**
 */
typedef struct mpwObject_s {
    PyObject_HEAD
    int ob_size;
    mpw data[1];
} mpwObject;

/**
 */
/*@unchecked@*/
extern PyTypeObject mpw_Type;

#define	mpw_Check(_o)		PyObject_TypeCheck((_o), &mpw_Type)
#define mpw_CheckExact(_o)	((_o)->ob_type == &mpw_Type)

#define	MP_ROUND_B2W(_b)	MP_BITS_TO_WORDS((_b) + MP_WBITS - 1)

#define	MPW_SIZE(_a)	(size_t)((_a)->ob_size < 0 ? -(_a)->ob_size : (_a)->ob_size)
#define	MPW_DATA(_a)	((_a)->data)

/**
 */
mpwObject * mpw_New(int ob_size)
	/*@*/;

/**
 */
mpwObject * mpw_FromMPW(size_t size, mpw* data, int normalize)
	/*@*/;

#endif
