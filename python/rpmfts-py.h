#ifndef H_RPMFTS_PY
#define H_RPMFTS_PY

/** \ingroup py_c  
 * \file python/rpmfts-py.h
 */

#include <fts.h>

typedef struct rpmftsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    PyObject *callbacks;

/*@null@*/
    const char ** roots;
    int		options;
    int		ignore;

/*@null@*/
    int	(*compare) (const void *, const void *);

/*@null@*/
    FTS *	ftsp;
/*@null@*/
    FTSENT *	fts;
    int         active;
} rpmftsObject;

/*@unchecked@*/
extern PyTypeObject rpmfts_Type;

#endif
