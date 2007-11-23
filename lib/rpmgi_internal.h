#ifndef _RPMGI_INTERNAL_H
#define _RPMGI_INTERNAL_H

#include <rpmgi.h>
#include "rpmio/fts.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 */
struct rpmgi_s {
    rpmts ts;			/*!< Iterator transaction set. */
    int tag;			/*!< Iterator type. */
    const void * keyp;		/*!< Iterator key. */
    size_t keylen;		/*!< Iterator key length. */

    rpmgiFlags flags;		/*!< Iterator control bits. */
    int active;			/*!< Iterator is active? */
    int i;			/*!< Element index. */
    const char * hdrPath;	/*!< Path to current iterator header. */
    Header h;			/*!< Current iterator header. */

    rpmtsi tsi;

    rpmdbMatchIterator mi;

    FD_t fd;

    ARGV_t argv;
    int argc;

    int ftsOpts;
    FTS * ftsp;
    FTSENT * fts;

    int nrefs;			/*!< Reference count. */
};

#ifdef __cplusplus
}
#endif

#endif /* _RPMGI_INTERNAL_H */
