#ifndef _DBI_H
#define _DBI_H

#include "dbiset.h"

/* XXX: make this backend-specific, eliminate or something... */
#define	_USE_COPY_LOAD

enum rpmdbFlags {
    RPMDB_FLAG_JUSTCHECK	= (1 << 0),
    RPMDB_FLAG_REBUILD		= (1 << 1),
    RPMDB_FLAG_VERIFYONLY	= (1 << 2),
};

typedef enum dbCtrlOp_e {
    DB_CTRL_LOCK_RO		= 1,
    DB_CTRL_UNLOCK_RO		= 2,
    DB_CTRL_LOCK_RW		= 3,
    DB_CTRL_UNLOCK_RW		= 4,
    DB_CTRL_INDEXSYNC		= 5
} dbCtrlOp;

typedef struct dbiIndex_s * dbiIndex;
typedef struct dbiCursor_s * dbiCursor;

struct dbConfig_s {
    int	db_mmapsize;	/*!< (10Mb) */
    int	db_cachesize;	/*!< (128Kb) */
    int	db_verbose;
    int	db_no_fsync;	/*!< no-op fsync for db */
    int db_eflags;	/*!< obsolete */
};

struct dbiConfig_s {
    int	dbi_oflags;		/*!< open flags */
    int	dbi_no_dbsync;		/*!< don't call dbiSync */
    int	dbi_lockdbfd;		/*!< do fcntl lock on db fd */
};

struct rpmdbOps_s;

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
    char	* db_descr;	/*!< db backend description (for error msgs) */
    struct dbChk_s * db_checked;/*!< headerCheck()'ed package instances */
    rpmdb	db_next;
    int		db_opens;
    dbiIndex	db_pkgs;	/*!< Package db */
    const rpmDbiTag * db_tags;
    int		db_ndbi;	/*!< No. of tag indices. */
    dbiIndex 	* db_indexes;	/*!< Tag indices. */
    int		db_buildindex;	/*!< Index rebuild indicator */

    struct rpmdbOps_s * db_ops;	/*!< backend ops */

    /* dbenv and related parameters */
    void * db_dbenv;		/*!< Backend private handle */
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

enum dbcFlags_e {
    DBC_READ	= 0,
    DBC_WRITE	= (1 << 0),
};

enum dbcSearchType_e {
    DBC_NORMAL_SEARCH   = 0,
    DBC_PREFIX_SEARCH   = (1 << 0),
};

/** \ingroup dbi
 * Describes an index database (implemented on Berkeley db functionality).
 */
struct dbiIndex_s {
    rpmdb dbi_rpmdb;		/*!< the parent rpm database */
    dbiIndexType dbi_type;	/*! Type of dbi (primary / index) */
    const char * dbi_file;	/*!< file component of path */
    int dbi_flags;
    int	dbi_byteswapped;

    struct dbiConfig_s cfg;

    void * dbi_db;		/*!< Backend private handle */
};

#ifdef __cplusplus
extern "C" {
#endif


RPM_GNUC_INTERNAL
/* Globally enable/disable fsync in the backend */
void dbSetFSync(rpmdb rdb, int enable);

RPM_GNUC_INTERNAL
int dbCtrl(rpmdb rdb, dbCtrlOp ctrl);

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
 * Verify (and close) index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int dbiVerify(dbiIndex dbi, unsigned int flags);

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
 * @param flags		DBC_WRITE if writing, or 0 (DBC_READ) for reading
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
dbiCursor dbiCursorFree(dbiIndex dbi, dbiCursor dbc);


RPM_GNUC_INTERNAL
rpmRC pkgdbPut(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum,
               unsigned char *hdrBlob, unsigned int hdrLen);
RPM_GNUC_INTERNAL
rpmRC pkgdbDel(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum);
RPM_GNUC_INTERNAL
rpmRC pkgdbGet(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum,
               unsigned char **hdrBlob, unsigned int *hdrLen);
RPM_GNUC_INTERNAL
rpmRC pkgdbNew(dbiIndex dbi, dbiCursor dbc,  unsigned int *hdrNum);
RPM_GNUC_INTERNAL
unsigned int pkgdbKey(dbiIndex dbi, dbiCursor dbc);

RPM_GNUC_INTERNAL
rpmRC idxdbGet(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
               dbiIndexSet *set, int curFlags);
RPM_GNUC_INTERNAL
rpmRC idxdbPut(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
               dbiIndexItem rec);
RPM_GNUC_INTERNAL
rpmRC idxdbDel(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
               dbiIndexItem rec);
RPM_GNUC_INTERNAL
const void * idxdbKey(dbiIndex dbi, dbiCursor dbc, unsigned int *keylen);

struct rpmdbOps_s {
    int (*open)(rpmdb rdb, rpmDbiTagVal rpmtag, dbiIndex * dbip, int flags);
    int (*close)(dbiIndex dbi, unsigned int flags);
    int (*verify)(dbiIndex dbi, unsigned int flags);
    void (*setFSync)(rpmdb rdb, int enable);
    int (*ctrl)(rpmdb rdb, dbCtrlOp ctrl);

    dbiCursor (*cursorInit)(dbiIndex dbi, unsigned int flags);
    dbiCursor (*cursorFree)(dbiIndex dbi, dbiCursor dbc);

    rpmRC (*pkgdbGet)(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum, unsigned char **hdrBlob, unsigned int *hdrLen);
    rpmRC (*pkgdbPut)(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum, unsigned char *hdrBlob, unsigned int hdrLen);
    rpmRC (*pkgdbDel)(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum);
    rpmRC (*pkgdbNew)(dbiIndex dbi, dbiCursor dbc,  unsigned int *hdrNum);
    unsigned int (*pkgdbKey)(dbiIndex dbi, dbiCursor dbc);

    rpmRC (*idxdbGet)(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexSet *set, int curFlags);
    rpmRC (*idxdbPut)(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexItem rec);
    rpmRC (*idxdbDel)(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen, dbiIndexItem rec);
    const void * (*idxdbKey)(dbiIndex dbi, dbiCursor dbc, unsigned int *keylen);
};

RPM_GNUC_INTERNAL
extern struct rpmdbOps_s db3_dbops;

#ifdef ENABLE_NDB
RPM_GNUC_INTERNAL
extern struct rpmdbOps_s ndb_dbops;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _DBI_H */
