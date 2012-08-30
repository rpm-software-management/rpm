#ifndef _DBI_H
#define _DBI_H

enum rpmdbFlags {
    RPMDB_FLAG_JUSTCHECK	= (1 << 0),
    RPMDB_FLAG_REBUILD		= (1 << 1),
    RPMDB_FLAG_VERIFYONLY	= (1 << 2),
};

typedef struct dbiIndex_s * dbiIndex;
typedef struct dbiCursor_s * dbiCursor;

struct dbConfig_s {
    int	db_mmapsize;	/*!< (10Mb) */
    int	db_cachesize;	/*!< (128Kb) */
    int	db_verbose;
    int	db_no_fsync;	/*!< no-op fsync for db */
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
    int		db_ver;		/*!< Berkeley DB version */
    struct dbChk_s * db_checked;/*!< headerCheck()'ed package instances */
    rpmdb	db_next;
    int		db_opens;
    int		db_ndbi;	/*!< No. of tag indices. */
    dbiIndex * _dbi;		/*!< Tag indices. */
    int		db_buildindex;	/*!< Index rebuild indicator */

    /* dbenv and related parameters */
    void * db_dbenv;		/*!< Berkeley DB_ENV handle. */
    struct dbConfig_s cfg;
    int db_remove_env;

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

enum dbiFlags_e {
    DBI_NONE		= 0,
    DBI_CREATED		= (1 << 0),
    DBI_RDONLY		= (1 << 1),
};

/** \ingroup dbi
 * Describes an index database (implemented on Berkeley db functionality).
 */
struct dbiIndex_s {
    const char * dbi_file;	/*!< file component of path */

    int	dbi_oflags;		/*!< db->open flags */
    int	dbi_permit_dups;	/*!< permit duplicate entries? */
    int	dbi_no_dbsync;		/*!< don't call dbiSync */
    int	dbi_lockdbfd;		/*!< do fcntl lock on db fd */
    int	dbi_byteswapped;

    rpmdb dbi_rpmdb;		/*!< the parent rpm database */
    dbiIndexType dbi_type;	/*! Type of dbi (primary / index) */

    DB * dbi_db;		/*!< Berkeley DB * handle */
};

#ifdef __cplusplus
extern "C" {
#endif


RPM_GNUC_INTERNAL
/* Globally enable/disable fsync in the backend */
void dbSetFSync(void *dbenv, int enable);

/** \ingroup dbi
 * Return new configured index database handle instance.
 * @param rdb		rpm database
 * @param rpmtag	database index tag
 * @return		index database handle
 */
RPM_GNUC_INTERNAL
dbiIndex dbiNew(rpmdb rdb, rpmDbiTagVal rpmtag);

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
 * @param rpmtag	database index tag
 * @param dbiIndex	address of index database handle
 * @param flags
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiOpen(rpmdb rdb, rpmDbiTagVal rpmtag, dbiIndex * dbip, int flags);

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

/** \ingroup dbi
 * Retrieve index control flags (new/existing, read-only etc)
 * @param dbi		index database handle
 * @return		dbi control flags
 */
RPM_GNUC_INTERNAL
int dbiFlags(dbiIndex dbi);

/** \ingroup dbi
 * Retrieve index name (same as the backing file name)
 * @param dbi		index database handle
 * @return		dbi name
 */
RPM_GNUC_INTERNAL
const char * dbiName(dbiIndex dbi);

/** \ingroup dbi
 * Open a database cursor.
 * @param dbi		index database handle
 * @param flags		DB_WRITECURSOR if writing, or 0
 * @return		database cursor handle
 */
RPM_GNUC_INTERNAL
dbiCursor dbiCursorInit(dbiIndex dbi, unsigned int flags);

/** \ingroup dbi
 * Destroy a database cursor handle
 * @param dbc		database cursor handle
 * @return		NULL always
 */
RPM_GNUC_INTERNAL
dbiCursor dbiCursorFree(dbiCursor dbc);

/** \ingroup dbi
 * Store (key,data) pair in index database.
 * @param dbcursor	database cursor handle
 * @param key		store key value/length/flags
 * @param data		store data value/length/flags
 * @param flags		flags
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiCursorPut(dbiCursor dbc, DBT * key, DBT * data, unsigned int flags);

/** \ingroup dbi
 * Retrieve (key,data) pair from index database.
 * @param dbc		database cursor handle
 * @param key		retrieve key value/length/flags
 * @param data		retrieve data value/length/flags
 * @param flags		flags
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiCursorGet(dbiCursor dbc, DBT * key, DBT * data, unsigned int flags);

/** \ingroup dbi
 * Delete (key,data) pair(s) from index database.
 * @param dbc		database cursor handle
 * @param key		delete key value/length/flags
 * @param data		delete data value/length/flags
 * @param flags		flags
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiCursorDel(dbiCursor dbc, DBT * key, DBT * data, unsigned int flags);

/** \ingroup dbi
 * Retrieve count of (possible) duplicate items.
 * @param dbcursor	database cursor
 * @return		number of duplicates
 */
RPM_GNUC_INTERNAL
unsigned int dbiCursorCount(dbiCursor dbc);

/** \ingroup dbi
 * Retrieve underlying index database handle.
 * @param dbcursor	database cursor
 * @return		index database handle
 */
RPM_GNUC_INTERNAL
dbiIndex dbiCursorIndex(dbiCursor dbc);
#ifdef __cplusplus
}
#endif

#endif /* _DBI_H */
