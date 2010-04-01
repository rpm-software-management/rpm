#ifndef H_RPMDB_INTERNAL
#define H_RPMDB_INTERNAL

#include <assert.h>
#include <db.h>

#include <rpm/rpmsw.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>

typedef struct _dbiIndex * dbiIndex;

/** \ingroup dbi
 * Describes an index database (implemented on Berkeley db functionality).
 */
struct _dbiIndex {
    const char * dbi_file;	/*!< file component of path */

    int	dbi_ecflags;		/*!< db_env_create flags */
    int	dbi_cflags;		/*!< db_create flags */
    int	dbi_oeflags;		/*!< common (db,dbenv}->open flags */
    int	dbi_eflags;		/*!< dbenv->open flags */
    int	dbi_oflags;		/*!< db->open flags */
    int	dbi_tflags;		/*!< dbenv->txn_begin flags */

    int	dbi_type;		/*!< db index type */
    unsigned dbi_mode;		/*!< mode to use on open */
    int	dbi_perms;		/*!< file permission to use on open */

    int	dbi_verify_on_close;
    int	dbi_use_dbenv;		/*!< use db environment? */
    int	dbi_permit_dups;	/*!< permit duplicate entries? */
    int	dbi_no_fsync;		/*!< no-op fsync for db */
    int	dbi_no_dbsync;		/*!< don't call dbiSync */
    int	dbi_lockdbfd;		/*!< do fcntl lock on db fd */
    int	dbi_byteswapped;

	/* dbenv parameters */
    /* XXX db-4.3.14 adds dbenv as 1st arg. */
    int	dbi_verbose;
	/* mpool sub-system parameters */
    int	dbi_mmapsize;	/*!< (10Mb) */
    int	dbi_cachesize;	/*!< (128Kb) */
	/* dbinfo parameters */
    int	dbi_pagesize;		/*!< (fs blksize) */

    rpmdb dbi_rpmdb;		/*!< the parent rpm database */
    rpmTag dbi_rpmtag;	/*!< rpm tag used for index */
    int	dbi_jlen;		/*!< size of join key */

    DB * dbi_db;		/*!< Berkeley DB * handle */
    DB_TXN * dbi_txnid;		/*!< Bekerley DB_TXN * transaction id */
    void * dbi_stats;		/*!< Berkeley db statistics */
};

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

/* for RPM's internal use only */

/** \ingroup rpmdb
 */
enum rpmdbFlags {
	RPMDB_FLAG_JUSTCHECK	= (1 << 0),
	RPMDB_FLAG_MINIMAL	= (1 << 1),
	RPMDB_FLAG_CHROOT	= (1 << 2)
};

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup dbi
 * Return new configured index database handle instance.
 * @param rpmdb		rpm database
 * @param rpmtag	rpm tag
 * @return		index database handle
 */
RPM_GNUC_INTERNAL
dbiIndex dbiNew(rpmdb rpmdb, rpmTag rpmtag);

/** \ingroup dbi
 * Destroy index database handle instance.
 * @param dbi		index database handle
 * @return		NULL always
 */
RPM_GNUC_INTERNAL
dbiIndex dbiFree( dbiIndex dbi);

/** \ingroup dbi
 * Format dbi open flags for debugging print.
 * @param dbflags		db open flags
 * @param print_dbenv_flags	format db env flags instead?
 * @return			formatted flags (malloced)
 */
RPM_GNUC_INTERNAL
char * prDbiOpenFlags(int dbflags, int print_dbenv_flags);

/** \ingroup dbi
 * Actually open the database of the index.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @param dbiIndex	address of index database handle
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiOpenDB(rpmdb rpmdb, rpmTag rpmtag, dbiIndex * dbip);


/* FIX: vector annotations */
/** \ingroup dbi
 * Open a database cursor.
 * @param dbi		index database handle
 * @retval dbcp		returned database cursor
 * @param flags		DB_WRITECURSOR if writing, or 0
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiCopen(dbiIndex dbi, DBC ** dbcp, unsigned int flags);

/** \ingroup dbi
 * Close a database cursor.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param flags		(unused)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiCclose(dbiIndex dbi, DBC * dbcursor, unsigned int flags);

/** \ingroup dbi
 * Delete (key,data) pair(s) from index database.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->del)
 * @param key		delete key value/length/flags
 * @param data		delete data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiDel(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
	   unsigned int flags);

/** \ingroup dbi
 * Retrieve (key,data) pair from index database.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->get)
 * @param key		retrieve key value/length/flags
 * @param data		retrieve data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiGet(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
	   unsigned int flags);

/** \ingroup dbi
 * Store (key,data) pair in index database.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->put)
 * @param key		store key value/length/flags
 * @param data		store data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiPut(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
	   unsigned int flags);

/** \ingroup dbi
 * Retrieve count of (possible) duplicate items.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param countp	address of count
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiCount(dbiIndex dbi, DBC * dbcursor, unsigned int * countp,
	     unsigned int flags);

/** \ingroup dbi
 * Close index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiClose(dbiIndex dbi, unsigned int flags);

/** \ingroup dbi
 * Flush pending operations to disk.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiSync (dbiIndex dbi, unsigned int flags);


/** \ingroup dbi
 * Is database byte swapped?
 * @param dbi		index database handle
 * @return		0 same order, 1 swapped order
 */
int dbiByteSwapped(dbiIndex dbi);

/** \ingroup dbi
 * Is database byte swapped?
 * @param dbi		index database handle
 * @param flags		DB_FAST_STAT or 0
 * @return		0 on success
 */
int dbiStat(dbiIndex dbi, unsigned int flags);

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
