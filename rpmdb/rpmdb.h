#ifndef H_RPMDB
#define H_RPMDB

/** \ingroup rpmdb dbi db1 db3
 * \file rpmdb/rpmdb.h
 * Access RPM indices using Berkeley DB interface(s).
 */

#include <rpmlib.h>

/**
 */
typedef /*@abstract@*/ struct _dbiIndexItem * dbiIndexItem;

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
    unsigned int dbNum;			/*!< database index */
};

/** \ingroup dbi
 * Items retrieved from the index database.
 */
struct _dbiIndexSet {
/*@owned@*/ struct _dbiIndexItem * recs; /*!< array of records */
    int count;				/*!< number of records */
};

/* XXX hack to get prototypes correct */
#if !defined(DB_VERSION_MAJOR)
#define	DB	void
#define	DB_ENV	void
#define	DBC	void
#define	DBT	void
#define	DB_LSN	void
#endif

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
    int (*open) (rpmdb rpmdb, int rpmtag, /*@out@*/ dbiIndex * dbip)
	/*@globals fileSystem@*/
	/*@modifies *dbip, fileSystem @*/;

/** \ingroup dbi
 * Close index database, and destroy database handle.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*close) (/*@only@*/ dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies dbi, fileSystem @*/;

/** \ingroup dbi
 * Flush pending operations to disk.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*sync) (dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies fileSystem @*/;

/** \ingroup dbi
 * Open database cursor.
 * @param dbi		index database handle
 * @param dbcp		address of database cursor
 * @param flags		(unused)
 */
    int (*copen) (dbiIndex dbi, /*@out@*/ DBC ** dbcp, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies dbi, *dbcp, fileSystem @*/;

/** \ingroup dbi
 * Close database cursor.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param flags		(unused)
 */
    int (*cclose) (dbiIndex dbi, /*@only@*/ DBC * dbcursor, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies dbi, *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Delete (key,data) pair(s) using db->del or dbcursor->c_del.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param keyp		key data
 * @param keylen	key data length
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cdel) (dbiIndex dbi, DBC * dbcursor,
			const void * keyp, size_t keylen, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Retrieve (key,data) pair using db->get or dbcursor->c_get.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param keypp		address of key data
 * @param keylenp	address of key data length
 * @param datapp	address of data pointer
 * @param datalenp	address of data length
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cget) (dbiIndex dbi, DBC * dbcursor,
			/*@null@*/ void ** keypp, /*@null@*/ size_t * keylenp,
			/*@null@*/ void ** datapp, /*@null@*/ size_t * datalenp,
			unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies *dbcursor, *keypp, *keylenp, *datapp, *datalenp,
		fileSystem @*/;

/** \ingroup dbi
 * Store (key,data) pair using db->put or dbcursor->c_put.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param keyp		key data
 * @param keylen	key data length
 * @param datap		data pointer
 * @param datalen	data length
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cput) (dbiIndex dbi, DBC * dbcursor,
			const void * keyp, size_t keylen,
			const void * datap, size_t datalen,
			unsigned int flags)
	/*@globals fileSystem@*/
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
	/*@globals fileSystem@*/
	/*@modifies *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Is database byte swapped?
 * @param dbi		index database handle
 * @return		0 no
 */
    int (*byteswapped) (dbiIndex dbi)
	/*@globals fileSystem@*/
	/*@modifies fileSystem@*/;

/** \ingroup dbi
 * Save statistics in database handle.
 * @param dbi		index database handle
 * @param flags		retrieve statistics that don't require traversal?
 * @return		0 on success
 */
    int (*stat) (dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies dbi, fileSystem @*/;

};

/** \ingroup dbi
 * Describes an index database (implemented on Berkeley db3 functionality).
 */
struct _dbiIndex {
/*@null@*/ const char *	dbi_root;	/*!< chroot(2) component of path */
/*@null@*/ const char *	dbi_home;	/*!< directory component of path */
/*@null@*/ const char *	dbi_file;	/*!< file component of path */
/*@null@*/ const char *	dbi_subfile;
/*@null@*/ const char *	dbi_tmpdir;	/*!< temporary directory */

    int			dbi_ecflags;	/*!< db_env_create flags */
    int			dbi_cflags;	/*!< db_create flags */
    int			dbi_oeflags;	/*!< common (db,dbenv}->open flags */
    int			dbi_eflags;	/*!< dbenv->open flags */
    int			dbi_oflags;	/*!< db->open flags */
    int			dbi_tflags;	/*!< dbenv->txn_begin flags */

    int			dbi_type;	/*!< db index type */
    unsigned		dbi_mode;	/*!< mode to use on open */
    int			dbi_perms;	/*!< file permission to use on open */
    long		dbi_shmkey;	/*!< shared memory base key */
    int			dbi_api;	/*!< Berkeley API type */

    int			dbi_verify_on_close;
    int			dbi_tear_down;	/*!< tear down dbenv on close */
    int			dbi_use_cursors;/*!< access with cursors? (always) */
    int			dbi_use_dbenv;	/*!< use db environment? */
    int			dbi_permit_dups;/*!< permit duplicate entries? */
    int			dbi_get_rmw_cursor;
    int			dbi_no_fsync;	/*!< no-op fsync for db */
    int			dbi_no_dbsync;	/*!< don't call dbiSync */
    int			dbi_lockdbfd;	/*!< do fcntl lock on db fd */
    int			dbi_temporary;	/*!< non-persistent */
    int			dbi_debug;
    int			dbi_byteswapped;

/*@null@*/ char *	dbi_host;
    long		dbi_cl_timeout;
    long		dbi_sv_timeout;

	/* dbenv parameters */
    int			dbi_lorder;
/*@unused@*/ /*@null@*/ void		(*db_errcall) (const char *db_errpfx, char *buffer)
	/*@globals fileSystem@*/
	/*@modifies fileSystem @*/;
/*@unused@*/ /*@shared@*/ FILE *	dbi_errfile;
    const char *	dbi_errpfx;
    int			dbi_verbose;
    int			dbi_region_init;
    int			dbi_tas_spins;
	/* mpool sub-system parameters */
    int			dbi_mp_mmapsize;	/*!< (10Mb) */
    int			dbi_mp_size;	/*!< (128Kb) */
	/* lock sub-system parameters */
    unsigned int	dbi_lk_max;
    unsigned int	dbi_lk_detect;
/*@unused@*/ int	dbi_lk_nmodes;
/*@unused@*/ unsigned char * dbi_lk_conflicts;
	/* log sub-system parameters */
    unsigned int	dbi_lg_max;
    unsigned int	dbi_lg_bsize;
	/* transaction sub-system parameters */
    unsigned int	dbi_tx_max;
#if 0
    int			(*dbi_tx_recover) (DB_ENV *dbenv, DBT *log_rec,
				DB_LSN *lsnp, int redo, void *info)
	/*@globals fileSystem@*/
	/*@modifies fileSystem @*/;
#endif
	/* dbinfo parameters */
    int			dbi_cachesize;	/*!< */
    int			dbi_pagesize;	/*!< (fs blksize) */
/*@unused@*/ /*@null@*/ void *	(*dbi_malloc) (size_t nbytes)
	/*@*/;
	/* hash access parameters */
    unsigned int	dbi_h_ffactor;	/*!< */
/*@null@*/ unsigned int	(*dbi_h_hash_fcn) (DB *, const void *bytes, unsigned int length) /*@*/;
    unsigned int	dbi_h_nelem;	/*!< */
    unsigned int	dbi_h_flags;	/*!< DB_DUP, DB_DUPSORT */
/*@null@*/ int		(*dbi_h_dup_compare_fcn) (DB *, const DBT *, const DBT *) /*@*/;
	/* btree access parameters */
    int			dbi_bt_flags;
    int			dbi_bt_minkey;
/*@null@*/ int		(*dbi_bt_compare_fcn) (DB *, const DBT *, const DBT *) /*@*/;
/*@null@*/ int		(*dbi_bt_dup_compare_fcn) (DB *, const DBT *, const DBT *) /*@*/;
/*@null@*/ size_t	(*dbi_bt_prefix_fcn) (DB *, const DBT *, const DBT *) /*@*/;
	/* recno access parameters */
    int			dbi_re_flags;
    int			dbi_re_delim;
    unsigned int	dbi_re_len;
    int			dbi_re_pad;
    const char *	dbi_re_source;
	/* queue access parameters */
    unsigned int	dbi_q_extentsize;

/*@refcounted@*/ rpmdb	dbi_rpmdb;
    rpmTag	dbi_rpmtag;		/*!< rpm tag used for index */
    int		dbi_jlen;		/*!< size of join key */

    unsigned int dbi_lastoffset;	/*!< db1 with falloc.c needs this */

/*@only@*//*@null@*/ void * dbi_db;	/*!< Berkeley DB handle */
/*@only@*//*@null@*/ void * dbi_rmw;	/*!< db cursor (with DB_WRITECURSOR) */
/*@only@*//*@null@*/ void * dbi_stats;	/*!< Berkeley db statistics */

/*@observer@*/ const struct _dbiVec * dbi_vec;	/*!< private methods */

};

/** \ingroup rpmdb
 * Describes the collection of index databases used by rpm.
 */
struct rpmdb_s {
/*@owned@*/ const char * db_root;/*!< path prefix */
/*@owned@*/ const char * db_home;/*!< directory path */
    int		db_flags;
    int		db_mode;	/*!< open mode */
    int		db_perms;	/*!< open permissions */
    int		db_api;		/*!< Berkeley API type */
/*@owned@*/ const char * db_errpfx;
    int		db_remove_env;
    int		db_filter_dups;
    int		db_chrootDone;	/*!< If chroot(2) done, ignore db_root. */
    void (*db_errcall) (const char *db_errpfx, char *buffer)
	/*@*/;
/*@shared@*/ FILE *	db_errfile;
/*@only@*/ void * (*db_malloc) (size_t nbytes)
	/*@*/;
/*@only@*/ void * (*db_realloc) (/*@only@*//*@null@*/ void * ptr,
						size_t nbytes)
	/*@*/;
    void (*db_free) (/*@only@*/ void * ptr)
	/*@modifies *ptr @*/;
    int		db_opens;
/*@only@*//*@null@*/ void * db_dbenv;	/*!< Berkeley DB_ENV handle */
    int		db_ndbi;	/*!< No. of tag indices. */
    dbiIndex *	_dbi;		/*!< Tag indices. */

/*@refs@*/ int nrefs;		/*!< Reference count. */
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
 */
/*@only@*/ /*@null@*/ dbiIndex db3New(rpmdb rpmdb, int rpmtag)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/;

/** \ingroup db3
 * Destroy index database handle instance.
 * @param dbi		index database handle
 * @return		NULL always
 */
/*@null@*/ dbiIndex db3Free( /*@only@*/ /*@null@*/ dbiIndex dbi)
	/*@*/;

/** \ingroup db3
 * Format db3 open flags for debugging print.
 * @param dbflags		db open flags
 * @param print_dbenv_flags	format db env flags instead?
 * @return			formatted flags (static buffer)
 */
/*@-redecl@*/
/*@exposed@*/ extern const char *const prDbiOpenFlags(int dbflags,
						int print_dbenv_flags)
	/*@*/;
/*@=redecl@*/

/** \ingroup dbi
 * Return handle for an index database.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @param flags		(unused)
 * @return		index database handle
 */
/*@only@*/ /*@null@*/ dbiIndex dbiOpen(/*@null@*/ rpmdb db, int rpmtag,
		unsigned int flags)
	/*@modifies db @*/;

/** \ingroup dbi
 * @param dbi		index database handle
 * @param flags		DBI_WRITECURSOR, DBI_ITERATOR or 0
 */
int dbiCopen(dbiIndex dbi, /*@out@*/ DBC ** dbcp, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies dbi, *dbcp, fileSystem @*/;

#define	DBI_WRITECURSOR		(1 << 0)
#define	DBI_ITERATOR		(1 << 1)

/** \ingroup dbi
 * @param dbi		index database handle
 * @param flags		(unused)
 */
int dbiCclose(dbiIndex dbi, /*@only@*/ DBC * dbcursor, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies dbi, *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Delete (key,data) pair(s) from index database.
 * @param dbi		index database handle
 * @param keyp		key data
 * @param keylen	key data length
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiDel(dbiIndex dbi, DBC * dbcursor, const void * keyp, size_t keylen,
		unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Retrieve (key,data) pair from index database.
 * @param dbi		index database handle
 * @param keypp		address of key data
 * @param keylenp	address of key data length
 * @param datapp	address of data pointer
 * @param datalenp	address of data length
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiGet(dbiIndex dbi, DBC * dbcursor, void ** keypp,
		/*@null@*/ size_t * keylenp,
		/*@null@*/ void ** datapp, 
		/*@null@*/ size_t * datalenp,
		unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies *dbcursor, **keypp, *keylenp, **datapp, *datalenp,
		fileSystem @*/;

/** \ingroup dbi
 * Store (key,data) pair in index database.
 * @param dbi		index database handle
 * @param keyp		key data
 * @param keylen	key data length
 * @param datap		data pointer
 * @param datalen	data length
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiPut(dbiIndex dbi, DBC * dbcursor, const void * keyp, size_t keylen,
		const void * datap, size_t datalen, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Retrieve count of (possible) duplicate items.
 * @param dbi		index database handle
 * @param dbcursor	database cursor
 * @param countp	address of count
 * @param flags		(unused)
 * @return		0 on success
 */
/*@unused@*/
int dbiCount(dbiIndex dbi, DBC * dbcursor, /*@out@*/ unsigned int * countp,
		unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies *dbcursor, fileSystem @*/;

/** \ingroup dbi
 * Verify (and close) index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiVerify(/*@only@*/ dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies dbi, fileSystem @*/;

/** \ingroup dbi
 * Close index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiClose(/*@only@*/ dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies dbi, fileSystem @*/;

/** \ingroup dbi
 * Flush pending operations to disk.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiSync (dbiIndex dbi, unsigned int flags)
	/*@globals fileSystem@*/
	/*@modifies fileSystem @*/;

/** \ingroup dbi
 * Is database byte swapped?
 * @param dbi		index database handle
 * @return		0 no
 */
int dbiByteSwapped(dbiIndex dbi)
	/*@*/;

/** \ingroup db1
 * Return base file name for db1 database (legacy).
 * @param rpmtag	rpm tag
 * @return		base file name of db1 database
 */
char * db1basename(int rpmtag)
	/*@*/;
/*@=exportlocal@*/

/** \ingroup rpmdb
 */
/*@unused@*/
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

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDB */
