#ifndef H_RPMDB
#define H_RPMDB

/** \file lib/rpmdb.h
 * Access RPM indices using Berkeley db[123] interface.
 */

#include <rpmlib.h>

#include "fprint.h"

typedef /*@abstract@*/ struct _dbiIndexRecord * dbiIndexRecord;
typedef /*@abstract@*/ struct _dbiIndex * dbiIndex;

/* this will break if sizeof(int) != 4 */
/**
 * A single item from an index database (i.e. the "data returned").
 * Note: In rpm-3.0.4 and earlier, this structure was passed by value,
 * and was identical to the "data saved" structure below.
 */
struct _dbiIndexRecord {
    unsigned int recOffset;		/*!< byte offset of header in db */
    unsigned int fileNumber;		/*!< file array index */
    unsigned int fpNum;			/*!< finger print index */
    unsigned int dbNum;			/*!< database index */
};

/**
 * A single item in an index database (i.e. the "data saved").
 */
struct _dbiIR {
    unsigned int recOffset;		/*!< byte offset of header in db */
    unsigned int fileNumber;		/*!< file array index */
};
typedef	struct _dbiIR * DBIR_t;

/**
 * Items retrieved from the index database.
 */
struct _dbiIndexSet {
/*@owned@*/ struct _dbiIndexRecord * recs; /*!< array of records */
    int count;				/*!< number of records */
};

/* XXX hack to get prototypes correct */
#if !defined(DB_VERSION_MAJOR)
#define	DB_ENV	void
#define	DBC	void
#define	DBT	void
#define	DB_LSN	void
#endif

/**
 * Private methods for accessing an index database.
 */
struct _dbiVec {
    int dbv_major;			/*<! Berkeley db version major */
    int dbv_minor;			/*<! Berkeley db version minor */
    int dbv_patch;			/*<! Berkeley db version patch */

/**
 * Return handle for an index database.
 * @param rpmdb		rpm database
 * @param rpmtag	rpm tag
 * @return		0 on success
 */
    int (*open) (rpmdb rpmdb, int rpmtag, dbiIndex * dbip);

/**
 * Close index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*close) (dbiIndex dbi, unsigned int flags);

/**
 * Flush pending operations to disk.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*sync) (dbiIndex dbi, unsigned int flags);

/**
 */
    int (*copen) (dbiIndex dbi, DBC ** dbcp, unsigned int flags);

/**
 */
    int (*cclose) (dbiIndex dbi, DBC * dbcursor, unsigned int flags);

/**
 * Delete (key,data) pair(s) using db->del or dbcursor->c_del.
 * @param dbi		index database handle
 * @param keyp		key data
 * @param keylen	key data length
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cdel) (dbiIndex dbi, const void * keyp, size_t keylen, unsigned int flags);

/**
 * Retrieve (key,data) pair using db->get or dbcursor->c_get.
 * @param dbi		index database handle
 * @param keypp		address of key data
 * @param keylenp	address of key data length
 * @param datapp	address of data pointer
 * @param datalenp	address of data length
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cget) (dbiIndex dbi, void ** keypp, size_t * keylenp,
			void ** datapp, size_t * datalenp, unsigned int flags);

/**
 * Store (key,data) pair using db->put or dbcursor->c_put.
 * @param dbi		index database handle
 * @param keyp		key data
 * @param keylen	key data length
 * @param datap		data pointer
 * @param datalen	data length
 * @param flags		(unused)
 * @return		0 on success
 */
    int (*cput) (dbiIndex dbi, const void * keyp, size_t keylen,
			const void * datap, size_t datalen, unsigned int flags);

/**
 * Is database byte swapped?
 * @param dbi		index database handle
 * @return		0 no
 */
    int (*byteswapped) (dbiIndex dbi);

};

/**
 * Describes an index database (implemented on Berkeley db[123] API).
 */
struct _dbiIndex {
    const char *	dbi_root;
    const char *	dbi_home;
    const char *	dbi_file;
    const char *	dbi_subfile;

    int			dbi_cflags;	/*<! db_create/db_env_create flags */
    int			dbi_oeflags;	/*<! common (db,dbenv}->open flags */
    int			dbi_eflags;	/*<! dbenv->open flags */
    int			dbi_oflags;	/*<! db->open flags */
    int			dbi_tflags;	/*<! dbenv->txn_begin flags */

    int			dbi_type;	/*<! db index type */
    int			dbi_mode;	/*<! mode to use on open */
    int			dbi_perms;	/*<! file permission to use on open */
    int			dbi_major;	/*<! Berkeley API type */

    int			dbi_tear_down;
    int			dbi_use_cursors;
    int			dbi_get_rmw_cursor;
    int			dbi_no_fsync;	/*<! no-op fsync for db */
    int			dbi_no_dbsync;	/*<! don't call dbiSync */
    int			dbi_lockdbfd;	/*<! do fcntl lock on db fd */
    int			dbi_temporary;	/*<! non-persistent */
    int			dbi_debug;

	/* dbenv parameters */
    int			dbi_lorder;
    void		(*db_errcall) (const char *db_errpfx, char *buffer);
    FILE *		dbi_errfile;
    const char *	dbi_errpfx;
    int			dbi_verbose;
    int			dbi_region_init;
    int			dbi_tas_spins;
	/* mpool sub-system parameters */
    int			dbi_mp_mmapsize;	/*<! (10Mb) */
    int			dbi_mp_size;	/*<! (128Kb) */
	/* lock sub-system parameters */
    u_int32_t		dbi_lk_max;
    u_int32_t		dbi_lk_detect;
    int			dbi_lk_nmodes;
    u_int8_t		*dbi_lk_conflicts;
	/* log sub-system parameters */
    u_int32_t		dbi_lg_max;
    u_int32_t		dbi_lg_bsize;
	/* transaction sub-system parameters */
    u_int32_t		dbi_tx_max;
#if 0
    int			(*dbi_tx_recover) (DB_ENV *dbenv, DBT *log_rec, DB_LSN *lsnp, int redo, void *info);
#endif
	/* dbinfo parameters */
    int			dbi_cachesize;	/*<! */
    int			dbi_pagesize;	/*<! (fs blksize) */
    void *		(*dbi_malloc) (size_t nbytes);
	/* hash access parameters */
    unsigned int	dbi_h_ffactor;	/*<! */
    unsigned int	(*dbi_h_hash_fcn) (const void *bytes, u_int32_t length);
    unsigned int	dbi_h_nelem;	/*<! */
    unsigned int	dbi_h_flags;	/*<! DB_DUP, DB_DUPSORT */
    int			(*dbi_h_dup_compare_fcn) (const DBT *, const DBT *);
	/* btree access parameters */
    int			dbi_bt_flags;
    int			dbi_bt_minkey;
    int			(*dbi_bt_compare_fcn)(const DBT *, const DBT *);
    int			(*dbi_bt_dup_compare_fcn) (const DBT *, const DBT *);
    size_t		(*dbi_bt_prefix_fcn) (const DBT *, const DBT *);
	/* recno access parameters */
    int			dbi_re_flags;
    int			dbi_re_delim;
    u_int32_t		dbi_re_len;
    int			dbi_re_pad;
    const char *	dbi_re_source;

    rpmdb	dbi_rpmdb;
    int		dbi_rpmtag;		/*<! rpm tag used for index */
    int		dbi_jlen;		/*<! size of join key */

    unsigned int dbi_lastoffset;	/*<! db0 with falloc.c needs this */

    void *	dbi_db;			/*<! Berkeley db[123] handle */
    void *	dbi_dbenv;
    void *	dbi_dbinfo;
    void *	dbi_rmw;		/*<! db cursor (with DB_WRITECURSOR) */
    void *	dbi_pkgs;

/*@observer@*/ const struct _dbiVec * dbi_vec;	/*<! private methods */

};

/**
 * Describes the collection of index databases used by rpm.
 */
struct rpmdb_s {
    const char *	db_root;	/*<! path prefix */
    const char *	db_home;	/*<! directory path */
    int			db_flags;

    int			db_mode;	/*<! open mode */
    int			db_perms;	/*<! open permissions */

    int			db_major;	/*<! Berkeley API type */

    int			db_remove_env;
    int			db_filter_dups;

    const char *	db_errpfx;

    void		(*db_errcall) (const char *db_errpfx, char *buffer);
    FILE *		db_errfile;
    void *		(*db_malloc) (size_t nbytes);

    int			db_ndbi;
    dbiIndex		*_dbi;
};

/* for RPM's internal use only */

#define RPMDB_FLAG_JUSTCHECK	(1 << 0)
#define RPMDB_FLAG_MINIMAL	(1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return new configured index database handle instance.
 * @param rpmdb		rpm database
 */
/*@only@*/ /*@null@*/ dbiIndex db3New(rpmdb rpmdb, int rpmtag);

/**
 * Destroy index database handle instance.
 * @param dbi		index database handle
 */
void db3Free( /*@only@*/ /*@null@*/ dbiIndex dbi);

/**
 * Return handle for an index database.
 * @param rpmdb		rpm database
 * @param rpmtag	rpm tag
 * @param flags		(unused)
 * @return		index database handle
 */
dbiIndex dbiOpen(rpmdb rpmdb, int rpmtag, unsigned int flags);

/**
 * @param flags		(unused)
 */
int dbiCopen(dbiIndex dbi, DBC ** dbcp, unsigned int flags);
int XdbiCopen(dbiIndex dbi, DBC ** dbcp, unsigned int flags, const char *f, unsigned int l);
#define	dbiCopen(_a,_b,_c) \
	XdbiCopen(_a, _b, _c, __FILE__, __LINE__)

/**
 * @param flags		(unused)
 */
int dbiCclose(dbiIndex dbi, DBC * dbcursor, unsigned int flags);
int XdbiCclose(dbiIndex dbi, DBC * dbcursor, unsigned int flags, const char *f, unsigned int l);
#define	dbiCclose(_a,_b,_c) \
	XdbiCclose(_a, _b, _c, __FILE__, __LINE__)

/**
 * Delete (key,data) pair(s) from index database.
 * @param dbi		index database handle
 * @param keyp		key data
 * @param keylen	key data length
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiDel(dbiIndex dbi, const void * keyp, size_t keylen, unsigned int flags);

/**
 * Retrieve (key,data) pair from index database.
 * @param dbi		index database handle
 * @param keypp		address of key data
 * @param keylenp	address of key data length
 * @param datapp	address of data pointer
 * @param datalenp	address of data length
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiGet(dbiIndex dbi, void ** keypp, size_t * keylenp,
        void ** datapp, size_t * datalenp, unsigned int flags);

/**
 * Store (key,data) pair in index database.
 * @param dbi		index database handle
 * @param keyp		key data
 * @param keylen	key data length
 * @param datap		data pointer
 * @param datalen	data length
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiPut(dbiIndex dbi, const void * keyp, size_t keylen,
	const void * datap, size_t datalen, unsigned int flags);

/**
 * Close index database.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiClose(dbiIndex dbi, unsigned int flags);

/**
 * Flush pending operations to disk.
 * @param dbi		index database handle
 * @param flags		(unused)
 * @return		0 on success
 */
int dbiSync (dbiIndex dbi, unsigned int flags);

/**
 * Is database byte swapped?
 * @param dbi		index database handle
 * @return		0 no
 */
int dbiByteSwapped(dbiIndex dbi);

/**
 * Return base file name for index database (legacy).
 * @param rpmtag	rpm tag
 * @return		base file name
 */
char * db1basename(int rpmtag);

/**
 */
unsigned int rpmdbGetIteratorFileNum(rpmdbMatchIterator mi);

/**
 * @param rpmdb		rpm database
 */
int rpmdbFindFpList(rpmdb rpmdb, fingerPrint * fpList, /*@out@*/dbiIndexSet * matchList, 
		    int numItems);

/**
 * Destroy set of index database items.
 * @param set	set of index database items
 */
void dbiFreeIndexSet(/*@only@*/ /*@null@*/ dbiIndexSet set);

/**
 * Count items in index database set.
 * @param set	set of index database items
 * @return	number of items
 */
unsigned int dbiIndexSetCount(dbiIndexSet set);

/**
 * Return record offset of header from element in index database set.
 * @param set	set of index database items
 * @param recno	index of item in set
 * @return	record offset of header
 */
unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno);

/**
 * Return file index from element in index database set.
 * @param set	set of index database items
 * @param recno	index of item in set
 * @return	file index
 */
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDB */
