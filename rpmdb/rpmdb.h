#ifndef H_RPMDB
#define H_RPMDB
/*@-bounds@*/

/** \ingroup rpmdb dbi db1 db3
 * \file rpmdb/rpmdb.h
 * Access RPM indices using Berkeley DB interface(s).
 */

#include <assert.h>
#include "rpmlib.h"
#include "rpmsw.h"
#include "db.h"

/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmdb_debug;
/*@=exportlocal@*/

#ifdef	NOTYET
/** \ingroup rpmdb
 * Database of headers and tag value indices.
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmdb_s * rpmdb;

/** \ingroup rpmdb
 * Database iterator.
 */
typedef /*@abstract@*/ struct _rpmdbMatchIterator * rpmdbMatchIterator;
#endif

/**
 * Tag value pattern match mode.
 */
typedef enum rpmMireMode_e {
    RPMMIRE_DEFAULT	= 0,	/*!< regex with \., .* and ^...$ added */
    RPMMIRE_STRCMP	= 1,	/*!< strings  using strcmp(3) */
    RPMMIRE_REGEX	= 2,	/*!< regex(7) patterns through regcomp(3) */
    RPMMIRE_GLOB	= 3	/*!< glob(7) patterns through fnmatch(3) */
} rpmMireMode;

/**
 */
typedef /*@abstract@*/ struct _dbiIndexItem * dbiIndexItem;

/** \ingroup rpmdb
 * A single element (i.e. inverted list from tag values) of a database.
 */
typedef /*@abstract@*/ struct _dbiIndexSet * dbiIndexSet;

/**
 */
typedef /*@abstract@*/ struct _dbiIndex * dbiIndex;

/* this will break if sizeof(int) != 4 */
/** \ingroup dbi
 * A single item from an index database (i.e. the "data returned").
 * Note: In rpm-3.0.4 and earlier, this structure was passed by value,
 * and was identical to the "data saved" structure below.
 */
struct _dbiIndexItem {
    unsigned int hdrNum;		/*!< header instance in db */
    unsigned int tagNum;		/*!< tag index in header */
    unsigned int fpNum;			/*!< finger print index */
};

/** \ingroup dbi
 * Items retrieved from the index database.
 */
struct _dbiIndexSet {
/*@owned@*/ struct _dbiIndexItem * recs; /*!< array of records */
    int count;				/*!< number of records */
};

/** \ingroup dbi
 * Private methods for accessing an index database.
 */
struct _dbiVec {
    int dbv_major;			/*!< Berkeley db version major */
    int dbv_minor;			/*!< Berkeley db version minor */
    int dbv_patch;			/*!< Berkeley db version patch */

/** \ingroup dbi
 * Return handle for an index database.
 * @param rpmdb		rpm database
 * @param rpmtag	rpm tag
 * @return		0 on success
 */
    int (*open) (rpmdb rpmdb, rpmTag rpmtag, /*@out@*/ dbiIndex * dbip)
	/*@globals fileSystem @*/
	/*@modifies *dbip, fileSystem @*/;

/** \ingroup dbi
 * Close index database, and destroy database handle.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*close) (/*@only@*/ dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/;

/** \ingroup dbi
 * Flush pending operations to disk.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*sync) (dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/** \ingroup dbi
 * Associate secondary database with primary.
 * @param dbi		index database handle
 * @param dbisecondary	secondary index database handle
 * @param callback	create secondary key from primary (NULL if DB_RDONLY)
 * @param flags		DB_CREATE or 0
 * @return		0 on success
 */
    int (*associate) (dbiIndex dbi, dbiIndex dbisecondary,
                int (*callback) (DB *, const DBT *, const DBT *, DBT *),
                unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/;

/** \ingroup dbi
 * Return join cursor for list of cursors.
 * @param dbi		index database handle
 * @param curslist	NULL terminated list of database cursors
 * @retval dbcp		address of join database cursor
 * @param flags		DB_JOIN_NOSORT or 0
 * @return		0 on success
 */
    int (*join) (dbiIndex dbi, DBC ** curslist, /*@out@*/ DBC ** dbcp,
                unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/;

/** \ingroup dbi
 * Open database cursor.
 * @param dbi		index database handle
 * @param txnid		database transaction handle
 * @retval dbcp		address of new database cursor
 * @param dbiflags	DB_WRITECURSOR or 0
 * @return		0 on success
 */
    int (*copen) (dbiIndex dbi, /*@null@*/ DB_TXN * txnid,
			/*@out@*/ DBC ** dbcp, unsigned int dbiflags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *txnid, *dbcp, fileSystem @*/;

/** \ingroup dbi
 * Close database cursor.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cclose) (dbiIndex dbi, /*@only@*/ DBC * dbcursor, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Duplicate a database cursor.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @retval dbcp		address of new database cursor
 * @param flags		DB_POSITION for same position, 0 for uninitialized
 * @return		0 on success
 */
    int (*cdup) (dbiIndex dbi, DBC * dbcursor, /*@out@*/ DBC ** dbcp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/;

/** \ingroup dbi
 * Delete (key,data) pair(s) using db->del or dbcursor->c_del.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->del)
 * @param key		delete key value/length/flags
 * @param data		delete data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cdel) (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key, DBT * data,
			unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Retrieve (key,data) pair using db->get or dbcursor->c_get.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->get)
 * @param key		retrieve key value/length/flags
 * @param data		retrieve data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cget) (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key, DBT * data,
			unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *data, fileSystem @*/;

/** \ingroup dbi
 * Retrieve (key,data) pair using dbcursor->c_pget.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param key		secondary retrieve key value/length/flags
 * @param pkey		primary retrieve key value/length/flags
 * @param data		primary retrieve data value/length/flags
 * @param flags		DB_NEXT, DB_SET, or 0
 * @return		0 on success
 */
    int (*cpget) (dbiIndex dbi, /*@null@*/ DBC * dbcursor,
		DBT * key, DBT * pkey, DBT * data, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *pkey, *data, fileSystem @*/;

/** \ingroup dbi
 * Store (key,data) pair using db->put or dbcursor->c_put.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->put)
 * @param key		store key value/length/flags
 * @param data		store data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cput) (dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key, DBT * data,
			unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Retrieve count of (possible) duplicate items using dbcursor->c_count.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param countp	address of count
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*ccount) (dbiIndex dbi, DBC * dbcursor,
			/*@out@*/ unsigned int * countp,
			unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Is database byte swapped?
 * @param dbi		index database handle
 * @return		0 no
 */
    int (*byteswapped) (dbiIndex dbi)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/** \ingroup dbi
 * Save statistics in database handle.
 * @param dbi		index database handle
 * @param flags		retrieve statistics that don't require traversal?
 * @return		0 on success
 */
    int (*stat) (dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/;

};

/** \ingroup dbi
 * Describes an index database (implemented on Berkeley db3 functionality).
 */
struct _dbiIndex {
/*@null@*/
    const char * dbi_root;	/*!< chroot(2) component of path */
/*@null@*/
    const char * dbi_home;	/*!< directory component of path */
/*@null@*/
    const char * dbi_file;	/*!< file component of path */
/*@null@*/
    const char * dbi_subfile;
/*@null@*/
    const char * dbi_tmpdir;	/*!< temporary directory */

    int	dbi_ecflags;		/*!< db_env_create flags */
    int	dbi_cflags;		/*!< db_create flags */
    int	dbi_oeflags;		/*!< common (db,dbenv}->open flags */
    int	dbi_eflags;		/*!< dbenv->open flags */
    int	dbi_oflags;		/*!< db->open flags */
    int	dbi_tflags;		/*!< dbenv->txn_begin flags */

    int	dbi_type;		/*!< db index type */
    unsigned dbi_mode;		/*!< mode to use on open */
    int	dbi_perms;		/*!< file permission to use on open */
    long dbi_shmkey;		/*!< shared memory base key */
    int	dbi_api;		/*!< Berkeley API type */

    int	dbi_verify_on_close;
    int	dbi_use_dbenv;		/*!< use db environment? */
    int	dbi_permit_dups;	/*!< permit duplicate entries? */
    int	dbi_no_fsync;		/*!< no-op fsync for db */
    int	dbi_no_dbsync;		/*!< don't call dbiSync */
    int	dbi_lockdbfd;		/*!< do fcntl lock on db fd */
    int	dbi_temporary;		/*!< non-persistent */
    int	dbi_debug;
    int	dbi_byteswapped;

/*@null@*/
    char * dbi_host;
    unsigned long dbi_cl_timeout;
    unsigned long dbi_sv_timeout;

	/* dbenv parameters */
    int	dbi_lorder;
/*@unused@*/
    void (*db_errcall) (const char *db_errpfx, char *buffer)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@unused@*/ /*@shared@*/
    FILE *	dbi_errfile;
    const char * dbi_errpfx;
    int	dbi_verbose;
    int	dbi_region_init;
    int	dbi_tas_spins;
	/* mpool sub-system parameters */
    int	dbi_mmapsize;	/*!< (10Mb) */
    int	dbi_cachesize;	/*!< (128Kb) */
	/* lock sub-system parameters */
    unsigned int dbi_lk_max;
    unsigned int dbi_lk_detect;
/*@unused@*/ int dbi_lk_nmodes;
/*@unused@*/ unsigned char * dbi_lk_conflicts;
	/* log sub-system parameters */
    unsigned int dbi_lg_max;
    unsigned int dbi_lg_bsize;
	/* transaction sub-system parameters */
    unsigned int dbi_tx_max;
#if 0
    int	(*dbi_tx_recover) (DB_ENV *dbenv, DBT *log_rec,
				DB_LSN *lsnp, int redo, void *info)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
#endif
	/* dbinfo parameters */
    int	dbi_pagesize;		/*!< (fs blksize) */
/*@unused@*/ /*@null@*/
    void * (*dbi_malloc) (size_t nbytes)
	/*@*/;
	/* hash access parameters */
    unsigned int dbi_h_ffactor;	/*!< */
    unsigned int (*dbi_h_hash_fcn) (DB *, const void *bytes,
				unsigned int length)
	/*@*/;
    unsigned int dbi_h_nelem;	/*!< */
    unsigned int dbi_h_flags;	/*!< DB_DUP, DB_DUPSORT */
    int (*dbi_h_dup_compare_fcn) (DB *, const DBT *, const DBT *)
	/*@*/;
	/* btree access parameters */
    int	dbi_bt_flags;
    int	dbi_bt_minkey;
    int	(*dbi_bt_compare_fcn) (DB *, const DBT *, const DBT *)
	/*@*/;
    int	(*dbi_bt_dup_compare_fcn) (DB *, const DBT *, const DBT *)
	/*@*/;
    size_t (*dbi_bt_prefix_fcn) (DB *, const DBT *, const DBT *)
	/*@*/;
	/* recno access parameters */
    int	dbi_re_flags;
    int	dbi_re_delim;
    unsigned int dbi_re_len;
    int	dbi_re_pad;
    const char * dbi_re_source;
	/* queue access parameters */
    unsigned int dbi_q_extentsize;

/*@refcounted@*/
    rpmdb dbi_rpmdb;		/*!< the parent rpm database */
    rpmTag dbi_rpmtag;		/*!< rpm tag used for index */
    int	dbi_jlen;		/*!< size of join key */

/*@only@*//*@null@*/
    DB * dbi_db;		/*!< Berkeley DB * handle */
/*@only@*//*@null@*/
    DB_TXN * dbi_txnid;		/*!< Bekerley DB_TXN * transaction id */
/*@only@*//*@null@*/
    void * dbi_stats;		/*!< Berkeley db statistics */

/*@observer@*/
    const struct _dbiVec * dbi_vec;	/*!< private methods */

};

/** \ingroup rpmdb
 * Describes the collection of index databases used by rpm.
 */
struct rpmdb_s {
/*@owned@*/
    const char * db_root;/*!< path prefix */
/*@owned@*/
    const char * db_home;/*!< directory path */
    int		db_flags;
    int		db_mode;	/*!< open mode */
    int		db_perms;	/*!< open permissions */
    int		db_api;		/*!< Berkeley API type */
/*@owned@*/
    const char * db_errpfx;
    int		db_remove_env;
    int		db_filter_dups;
    int		db_chrootDone;	/*!< If chroot(2) done, ignore db_root. */
    void (*db_errcall) (const char *db_errpfx, char *buffer)
	/*@*/;
/*@shared@*/
    FILE *	db_errfile;
/*@only@*/
    void * (*db_malloc) (size_t nbytes)
	/*@*/;
/*@only@*/
    void * (*db_realloc) (/*@only@*//*@null@*/ void * ptr,
						size_t nbytes)
	/*@*/;
    void (*db_free) (/*@only@*/ void * ptr)
	/*@modifies *ptr @*/;
/*@only@*/ /*@null@*/
    unsigned char * db_bits;	/*!< package instance bit mask. */
    int		db_nbits;	/*!< no. of bits in mask. */
    rpmdb	db_next;
    int		db_opens;
/*@only@*/ /*@null@*/
    void *	db_dbenv;	/*!< Berkeley DB_ENV handle. */
    int		db_ndbi;	/*!< No. of tag indices. */
    dbiIndex * _dbi;		/*!< Tag indices. */

    struct rpmop_s db_getops;
    struct rpmop_s db_putops;
    struct rpmop_s db_delops;

/*@refs@*/
    int nrefs;			/*!< Reference count. */
};

/* for RPM's internal use only */

/** \ingroup rpmdb
 */
enum rpmdbFlags {
	RPMDB_FLAG_JUSTCHECK	= (1 << 0),
	RPMDB_FLAG_MINIMAL	= (1 << 1),
/*@-enummemuse@*/
	RPMDB_FLAG_CHROOT	= (1 << 2)
/*@=enummemuse@*/
};

#ifdef __cplusplus
extern "C" {
#endif

/*@-exportlocal@*/
/** \ingroup db3
 * Return new configured index database handle instance.
 * @param rpmdb		rpm database
 * @param rpmtag	rpm tag
 * @return		index database handle
 */
/*@unused@*/ /*@only@*/ /*@null@*/
dbiIndex db3New(rpmdb rpmdb, rpmTag rpmtag)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/;

/** \ingroup db3
 * Destroy index database handle instance.
 * @param dbi		index database handle
 * @return		NULL always
 */
/*@null@*/
dbiIndex db3Free( /*@only@*/ /*@null@*/ dbiIndex dbi)
	/*@*/;

/** \ingroup db3
 * Format db3 open flags for debugging print.
 * @param dbflags		db open flags
 * @param print_dbenv_flags	format db env flags instead?
 * @return			formatted flags (static buffer)
 */
/*@-redecl@*/
/*@exposed@*/
extern const char *const prDbiOpenFlags(int dbflags, int print_dbenv_flags)
	/*@*/;
/*@=redecl@*/

/** \ingroup dbi
 * Return handle for an index database.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @param flags		(unused)
 * @return		index database handle
 */
/*@only@*/ /*@null@*/ dbiIndex dbiOpen(/*@null@*/ rpmdb db, rpmTag rpmtag,
		unsigned int flags)
	/*@globals rpmGlobalMacroContext, errno @*/
	/*@modifies db, rpmGlobalMacroContext, errno @*/;

/*@-globuse -mustmod @*/ /* FIX: vector annotations */
/** \ingroup dbi
 * Open a database cursor.
 * @param dbi		index database handle
 * @param txnid		database transaction handle
 * @retval dbcp		returned database cursor
 * @param flags		DB_WRITECURSOR if writing, or 0
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiCopen(dbiIndex dbi, /*@null@*/ DB_TXN * txnid,
		/*@out@*/ DBC ** dbcp, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/
{
    return (*dbi->dbi_vec->copen) (dbi, txnid, dbcp, flags);
}

/** \ingroup dbi
 * Close a database cursor.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiCclose(dbiIndex dbi, /*@only@*/ DBC * dbcursor, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcursor, fileSystem @*/
{
    return (*dbi->dbi_vec->cclose) (dbi, dbcursor, flags);
}

/** \ingroup dbi
 * Duplicate a database cursor.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @retval dbcp		address of new database cursor
 * @param flags		DB_POSITION for same position, 0 for uninitialized
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiCdup(dbiIndex dbi, DBC * dbcursor, /*@out@*/ DBC ** dbcp,
		unsigned int flags)
	/*@modifies dbi, *dbcp @*/
{
    return (*dbi->dbi_vec->cdup) (dbi, dbcursor, dbcp, flags);
}

/** \ingroup dbi
 * Delete (key,data) pair(s) from index database.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->del)
 * @param key		delete key value/length/flags
 * @param data		delete data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiDel(dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/
{
    int rc;
    assert(key->data != NULL && key->size > 0);
    (void) rpmswEnter(&dbi->dbi_rpmdb->db_delops, 0);
    rc = (dbi->dbi_vec->cdel) (dbi, dbcursor, key, data, flags);
    (void) rpmswExit(&dbi->dbi_rpmdb->db_delops, data->size);
    return rc;
}

/** \ingroup dbi
 * Retrieve (key,data) pair from index database.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->get)
 * @param key		retrieve key value/length/flags
 * @param data		retrieve data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiGet(dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *data, fileSystem @*/
{
    int rc;
    assert((flags == DB_NEXT) || (key->data != NULL && key->size > 0));
    (void) rpmswEnter(&dbi->dbi_rpmdb->db_getops, 0);
    rc = (dbi->dbi_vec->cget) (dbi, dbcursor, key, data, flags);
    (void) rpmswExit(&dbi->dbi_rpmdb->db_getops, data->size);
    return rc;
}

/** \ingroup dbi
 * Retrieve (key,data) pair using dbcursor->c_pget.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->get)
 * @param key		secondary retrieve key value/length/flags
 * @param pkey		primary retrieve key value/length/flags
 * @param data		primary retrieve data value/length/flags
 * @param flags		DB_NEXT, DB_SET, or 0
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiPget(dbiIndex dbi, /*@null@*/ DBC * dbcursor,
		DBT * key, DBT * pkey, DBT * data, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, *pkey, *data, fileSystem @*/
{
    int rc;
    assert((flags == DB_NEXT) || (key->data != NULL && key->size > 0));
    (void) rpmswEnter(&dbi->dbi_rpmdb->db_getops, 0);
    rc = (dbi->dbi_vec->cpget) (dbi, dbcursor, key, pkey, data, flags);
    (void) rpmswExit(&dbi->dbi_rpmdb->db_getops, data->size);
    return rc;
}

/** \ingroup dbi
 * Store (key,data) pair in index database.
 * @param dbi		index database handle
 * @param dbcursor	database cursor (NULL will use db->put)
 * @param key		store key value/length/flags
 * @param data		store data value/length/flags
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiPut(dbiIndex dbi, /*@null@*/ DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, *key, fileSystem @*/
{
    int rc;
    assert(key->data != NULL && key->size > 0 && data->data != NULL && data->size > 0);
    (void) rpmswEnter(&dbi->dbi_rpmdb->db_putops, 0);
    rc = (dbi->dbi_vec->cput) (dbi, dbcursor, key, data, flags);
    (void) rpmswExit(&dbi->dbi_rpmdb->db_putops, data->size);
    return rc;
}

/** \ingroup dbi
 * Retrieve count of (possible) duplicate items.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param countp	address of count
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiCount(dbiIndex dbi, DBC * dbcursor, /*@out@*/ unsigned int * countp,
		unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies *dbcursor, fileSystem @*/
{
    return (*dbi->dbi_vec->ccount) (dbi, dbcursor, countp, flags);
}

/** \ingroup dbi
 * Verify (and close) index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiVerify(/*@only@*/ dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    dbi->dbi_verify_on_close = 1;
    return (*dbi->dbi_vec->close) (dbi, flags);
}

/** \ingroup dbi
 * Close index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiClose(/*@only@*/ dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    return (*dbi->dbi_vec->close) (dbi, flags);
}

/** \ingroup dbi
 * Flush pending operations to disk.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiSync (dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    return (*dbi->dbi_vec->sync) (dbi, flags);
}

/** \ingroup dbi
 * Associate secondary database with primary.
 * @param dbi		index database handle
 * @param dbisecondary	secondary index database handle
 * @param callback	create secondary key from primary (NULL if DB_RDONLY)
 * @param flags		DB_CREATE or 0
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiAssociate(dbiIndex dbi, dbiIndex dbisecondary,
                int (*callback) (DB *, const DBT *, const DBT *, DBT *),
                unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, fileSystem @*/
{
    return (*dbi->dbi_vec->associate) (dbi, dbisecondary, callback, flags);
}

/** \ingroup dbi
 * Return join cursor for list of cursors.
 * @param dbi		index database handle
 * @param curslist	NULL terminated list of database cursors
 * @retval dbcp		address of join database cursor
 * @param flags		DB_JOIN_NOSORT or 0
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiJoin(dbiIndex dbi, DBC ** curslist, /*@out@*/ DBC ** dbcp,
                unsigned int flags)
	/*@globals fileSystem @*/
	/*@modifies dbi, *dbcp, fileSystem @*/
{
    return (*dbi->dbi_vec->join) (dbi, curslist, dbcp, flags);
}

/** \ingroup dbi
 * Is database byte swapped?
 * @param dbi		index database handle
 * @return		0 same order, 1 swapped order
 */
/*@unused@*/ static inline
int dbiByteSwapped(dbiIndex dbi)
	/*@modifies dbi @*/
{
    if (dbi->dbi_byteswapped == -1)
        dbi->dbi_byteswapped = (*dbi->dbi_vec->byteswapped) (dbi);
    return dbi->dbi_byteswapped;
}
/** \ingroup dbi
 * Is database byte swapped?
 * @param dbi		index database handle
 * @param flags		DB_FAST_STAT or 0
 * @return		0 on success
 */
/*@unused@*/ static inline
int dbiStat(dbiIndex dbi, unsigned int flags)
	/*@modifies dbi @*/
{
    return (*dbi->dbi_vec->stat) (dbi, flags);
}
/*@=globuse =mustmod @*/

/*@=exportlocal@*/

/** \ingroup rpmdb
 */
unsigned int rpmdbGetIteratorFileNum(rpmdbMatchIterator mi)
	/*@*/;

/** \ingroup dbi
 * Destroy set of index database items.
 * @param set	set of index database items
 * @return	NULL always
 */
/*@null@*/ dbiIndexSet dbiFreeIndexSet(/*@only@*/ /*@null@*/ dbiIndexSet set)
	/*@modifies set @*/;

/** \ingroup dbi
 * Count items in index database set.
 * @param set	set of index database items
 * @return	number of items
 */
unsigned int dbiIndexSetCount(dbiIndexSet set)
	/*@*/;

/** \ingroup dbi
 * Return record offset of header from element in index database set.
 * @param set	set of index database items
 * @param recno	index of item in set
 * @return	record offset of header
 */
unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno)
	/*@*/;

/** \ingroup dbi
 * Return file index from element in index database set.
 * @param set	set of index database items
 * @param recno	index of item in set
 * @return	file index
 */
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno)
	/*@*/;

/** \ingroup rpmdb
 * Tags for which rpmdb indices will be built.
 */
/*@-exportlocal@*/
/*@unchecked@*/
/*@only@*/ /*@null@*/ extern int * dbiTags;
/*@unchecked@*/
extern int dbiTagsMax;
/*@=exportlocal@*/

/** \ingroup rpmdb
 * Unreference a database instance.
 * @param db		rpm database
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmdb rpmdbUnlink (/*@killref@*/ /*@only@*/ rpmdb db, const char * msg)
	/*@modifies db @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmdb XrpmdbUnlink (/*@killref@*/ /*@only@*/ rpmdb db, const char * msg,
		const char * fn, unsigned ln)
	/*@modifies db @*/;
/*@=exportlocal@*/
#define	rpmdbUnlink(_db, _msg)	XrpmdbUnlink(_db, _msg, __FILE__, __LINE__)

/** \ingroup rpmdb
 * Reference a database instance.
 * @param db		rpm database
 * @param msg
 * @return		new rpm database reference
 */
/*@unused@*/
rpmdb rpmdbLink (rpmdb db, const char * msg)
	/*@modifies db @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
rpmdb XrpmdbLink (rpmdb db, const char * msg,
		const char * fn, unsigned ln)
        /*@modifies db @*/;
/*@=exportlocal@*/
#define	rpmdbLink(_db, _msg)	XrpmdbLink(_db, _msg, __FILE__, __LINE__)

/** \ingroup rpmdb
 * Open rpm database.
 * @param prefix	path to top of install tree
 * @retval dbp		address of rpm database
 * @param mode		open(2) flags:  O_RDWR or O_RDONLY (O_CREAT also)
 * @param perms		database permissions
 * @return		0 on success
 */
int rpmdbOpen (/*@null@*/ const char * prefix, /*@null@*/ /*@out@*/ rpmdb * dbp,
		int mode, int perms)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies *dbp, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmdb
 * Initialize database.
 * @param prefix	path to top of install tree
 * @param perms		database permissions
 * @return		0 on success
 */
int rpmdbInit(/*@null@*/ const char * prefix, int perms)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmdb
 * Verify database components.
 * @param prefix	path to top of install tree
 * @return		0 on success
 */
int rpmdbVerify(/*@null@*/ const char * prefix)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Close a single database index.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @return              0 on success
 */
int rpmdbCloseDBI(/*@null@*/ rpmdb db, int rpmtag)
	/*@globals fileSystem @*/
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmdb
 * Close all database indices and free rpmdb.
 * @param db		rpm database
 * @return		0 on success
 */
int rpmdbClose (/*@killref@*/ /*@only@*/ /*@null@*/ rpmdb db)
	/*@globals fileSystem @*/
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmdb
 * Sync all database indices.
 * @param db		rpm database
 * @return		0 on success
 */
int rpmdbSync (/*@null@*/ rpmdb db)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/** \ingroup rpmdb
 * Open all database indices.
 * @param db		rpm database
 * @return		0 on success
 */
/*@-exportlocal@*/
int rpmdbOpenAll (/*@null@*/ rpmdb db)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies db, rpmGlobalMacroContext @*/;
/*@=exportlocal@*/

/** \ingroup rpmdb
 * Return number of instances of package in rpm database.
 * @param db		rpm database
 * @param name		rpm package name
 * @return		number of instances
 */
int rpmdbCountPackages(/*@null@*/ rpmdb db, const char * name)
	/*@globals rpmGlobalMacroContext, fileSystem @*/
	/*@modifies db, rpmGlobalMacroContext, fileSystem @*/;

/** \ingroup rpmdb
 * Return header join key for current position of rpm database iterator.
 * @param mi		rpm database iterator
 * @return		current header join key
 */
unsigned int rpmdbGetIteratorOffset(/*@null@*/ rpmdbMatchIterator mi)
	/*@*/;

/** \ingroup rpmdb
 * Return number of elements in rpm database iterator.
 * @param mi		rpm database iterator
 * @return		number of elements
 */
int rpmdbGetIteratorCount(/*@null@*/ rpmdbMatchIterator mi)
	/*@*/;

/** \ingroup rpmdb
 * Append items to set of package instances to iterate.
 * @param mi		rpm database iterator
 * @param hdrNums	array of package instances
 * @param nHdrNums	number of elements in array
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbAppendIterator(/*@null@*/ rpmdbMatchIterator mi,
		/*@null@*/ const int * hdrNums, int nHdrNums)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Remove items from set of package instances to iterate.
 * @note Sorted hdrNums are always passed in rpmlib.
 * @param mi		rpm database iterator
 * @param hdrNums	array of package instances
 * @param nHdrNums	number of elements in array
 * @param sorted	is the array sorted? (array will be sorted on return)
 * @return		0 on success, 1 on failure (bad args)
 */
int rpmdbPruneIterator(/*@null@*/ rpmdbMatchIterator mi,
		/*@null@*/ int * hdrNums, int nHdrNums, int sorted)
	/*@modifies mi, hdrNums @*/;

/** \ingroup rpmdb
 * Add pattern to iterator selector.
 * @param mi		rpm database iterator
 * @param tag		rpm tag
 * @param mode		type of pattern match
 * @param pattern	pattern to match
 * @return		0 on success
 */
int rpmdbSetIteratorRE(/*@null@*/ rpmdbMatchIterator mi, rpmTag tag,
		rpmMireMode mode, /*@null@*/ const char * pattern)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies mi, mode, rpmGlobalMacroContext @*/;

/** \ingroup rpmdb
 * Prepare iterator for lazy writes.
 * @note Must be called before rpmdbNextIterator() with CDB model database.
 * @param mi		rpm database iterator
 * @param rewrite	new value of rewrite
 * @return		previous value
 */
int rpmdbSetIteratorRewrite(/*@null@*/ rpmdbMatchIterator mi, int rewrite)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Modify iterator to mark header for lazy write on release.
 * @param mi		rpm database iterator
 * @param modified	new value of modified
 * @return		previous value
 */
int rpmdbSetIteratorModified(/*@null@*/ rpmdbMatchIterator mi, int modified)
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Modify iterator to verify retrieved header blobs.
 * @param mi		rpm database iterator
 * @param ts		transaction set
 * @param (*hdrchk)	headerCheck() vector
 * @return		0 always
 */
int rpmdbSetHdrChk(/*@null@*/ rpmdbMatchIterator mi, /*@null@*/ rpmts ts,
		/*@null@*/ rpmRC (*hdrchk) (rpmts ts, const void * uh, size_t uc, const char ** msg))
	/*@modifies mi @*/;

/** \ingroup rpmdb
 * Return database iterator.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		NULL on failure
 */
/*@only@*/ /*@null@*/
rpmdbMatchIterator rpmdbInitIterator(/*@null@*/ rpmdb db, rpmTag rpmtag,
			/*@null@*/ const void * keyp, size_t keylen)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies db, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmdb
 * Return next package header from iteration.
 * @param mi		rpm database iterator
 * @return		NULL on end of iteration.
 */
/*@null@*/
Header rpmdbNextIterator(/*@null@*/ rpmdbMatchIterator mi)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies mi, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmdb
 * Check rpmdb signal handler for trapped signal exit.
 */
/*@mayexit@*/
int rpmdbCheckSignals(void)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/;

/** \ingroup rpmdb
 * Destroy rpm database iterator.
 * @param mi		rpm database iterator
 * @return		NULL always
 */
/*@null@*/
rpmdbMatchIterator rpmdbFreeIterator(/*@only@*/ /*@null@*/rpmdbMatchIterator mi)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies mi, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmdb
 * Add package header to rpm database and indices.
 * @param db		rpm database
 * @param iid		install transaction id (iid = 0 or -1 to skip)
 * @param h		header
 * @param ts		(unused) transaction set (or NULL)
 * @param (*hdrchk)	(unused) headerCheck() vector (or NULL)
 * @return		0 on success
 */
int rpmdbAdd(/*@null@*/ rpmdb db, int iid, Header h, /*@null@*/ rpmts ts,
		/*@null@*/ rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies db, h, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmdb
 * Remove package header from rpm database and indices.
 * @param db		rpm database
 * @param rid		(unused) remove transaction id (rid = 0 or -1 to skip)
 * @param hdrNum	package instance number in database
 * @param ts		(unused) transaction set (or NULL)
 * @param (*hdrchk)	(unused) headerCheck() vector (or NULL)
 * @return		0 on success
 */
int rpmdbRemove(/*@null@*/ rpmdb db, /*@unused@*/ int rid, unsigned int hdrNum,
		/*@null@*/ rpmts ts,
		/*@null@*/ rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies db, rpmGlobalMacroContext, fileSystem, internalState @*/;

/** \ingroup rpmdb
 * Rebuild database indices from package headers.
 * @param prefix	path to top of install tree
 * @param ts		transaction set (or NULL)
 * @param (*hdrchk)	headerCheck() vector (or NULL)
 * @return		0 on success
 */
int rpmdbRebuild(/*@null@*/ const char * prefix, /*@null@*/ rpmts ts,
		/*@null@*/ rpmRC (*hdrchk) (rpmts ts, const void *uh, size_t uc, const char ** msg))
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Mergesort, same arguments as qsort(2).
 */
/*@unused@*/
int mergesort(void *base, size_t nmemb, size_t size,
                int (*cmp) (const void *, const void *))
	/*@globals errno @*/
	/*@modifies base, errno @*/;

#ifdef __cplusplus
}
#endif

/*@=bounds@*/
#endif	/* H_RPMDB */
