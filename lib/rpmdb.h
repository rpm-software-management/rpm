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

/**
 * Private methods for accessing an index database.
 */
struct _dbiVec {
    int dbv_major;			/*<! Berkeley db version major */
    int dbv_minor;			/*<! Berkeley db version minor */
    int dbv_patch;			/*<! Berkeley db version patch */

/**
 * Return handle for an index database.
 * @return	0 success 1 fail
 */
    int (*open) (rpmdb rpmdb, int rpmtag, dbiIndex * dbip);

/**
 * Close index database.
 * @param dbi	index database handle
 * @param flags
 */
    int (*close) (dbiIndex dbi, unsigned int flags);

/**
 * Flush pending operations to disk.
 * @param dbi	index database handle
 * @param flags
 */
    int (*sync) (dbiIndex dbi, unsigned int flags);

/**
 * Return items that match criteria.
 * @param dbi	index database handle
 * @param str	search key
 * @param set	items retrieved from index database
 * @return	-1 error, 0 success, 1 not found
 */
    int (*SearchIndex) (dbiIndex dbi, const void * str, size_t len, dbiIndexSet * set);

/**
 * Change/delete items that match criteria.
 * @param dbi	index database handle
 * @param str	update key
 * @param set	items to update in index database
 * @return	0 success, 1 not found
 */
    int (*UpdateIndex) (dbiIndex dbi, const char * str, dbiIndexSet set);

/**
 * Delete item using db->del.
 * @param dbi	index database handle
 * @param keyp	key data
 * @param keylen key data length
 */
    int (*del) (dbiIndex dbi, void * keyp, size_t keylen);

/**
 * Retrieve item using db->get.
 * @param dbi	index database handle
 * @param keyp	key data
 * @param keylen key data length
 * @param datap	address of data pointer
 * @param datalen address of data length
 */
    int (*get) (dbiIndex dbi, void * keyp, size_t keylen,
			void ** datap, size_t * datalen);

/**
 * Save item using db->put.
 * @param dbi	index database handle
 * @param keyp	key data
 * @param keylen key data length
 * @param datap	data pointer
 * @param datalen data length
 */
    int (*put) (dbiIndex dbi, void * keyp, size_t keylen,
			void * datap, size_t datalen);

/**
 */
    int (*copen) (dbiIndex dbi);

/**
 */
    int (*cclose) (dbiIndex dbi);

/**
 */
    int (*join) (dbiIndex dbi);

/**
 * Retrieve item using dbcursor->c_get.
 * @param dbi	index database handle
 * @param keyp	address of key data
 * @param keylen address of key data length
 * @param datap	address of data pointer
 * @param datalen address of data length
 */
    int (*cget) (dbiIndex dbi, void ** keyp, size_t * keylen,
			void ** datap, size_t * datalen);

};

/* XXX hack to get prototypes correct */
#if !defined(DB_VERSION_MAJOR)
#define	DB_ENV	void
#define	DBT	void
#define	DB_LSN	void
#endif

/**
 * Describes an index database (implemented on Berkeley db[123] API).
 */
struct _dbiIndex {
    int			dbi_flags;	/*<! */
    int			dbi_type;	/*<! db index type */
    int			dbi_mode;	/*<! mode to use on open */
    int			dbi_perms;	/*<! file permission to use on open */
    int			dbi_major;	/*<! Berkeley API type */

    int			dbi_tear_down;
    int			dbi_use_cursors;
    int			dbi_get_rmw_cursor;
    int			dbi_no_fsync;

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
    void *	dbi_dbjoin;
    void *	dbi_dbcursor;
    void *	dbi_pkgs;

/*@observer@*/ const struct _dbiVec * dbi_vec;	/*<! private methods */

};

/**
 * Describes the collection of index databases used by rpm.
 */
struct rpmdb_s {
    const char *	db_root;	/*<! path prefix */
    const char *	db_home;	/*<! directory path */
    int			db_flags;	/*<! */
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
 * Return base file name for legacy index databases.
 * @param	rpmtag rpm tag
 * @return	base file name
 */
char * db0basename(int rpmtag);

/*@only@*/ /*@null@*/ dbiIndex db3New(rpmdb rpmdb, int rpmtag);

void db3Free( /*@only@*/ /*@null@*/ dbiIndex dbi);

/**
 * @param db		rpm database
 */
int rpmdbRemove(rpmdb db, unsigned int offset, int tolerant);

/**
 * @param db		rpm database
 */
int rpmdbAdd(rpmdb db, Header dbentry);

/**
 * @param db		rpm database
 */
int rpmdbUpdateRecord(rpmdb db, int secOffset, Header secHeader);

/**
 */
unsigned int rpmdbGetIteratorFileNum(rpmdbMatchIterator mi);

/**
 * @param db		rpm database
 */
int rpmdbFindFpList(rpmdb db, fingerPrint * fpList, /*@out@*/dbiIndexSet * matchList, 
		    int numItems);

/* XXX only for the benefit of runTransactions() */
/**
 * @param db		rpm database
 */
int findMatches(rpmdb db, const char * name, const char * version,
	const char * release, /*@out@*/ dbiIndexSet * matches);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDB */
