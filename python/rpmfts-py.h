#ifndef H_RPMFTS_PY
#define H_RPMFTS_PY

/** \ingroup python
 * \file python/rpmfts-py.h
 */

#include <fts.h>

typedef struct rpmftsObject_s {
    PyObject_HEAD
    FTS * ftsp;
    FTSENT * fts;
} rpmftsObject;

/*@unchecked@*/
extern PyTypeObject rpmfts_Type;

rpmftsObject * rpmfts_Wrap(FTSENT * ftsp)
	/*@*/;

#endif
