#ifndef _RPMSTRPOOL_H
#define _RPMSTRPOOL_H

#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* XXX TODO: properly document... */

/* create a new string pool */
rpmstrPool rpmstrPoolCreate(void);

/* destroy a string pool (refcounted) */
rpmstrPool rpmstrPoolFree(rpmstrPool sidpool);

/* reference a string pool */
rpmstrPool rpmstrPoolLink(rpmstrPool sidpool);

/* freeze pool to free memory (keephash required for string -> id lookups) */
void rpmstrPoolFreeze(rpmstrPool sidpool, int keephash);

/* unfreeze pool (ie recreate hash table) */
void rpmstrPoolUnfreeze(rpmstrPool sidpool);

/* get the id of a string, optionally storing if not already present */
rpmsid rpmstrPoolId(rpmstrPool sidpool, const char *s, int create);

/* get the id of a string + length, optionally storing if not already present */
rpmsid rpmstrPoolIdn(rpmstrPool sidpool, const char *s, size_t slen, int create);

/* get a string by its id */
const char * rpmstrPoolStr(rpmstrPool sidpool, rpmsid sid);

/* get a strings length by its id (in constant time) */
size_t rpmstrPoolStrlen(rpmstrPool pool, rpmsid sid);

/* pool string equality comparison (constant time if within same pool) */
int rpmstrPoolStreq(rpmstrPool poolA, rpmsid sidA,
                    rpmstrPool poolB, rpmsid sidB);

/* get number of unique strings in pool */
rpmsid rpmstrPoolNumStr(rpmstrPool pool);

#ifdef __cplusplus
}
#endif

#endif /* _RPMSIDPOOL_H */
