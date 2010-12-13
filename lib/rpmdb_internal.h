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
 * Reference a database instance.
 * @param db		rpm database
 * @return		new rpm database reference
 */
rpmdb rpmdbLink(rpmdb db);

/** \ingroup rpmdb
 * Open rpm database.
 * @param prefix	path to top of install tree
 * @retval dbp		address of rpm database
 * @param mode		open(2) flags:  O_RDWR or O_RDONLY (O_CREAT also)
 * @param perms		database permissions
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbOpen (const char * prefix, rpmdb * dbp, int mode, int perms);

/** \ingroup rpmdb
 * Initialize database.
 * @param prefix	path to top of install tree
 * @param perms		database permissions
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbInit(const char * prefix, int perms);

/** \ingroup rpmdb
 * Close all database indices and free rpmdb.
 * @param db		rpm database
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbClose (rpmdb db);

/** \ingroup rpmdb
 * Sync all database indices.
 * @param db		rpm database
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbSync (rpmdb db);

/** \ingroup rpmdb
 * Rebuild database indices from package headers.
 * @param prefix	path to top of install tree
 * @param ts		transaction set (or NULL)
 * @param (*hdrchk)	headerCheck() vector (or NULL)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbRebuild(const char * prefix, rpmts ts,
		rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, char ** msg));

/** \ingroup rpmdb
 * Verify database components.
 * @param prefix	path to top of install tree
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbVerify(const char * prefix);

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
 * @param hdrNum	package instance number in database
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdbRemove(rpmdb db, unsigned int hdrNum);

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

/** \ingroup rpmdb
 * Remove items from set of package instances to iterate.
 * @note Sorted hdrNums are always passed in rpmlib.
 * @param mi		rpm database iterator
 * @param hdrNums	hash of package instances
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbPruneIterator(rpmdbMatchIterator mi, intHash hdrNums);

/** \ingroup rpmdb
 * Create a new, empty match iterator (for purposes of extending it
 * through other means)
 * @param db		rpm database
 * @param dbitag	database index tag
 * @return		empty match iterator
 */
RPM_GNUC_INTERNAL
rpmdbMatchIterator rpmdbNewIterator(rpmdb db, rpmDbiTagVal dbitag);

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

#ifdef __cplusplus
}
#endif

#endif
