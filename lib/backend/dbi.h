#ifndef _DBI_H
#define _DBI_H

enum rpmdbFlags {
    RPMDB_FLAG_JUSTCHECK	= (1 << 0),
    RPMDB_FLAG_REBUILD		= (1 << 1),
    RPMDB_FLAG_VERIFYONLY	= (1 << 2),
};

typedef struct _dbiIndex * dbiIndex;

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
    unsigned char * db_bits;	/*!< package instance bit mask. */
    int		db_nbits;	/*!< no. of bits in mask. */
    rpmdb	db_next;
    int		db_opens;
    void *	db_dbenv;	/*!< Berkeley DB_ENV handle. */
    int		db_ndbi;	/*!< No. of tag indices. */
    dbiIndex * _dbi;		/*!< Tag indices. */
    int		db_buildindex;	/*!< Index rebuild indicator */

    struct rpmop_s db_getops;
    struct rpmop_s db_putops;
    struct rpmop_s db_delops;

    int nrefs;			/*!< Reference count. */
};

/* Type of the dbi, also serves as the join key size */
typedef enum dbiIndexType_e {
    DBI_PRIMARY 	= (1 * sizeof(int32_t)),
    DBI_SECONDARY	= (2 * sizeof(int32_t)),
} dbiIndexType;

/** \ingroup dbi
 * Describes an index database (implemented on Berkeley db functionality).
 */
struct _dbiIndex {
    const char * dbi_file;	/*!< file component of path */

    int	dbi_eflags;		/*!< dbenv->open flags */
    int	dbi_oflags;		/*!< db->open flags */

    DBTYPE dbi_dbtype;		/*!< db index type */

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
    dbiIndexType dbi_type;	/*! Type of dbi (primary / index) */

    DB * dbi_db;		/*!< Berkeley DB * handle */
};

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup dbi
 * Return new configured index database handle instance.
 * @param rdb		rpm database
 * @param rpmtag	rpm tag
 * @return		index database handle
 */
RPM_GNUC_INTERNAL
dbiIndex dbiNew(rpmdb rdb, rpmTag rpmtag);

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
 * @param flags
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiOpen(rpmdb rdb, rpmTag rpmtag, dbiIndex * dbip, int flags);


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
RPM_GNUC_INTERNAL
int dbiCount(dbiIndex dbi, DBC * dbcursor, unsigned int * countp,
	     unsigned int flags);

/** \ingroup dbi
 * Close index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiClose(dbiIndex dbi, unsigned int flags);

/** \ingroup dbi
 * Flush pending operations to disk.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiSync (dbiIndex dbi, unsigned int flags);

/** \ingroup dbi
 * Verify (and close) index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiVerify(dbiIndex dbi, unsigned int flags);

/** \ingroup dbi
 * Is database byte swapped?
 * @param dbi		index database handle
 * @return		0 same order, 1 swapped order
 */
RPM_GNUC_INTERNAL
int dbiByteSwapped(dbiIndex dbi);

/** \ingroup dbi
 * Type of dbi (primary data / index)
 * @param dbi		index database handle
 * @return		type of dbi
 */
RPM_GNUC_INTERNAL
dbiIndexType dbiType(dbiIndex dbi);

#ifdef __cplusplus
}
#endif

#endif /* _DBI_H */
