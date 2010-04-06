#ifndef H_RPMDB_INTERNAL
#define H_RPMDB_INTERNAL

#include <assert.h>
#include <db.h>

#include <rpm/rpmsw.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>
#include "lib/backend/dbi.h"

/** \ingroup rpmdb
 * Describes the collection of index databases used by rpm.
 */
struct rpmdb_s {
    char 	* db_root;/*!< path prefix */
    char 	* db_home;/*!< directory path */
    char	* db_fullpath;	/*!< full db path including prefix */
    int		db_flags;
    int		db_mode;	/*!< open mode */
    int		db_perms;	/*!< open permissions */
    int		db_api;		/*!< Berkeley API type */
    int		db_remove_env;
    int		db_chrootDone;	/*!< If chroot(2) done, ignore db_root. */
    int		db_mkdirDone;	/*!< Has db_home been created? */
    unsigned char * db_bits;	/*!< package instance bit mask. */
    int		db_nbits;	/*!< no. of bits in mask. */
    rpmdb	db_next;
    int		db_opens;
    void *	db_dbenv;	/*!< Berkeley DB_ENV handle. */
    int		db_ndbi;	/*!< No. of tag indices. */
    dbiIndex * _dbi;		/*!< Tag indices. */

    struct rpmop_s db_getops;
    struct rpmop_s db_putops;
    struct rpmop_s db_delops;

    int nrefs;			/*!< Reference count. */
};

#ifdef __cplusplus
extern "C" {
#endif

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
