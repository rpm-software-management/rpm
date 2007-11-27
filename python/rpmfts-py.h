#ifndef H_RPMFTS_PY
#define H_RPMFTS_PY

/** \ingroup py_c
 * \file python/rpmfts-py.h
 */

#include "rpmio/fts.h"

typedef struct rpmftsObject_s {
    PyObject_HEAD
    PyObject *md_dict;		/*!< to look like PyModuleObject */
    PyObject *callbacks;

    const char ** roots;
    int		options;
    int		ignore;

    int	(*compare) (const void *, const void *);

    FTS *	ftsp;
    FTSENT *	fts;
    int         active;
} rpmftsObject;

extern PyTypeObject rpmfts_Type;

#endif
