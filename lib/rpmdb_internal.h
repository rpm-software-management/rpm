#ifndef H_RPMDB_INTERNAL
#define H_RPMDB_INTERNAL

#include <assert.h>
#include <db.h>

#include <rpm/rpmsw.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>
#include "lib/backend/dbi.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmdb
 * Add package header to rpm database and indices.
 * @param db		rpm database
 * @param h		header
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbAdd(rpmdb db, Header h);

/** \ingroup rpmdb
 * Remove package header from rpm database and indices.
 * @param db		rpm database
 * @param h		header
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbRemove(rpmdb db, Header h);

/** \ingroup rpmdb
 * Return rpmdb home directory (depending on chroot state)
 * param db		rpmdb handle
 * return		db home directory (or NULL on error)
 */
RPM_GNUC_INTERNAL
const char *rpmdbHome(rpmdb db);

/** \ingroup rpmdb
 * Return database iterator.
 * @param mi		rpm database iterator
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		0 on success
 */
int rpmdbExtendIterator(rpmdbMatchIterator mi,
			const void * keyp, size_t keylen);

/** \ingroup rpmdb
 * sort the iterator by (recnum, filenum)
 * Return database iterator.
 * @param mi		rpm database iterator
 */
void rpmdbSortIterator(rpmdbMatchIterator mi);

#ifndef __APPLE__
/**
 *  * Mergesort, same arguments as qsort(2).
 *   */
RPM_GNUC_INTERNAL
int mergesort(void *base, size_t nmemb, size_t size,
                int (*cmp) (const void *, const void *));
#else
/* mergesort is defined in stdlib.h on Mac OS X */
#endif /* __APPLE__ */

#endif
