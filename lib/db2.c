/** \ingroup db2
 * \file lib/db2.c
 */

static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */

#ifdef	__LCLINT__
typedef	unsigned int u_int32_t;
typedef	unsigned short u_int16_t;
typedef	unsigned char u_int8_t;
typedef	int int32_t;
#endif

#define	INLINE

#include "system.h"

#include <db2/db.h>

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>	/* XXX urlPath proto */

#include "rpmdb.h"
#include "debug.h"

/*@access rpmdb@*/
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

#if DB_VERSION_MAJOR == 2
#define	__USE_DB2	1

/* XXX remap DB3 types back into DB2 types */
static INLINE DBTYPE db3_to_dbtype(int dbitype)
{
    switch(dbitype) {
    case 1:	return DB_BTREE;
    case 2:	return DB_HASH;
    case 3:	return DB_RECNO;
    case 4:	return DB_HASH;		/* XXX W2DO? */
    case 5:	return DB_HASH;		/* XXX W2DO? */
    }
    /*@notreached@*/ return DB_HASH;
}

#if defined(__USE_DB2) || defined(__USE_DB3)
#if defined(__USE_DB2)
static /*@observer@*/ const char * db_strerror(int error)
{
    if (error == 0)
	return ("Successful return: 0");
    if (error > 0)
	return (strerror(error));

    switch (error) {
    case DB_INCOMPLETE:
	return ("DB_INCOMPLETE: Cache flush was unable to complete");
    case DB_KEYEMPTY:
	return ("DB_KEYEMPTY: Non-existent key/data pair");
    case DB_KEYEXIST:
	return ("DB_KEYEXIST: Key/data pair already exists");
    case DB_LOCK_DEADLOCK:
	return ("DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock");
    case DB_LOCK_NOTGRANTED:
	return ("DB_LOCK_NOTGRANTED: Lock not granted");
    case DB_NOTFOUND:
	return ("DB_NOTFOUND: No matching key/data pair found");
#if defined(__USE_DB3)
    case DB_OLD_VERSION:
	return ("DB_OLDVERSION: Database requires a version upgrade");
    case DB_RUNRECOVERY:
	return ("DB_RUNRECOVERY: Fatal error, run database recovery");
#else	/* __USE_DB3 */
    case DB_LOCK_NOTHELD:
	return ("DB_LOCK_NOTHELD:");
    case DB_REGISTERED:
	return ("DB_REGISTERED:");
#endif	/* __USE_DB3 */
    default:
      {
	/*
	 * !!!
	 * Room for a 64-bit number + slop.  This buffer is only used
	 * if we're given an unknown error, which should never happen.
	 * Note, however, we're no longer thread-safe if it does.
	 */
	static char ebuf[40];

	(void)snprintf(ebuf, sizeof(ebuf), "Unknown error: %d", error);
	return(ebuf);
      }
    }
    /*@notreached@*/
}

static int db_env_create(DB_ENV **dbenvp, int foo)
{
    DB_ENV *dbenv;

    if (dbenvp == NULL)
	return 1;
    dbenv = xcalloc(1, sizeof(*dbenv));

    *dbenvp = dbenv;
    return 0;
}
#endif	/* __USE_DB2 */

static int cvtdberr(dbiIndex dbi, const char * msg, int error, int printit) {
    int rc = 0;

    rc = error;

    if (printit && rc) {
	if (msg)
	    rpmError(RPMERR_DBERR, _("db%d error(%d) from %s: %s\n"),
		dbi->dbi_api, rc, msg, db_strerror(error));
	else
	    rpmError(RPMERR_DBERR, _("db%d error(%d): %s\n"),
		dbi->dbi_api, rc, db_strerror(error));
    }

    return rc;
}

static int db_fini(dbiIndex dbi, const char * dbhome, const char * dbfile,
		/*@unused@*/ const char * dbsubfile)
{
    DB_ENV * dbenv = dbi->dbi_dbenv;
    int rc;

#if defined(__USE_DB3)
    rpmdb rpmdb = dbi->dbi_rpmdb;

    if (dbenv == NULL) {
	dbi->dbi_dbenv = NULL;
	return 0;
    }

    rc = dbenv->close(dbenv, 0);
    rc = cvtdberr(dbi, "dbenv->close", rc, _debug);

    if (dbfile)
	rpmMessage(RPMMESS_DEBUG, _("closed  db environment %s/%s\n"),
		dbhome, dbfile);

    if (rpmdb->db_remove_env || dbi->dbi_tear_down) {
	int xx;

	xx = db_env_create(&dbenv, 0);
	xx = cvtdberr(dbi, "db_env_create", rc, _debug);
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 1
	xx = dbenv->remove(dbenv, dbhome, 0);
#else
	xx = dbenv->remove(dbenv, dbhome, NULL, 0);
#endif
	xx = cvtdberr(dbi, "dbenv->remove", rc, _debug);

	if (dbfile)
	    rpmMessage(RPMMESS_DEBUG, _("removed db environment %s/%s\n"),
			dbhome, dbfile);

    }
	
#else	/* __USE_DB3 */
    rc = db_appexit(dbenv);
    rc = cvtdberr(dbi, "db_appexit", rc, _debug);
    free(dbenv);
#endif	/* __USE_DB3 */
    dbi->dbi_dbenv = NULL;
    return rc;
}

static int db2_fsync_disable(/*@unused@*/ int fd) {
    return 0;
}

static int db_init(dbiIndex dbi, const char *dbhome, const char *dbfile,
		/*@unused@*/ const char * dbsubfile, /*@out@*/ DB_ENV **dbenvp)
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    DB_ENV *dbenv = NULL;
    int eflags;
    int rc;

    if (dbenvp == NULL)
	return 1;

    /* XXX HACK */
    if (rpmdb->db_errfile == NULL)
	rpmdb->db_errfile = stderr;

    eflags = (dbi->dbi_oeflags | dbi->dbi_eflags);
    if ( dbi->dbi_mode & O_CREAT) eflags |= DB_CREATE;

    if (dbfile)
	rpmMessage(RPMMESS_DEBUG, _("opening db environment %s/%s %s\n"),
		dbhome, dbfile, prDbiOpenFlags(eflags, 1));

    rc = db_env_create(&dbenv, dbi->dbi_cflags);
    rc = cvtdberr(dbi, "db_env_create", rc, _debug);
    if (rc)
	goto errxit;

#if defined(__USE_DB3)
  { int xx;
    dbenv->set_errcall(dbenv, rpmdb->db_errcall);
    dbenv->set_errfile(dbenv, rpmdb->db_errfile);
    dbenv->set_errpfx(dbenv, rpmdb->db_errpfx);
 /* dbenv->set_paniccall(???) */
    dbenv->set_verbose(dbenv, DB_VERB_CHKPOINT,
		(dbi->dbi_verbose & DB_VERB_CHKPOINT));
    dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK,
		(dbi->dbi_verbose & DB_VERB_DEADLOCK));
    dbenv->set_verbose(dbenv, DB_VERB_RECOVERY,
		(dbi->dbi_verbose & DB_VERB_RECOVERY));
    dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR,
		(dbi->dbi_verbose & DB_VERB_WAITSFOR));
 /* dbenv->set_lg_max(???) */
 /* dbenv->set_lk_conflicts(???) */
 /* dbenv->set_lk_detect(???) */
 /* dbenv->set_lk_max(???) */
    xx = dbenv->set_mp_mmapsize(dbenv, dbi->dbi_mp_mmapsize);
    xx = cvtdberr(dbi, "dbenv->set_mp_mmapsize", xx, _debug);
    xx = dbenv->set_cachesize(dbenv, 0, dbi->dbi_mp_size, 0);
    xx = cvtdberr(dbi, "dbenv->set_cachesize", xx, _debug);
 /* dbenv->set_tx_max(???) */
 /* dbenv->set_tx_recover(???) */
    if (dbi->dbi_no_fsync) {
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 1
	xx = db_env_set_func_fsync(db3_fsync_disable);
#else
	xx = dbenv->set_func_fsync(dbenv, db3_fsync_disable);
#endif
	xx = cvtdberr(dbi, "db_env_set_func_fsync", xx, _debug);
    }
  }
#else	/* __USE_DB3 */
    dbenv->db_errcall = rpmdb->db_errcall;
    dbenv->db_errfile = rpmdb->db_errfile;
    dbenv->db_errpfx = rpmdb->db_errpfx;
    dbenv->db_verbose = dbi->dbi_verbose;
    dbenv->mp_mmapsize = dbi->dbi_mp_mmapsize;	/* XXX default is 10 Mb */
    dbenv->mp_size = dbi->dbi_mp_size;		/* XXX default is 128 Kb */
#endif	/* __USE_DB3 */

#if defined(__USE_DB3)
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 1
    rc = dbenv->open(dbenv, dbhome, eflags, dbi->dbi_perms);
#else
    rc = dbenv->open(dbenv, dbhome, NULL, eflags, dbi->dbi_perms);
#endif
    rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
    if (rc)
	goto errxit;
#else	/* __USE_DB3 */
    rc = db_appinit(dbhome, NULL, dbenv, eflags);
    rc = cvtdberr(dbi, "db_appinit", rc, _debug);
    if (rc)
	goto errxit;
#endif	/* __USE_DB3 */

    *dbenvp = dbenv;

    return 0;

errxit:

#if defined(__USE_DB3)
    if (dbenv) {
	int xx;
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
    }
#else	/* __USE_DB3 */
    if (dbenv)	free(dbenv);
#endif	/* __USE_DB3 */
    return rc;
}

#endif	/* __USE_DB2 || __USE_DB3 */

static int db2sync(dbiIndex dbi, unsigned int flags)
{
    DB * db = dbi->dbi_db;
    int rc;

#if defined(__USE_DB2) || defined(__USE_DB3)
    int _printit;

    rc = db->sync(db, flags);
    /* XXX DB_INCOMPLETE is returned occaisionally with multiple access. */
    _printit = (rc == DB_INCOMPLETE ? 0 : _debug);
    rc = cvtdberr(dbi, "db->sync", rc, _printit);
#else	/* __USE_DB2 || __USE_DB3 */
    rc = db->sync(db, flags);
#endif	/* __USE_DB2 || __USE_DB3 */

    return rc;
}

static int db2c_del(dbiIndex dbi, DBC * dbcursor, u_int32_t flags)
{
    int rc;

    rc = dbcursor->c_del(dbcursor, flags);
    rc = cvtdberr(dbi, "dbcursor->c_del", rc, _debug);
    return rc;
}

/*@unused@*/ static int db2c_dup(dbiIndex dbi, DBC * dbcursor, DBC ** dbcp,
		u_int32_t flags)
{
    int rc = 0;

#if defined(__USE_DB3)
    rc = dbcursor->c_dup(dbcursor, dbcp, flags);
    rc = cvtdberr(dbi, "dbcursor->c_dup", rc, _debug);
#endif
    return rc;
}

static int db2c_get(dbiIndex dbi, DBC * dbcursor,
	DBT * key, DBT * data, u_int32_t flags)
{
    int _printit;
    int rc;
    int rmw;

#ifdef	NOTYET
    if ((dbi->dbi_eflags & DB_INIT_CDB) && !(dbi->dbi_oflags & DB_RDONLY))
	rmw = DB_RMW;
    else
#endif
	rmw = 0;

    rc = dbcursor->c_get(dbcursor, key, data, rmw | flags);

    /* XXX DB_NOTFOUND can be returned */
    _printit = (rc == DB_NOTFOUND ? 0 : _debug);
    rc = cvtdberr(dbi, "dbcursor->c_get", rc, _printit);
    return rc;
}

static int db2c_put(dbiIndex dbi, /*@only@*/ DBC * dbcursor,
	DBT * key, DBT * data, u_int32_t flags)
{
    int rc;

    rc = dbcursor->c_put(dbcursor, key, data, flags);

    rc = cvtdberr(dbi, "dbcursor->c_put", rc, _debug);
    return rc;
}

static INLINE int db2c_close(dbiIndex dbi, DBC * dbcursor)
{
    int rc;

    rc = dbcursor->c_close(dbcursor);
    rc = cvtdberr(dbi, "dbcursor->c_close", rc, _debug);
    return rc;
}

static INLINE int db2c_open(dbiIndex dbi, /*@out@*/ DBC ** dbcp)
{
    DB * db = dbi->dbi_db;
    DB_TXN * txnid = NULL;
    int rc;

#if defined(__USE_DB3)
    int flags;

    if ((dbi->dbi_eflags & DB_INIT_CDB) && !(dbi->dbi_oflags & DB_RDONLY))
	flags = DB_WRITECURSOR;
    else
	flags = 0;
    rc = db->cursor(db, txnid, dbcp, flags);
#else	/* __USE_DB3 */
    rc = db->cursor(db, txnid, dbcp);
#endif	/* __USE_DB3 */
    rc = cvtdberr(dbi, "db2c_open", rc, _debug);

    return rc;
}

static int db2cclose(dbiIndex dbi, /*@only@*/ DBC * dbcursor,
		unsigned int flags)
{
    int rc = 0;

    /* XXX per-iterator cursors */
    if (flags == 1)
	return db2c_close(dbi, dbcursor);

    if (!dbi->dbi_use_cursors)
	return 0;

    if (dbcursor == NULL)
	dbcursor = dbi->dbi_rmw;
    if (dbcursor) {
	if (dbcursor == dbi->dbi_rmw)
	    dbi->dbi_rmw = NULL;
	rc = db2c_close(dbi, dbcursor);
    }
    return rc;
}

static int db2copen(dbiIndex dbi, /*@out@*/ DBC ** dbcp, unsigned int flags)
{
    DBC * dbcursor;
    int rc = 0;

    /* XXX per-iterator cursors */
    if (flags == 1)
	return db2c_open(dbi, dbcp);

    if (!dbi->dbi_use_cursors) {
	if (dbcp) *dbcp = NULL;
	return 0;
    }

    if ((dbcursor = dbi->dbi_rmw) == NULL) {
	if ((rc = db2c_open(dbi, &dbcursor)) == 0)
	    dbi->dbi_rmw = dbcursor;
    }

    if (dbcp)
	*dbcp = dbi->dbi_rmw;

    return rc;
}

static int db2cput(dbiIndex dbi, DBC * dbcursor,
		const void * keyp, size_t keylen,
		const void * datap, size_t datalen,
		/*@unused@*/ unsigned int flags)
{
    DB * db = dbi->dbi_db;
    DB_TXN * txnid = NULL;
    DBT key, data;
    int rc;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = (void *)keyp;
    key.size = keylen;
    data.data = (void *)datap;
    data.size = datalen;

    if (dbcursor == NULL) {
	rc = db->put(db, txnid, &key, &data, 0);
	rc = cvtdberr(dbi, "db->put", rc, _debug);
    } else {

	rc = db2c_put(dbi, dbcursor, &key, &data, DB_AFTER);

    }

    return rc;
}

static int db2cdel(dbiIndex dbi, DBC * dbcursor,
		const void * keyp, size_t keylen,
		/*@unused@*/ unsigned int flags)
{
    DB * db = dbi->dbi_db;
    DB_TXN * txnid = NULL;
    DBT key, data;
    int rc;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = (void *)keyp;
    key.size = keylen;

    if (dbcursor == NULL) {
	rc = db->del(db, txnid, &key, 0);
	rc = cvtdberr(dbi, "db->del", rc, _debug);
    } else {

	rc = db2c_get(dbi, dbcursor, &key, &data, DB_SET);

	if (rc == 0) {
	    /* XXX TODO: loop over duplicates */
	    rc = db2c_del(dbi, dbcursor, 0);
	}

    }

    return rc;
}

static int db2cget(dbiIndex dbi, DBC * dbcursor,
		void ** keyp, size_t * keylen,
		void ** datap, size_t * datalen,
		/*@unused@*/ unsigned int flags)
{
    DB * db = dbi->dbi_db;
    DB_TXN * txnid = NULL;
    DBT key, data;
    int rc;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    if (keyp)		key.data = *keyp;
    if (keylen)		key.size = *keylen;
    if (datap)		data.data = *datap;
    if (datalen)	data.size = *datalen;

    if (dbcursor == NULL) {
	int _printit;
	rc = db->get(db, txnid, &key, &data, 0);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "db->get", rc, _printit);
    } else {

	/* XXX W2DO? db2 does DB_FIRST on uninitialized cursor? */
	rc = db2c_get(dbi, dbcursor, &key, &data,
		key.data == NULL ? DB_NEXT : DB_SET);

    }

    if (rc == 0) {
	if (keyp)	*keyp = key.data;
	if (keylen)	*keylen = key.size;
	if (datap)	*datap = data.data;
	if (datalen)	*datalen = data.size;
    }

    return rc;
}

static int db2byteswapped(dbiIndex dbi)
{
    int rc = 0;

#if defined(__USE_DB3)
    DB * db = dbi->dbi_db;

    rc = db->get_byteswapped(db);
#endif	/* __USE_DB3 */

    return rc;
}

static int db2close(/*@only@*/ dbiIndex dbi, unsigned int flags)
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    const char * urlfn = NULL;
    const char * dbhome;
    const char * dbfile;
    const char * dbsubfile;
    DB * db = dbi->dbi_db;
    int rc = 0, xx;

    urlfn = rpmGenPath(
	(dbi->dbi_root ? dbi->dbi_root : rpmdb->db_root),
	(dbi->dbi_home ? dbi->dbi_home : rpmdb->db_home),
	NULL);
    (void) urlPath(urlfn, &dbhome);
    if (dbi->dbi_temporary) {
	dbfile = NULL;
	dbsubfile = NULL;
    } else {
#ifdef	HACK
	dbfile = (dbi->dbi_file ? dbi->dbi_file : db2basename);
	dbsubfile = (dbi->dbi_subfile ? dbi->dbi_subfile : tagName(dbi->dbi_rpmtag));
#else
	dbfile = (dbi->dbi_file ? dbi->dbi_file : tagName(dbi->dbi_rpmtag));
	dbsubfile = NULL;
#endif
    }

#if defined(__USE_DB2) || defined(__USE_DB3)

    if (dbi->dbi_rmw)
	db2cclose(dbi, NULL, 0);

    if (db) {
	rc = db->close(db, 0);
	rc = cvtdberr(dbi, "db->close", rc, _debug);
	db = dbi->dbi_db = NULL;

	rpmMessage(RPMMESS_DEBUG, _("closed  db index       %s/%s\n"),
		dbhome, (dbfile ? dbfile : tagName(dbi->dbi_rpmtag)));

    }

    if (dbi->dbi_dbinfo) {
	free(dbi->dbi_dbinfo);
	dbi->dbi_dbinfo = NULL;
    }

    if (dbi->dbi_use_dbenv)
	xx = db_fini(dbi, dbhome, dbfile, dbsubfile);

#else	/* __USE_DB2 || __USE_DB3 */

    rc = db->close(db);

#endif	/* __USE_DB2 || __USE_DB3 */
    dbi->dbi_db = NULL;

    if (urlfn)
	free((void *)urlfn);

    db3Free(dbi);

    return rc;
}

static int db2open(rpmdb rpmdb, int rpmtag, dbiIndex * dbip)
{
    const char * urlfn = NULL;
    const char * dbhome;
    const char * dbfile;
    const char * dbsubfile;
    extern struct _dbiVec db2vec;
    dbiIndex dbi = NULL;
    int rc = 0;
    int xx;

#if defined(__USE_DB2) || defined(__USE_DB3)
    DB * db = NULL;
    DB_ENV * dbenv = NULL;
    u_int32_t oflags;
    int _printit;

    if (dbip)
	*dbip = NULL;
    if ((dbi = db3New(rpmdb, rpmtag)) == NULL)
	return 1;
    dbi->dbi_api = DB_VERSION_MAJOR;

    urlfn = rpmGenPath(
	(dbi->dbi_root ? dbi->dbi_root : rpmdb->db_root),
	(dbi->dbi_home ? dbi->dbi_home : rpmdb->db_home),
	NULL);
    (void) urlPath(urlfn, &dbhome);
    if (dbi->dbi_temporary) {
	dbfile = NULL;
	dbsubfile = NULL;
    } else {
#ifdef	HACK
	dbfile = (dbi->dbi_file ? dbi->dbi_file : db2basename);
	dbsubfile = (dbi->dbi_subfile ? dbi->dbi_subfile : tagName(dbi->dbi_rpmtag));
#else
	dbfile = (dbi->dbi_file ? dbi->dbi_file : tagName(dbi->dbi_rpmtag));
	dbsubfile = NULL;
#endif
    }

    oflags = (dbi->dbi_oeflags | dbi->dbi_oflags);
    oflags &= ~DB_TRUNCATE;	/* XXX this is dangerous */

#if 0	/* XXX rpmdb: illegal flag combination specified to DB->open */
    if ( dbi->dbi_mode & O_EXCL) oflags |= DB_EXCL;
#endif

    if (dbi->dbi_temporary) {
	oflags &= ~DB_RDONLY;
	oflags |= DB_CREATE;
    } else {
	if(!(dbi->dbi_mode & O_RDWR)) oflags |= DB_RDONLY;
	if ( dbi->dbi_mode & O_CREAT) oflags |= DB_CREATE;
	if ( dbi->dbi_mode & O_TRUNC) oflags |= DB_TRUNCATE;
    }

    dbi->dbi_dbinfo = NULL;

    if (dbi->dbi_use_dbenv)
	rc = db_init(dbi, dbhome, dbfile, dbsubfile, &dbenv);

    rpmMessage(RPMMESS_DEBUG, _("opening db index       %s/%s %s mode=0x%x\n"),
		dbhome, (dbfile ? dbfile : tagName(dbi->dbi_rpmtag)),
		prDbiOpenFlags(oflags, 0), dbi->dbi_mode);

    if (rc == 0) {
#if defined(__USE_DB3)
	rc = db_create(&db, dbenv, dbi->dbi_cflags);
	rc = cvtdberr(dbi, "db_create", rc, _debug);
	if (rc == 0) {
	    if (dbi->dbi_lorder) {
		rc = db->set_lorder(db, dbi->dbi_lorder);
		rc = cvtdberr(dbi, "db->set_lorder", rc, _debug);
	    }
	    if (dbi->dbi_cachesize) {
		rc = db->set_cachesize(db, 0, dbi->dbi_cachesize, 0);
		rc = cvtdberr(dbi, "db->set_cachesize", rc, _debug);
	    }
	    if (dbi->dbi_pagesize) {
		rc = db->set_pagesize(db, dbi->dbi_pagesize);
		rc = cvtdberr(dbi, "db->set_pagesize", rc, _debug);
	    }
	    if (rpmdb->db_malloc) {
		rc = db->set_malloc(db, rpmdb->db_malloc);
		rc = cvtdberr(dbi, "db->set_malloc", rc, _debug);
	    }
	    if (oflags & DB_CREATE) {
		switch(dbi->dbi_type) {
		default:
		case DB_HASH:
		    if (dbi->dbi_h_ffactor) {
			rc = db->set_h_ffactor(db, dbi->dbi_h_ffactor);
			rc = cvtdberr(dbi, "db->set_h_ffactor", rc, _debug);
		    }
		    if (dbi->dbi_h_hash_fcn) {
			rc = db->set_h_hash(db, dbi->dbi_h_hash_fcn);
			rc = cvtdberr(dbi, "db->set_h_hash", rc, _debug);
		    }
		    if (dbi->dbi_h_nelem) {
			rc = db->set_h_nelem(db, dbi->dbi_h_nelem);
			rc = cvtdberr(dbi, "db->set_h_nelem", rc, _debug);
		    }
		    if (dbi->dbi_h_flags) {
			rc = db->set_flags(db, dbi->dbi_h_flags);
			rc = cvtdberr(dbi, "db->set_h_flags", rc, _debug);
		    }
		    if (dbi->dbi_h_dup_compare_fcn) {
			rc = db->set_dup_compare(db, dbi->dbi_h_dup_compare_fcn);
			rc = cvtdberr(dbi, "db->set_dup_compare", rc, _debug);
		    }
		    break;
		case DB_BTREE:
		case DB_RECNO:
		case DB_QUEUE:
		    break;
		}
	    }
	    dbi->dbi_dbinfo = NULL;

	    {	const char * dbfullpath;
		const char * dbpath;
		char * t;
		int nb;

		nb = strlen(dbhome);
		if (dbfile)	nb += 1 + strlen(dbfile);
		dbfullpath = t = alloca(nb + 1);

		t = stpcpy(t, dbhome);
		if (dbfile)
		    t = stpcpy( stpcpy( t, "/"), dbfile);
		dbpath = (!dbi->dbi_use_dbenv && !dbi->dbi_temporary)
			? dbfullpath : dbfile;

		rc = db->open(db, dbpath, dbsubfile,
		    dbi->dbi_type, oflags, dbi->dbi_perms);
	    }

	    /* XXX return rc == errno without printing */
	    _printit = (rc > 0 ? 0 : _debug);
	    xx = cvtdberr(dbi, "db->open", rc, _printit);

	    if (rc == 0 && dbi->dbi_get_rmw_cursor) {
		DBC * dbcursor = NULL;
		DB_TXN * txnid = NULL;
		xx = db->cursor(db, txnid, &dbcursor,
			((oflags & DB_RDONLY) ? 0 : DB_WRITECURSOR));
		xx = cvtdberr(dbi, "db->cursor", xx, _debug);
		dbi->dbi_rmw = dbcursor;
	    } else
		dbi->dbi_rmw = NULL;

	    if (rc == 0 && dbi->dbi_lockdbfd) {
		int fdno = -1;

		if (!(db->fd(db, &fdno) == 0 && fdno >= 0)) {
		    rc = 1;
		} else {
		    struct flock l;
		    l.l_whence = 0;
		    l.l_start = 0;
		    l.l_len = 0;
		    l.l_type = (dbi->dbi_mode & O_RDWR) ? F_WRLCK : F_RDLCK;
		    l.l_pid = 0;

		    if (fcntl(fdno, F_SETLK, (void *) &l)) {
			rpmError(RPMERR_FLOCK,
				_("cannot get %s lock on %s/%s\n"),
				((dbi->dbi_mode & O_RDWR)
					? _("exclusive") : _("shared")),
				dbhome, dbfile);
			rc = 1;
		    } else if (dbfile) {
			rpmMessage(RPMMESS_DEBUG,
				_("locked  db index       %s/%s\n"),
				dbhome, dbfile);
		    }
		}
	    }
	}
	}
#else	/* __USE_DB3 */
      {	DB_INFO * dbinfo = xcalloc(1, sizeof(*dbinfo));
	dbinfo->db_cachesize = dbi->dbi_cachesize;
	dbinfo->db_lorder = dbi->dbi_lorder;
	dbinfo->db_pagesize = dbi->dbi_pagesize;
	dbinfo->db_malloc = rpmdb->db_malloc;
	if (oflags & DB_CREATE) {
	    switch(db3_to_dbtype(dbi->dbi_type)) {
	    default:
	    case DB_HASH:
		dbinfo->h_ffactor = dbi->dbi_h_ffactor;
		dbinfo->h_hash = dbi->dbi_h_hash_fcn;
		dbinfo->h_nelem = dbi->dbi_h_nelem;
		dbinfo->flags = dbi->dbi_h_flags;
		break;
	    }
	}
	dbi->dbi_dbinfo = dbinfo;

	    {	const char * dbfullpath;
		const char * dbpath;
		char * t;
		int nb;

		nb = strlen(dbhome);
		if (dbfile)	nb += 1 + strlen(dbfile);
		dbfullpath = t = alloca(nb + 1);

		t = stpcpy(t, dbhome);
		if (dbfile)
		    t = stpcpy( stpcpy( t, "/"), dbfile);
		dbpath = (!dbi->dbi_use_dbenv && !dbi->dbi_temporary)
			? dbfullpath : dbfile;

		rc = db_open(dbpath, db3_to_dbtype(dbi->dbi_type), oflags,
			dbi->dbi_perms, dbenv, dbinfo, &db);
	    }

	/* XXX return rc == errno without printing */
	_printit = (rc > 0 ? 0 : _debug);
	xx = cvtdberr(dbi, "db->open", rc, _printit);
      }
#endif	/* __USE_DB3 */
    }

    dbi->dbi_db = db;
    dbi->dbi_dbenv = dbenv;

#else	/* __USE_DB2 || __USE_DB3 */
    void * dbopeninfo = NULL;

    if (dbip)
	*dbip = NULL;
    if ((dbi = db3New(rpmdb, rpmtag)) == NULL)
	return 1;

    dbi->dbi_db = dbopen(dbfile, dbi->dbi_mode, dbi->dbi_perms,
		db3_to_dbtype(dbi->dbi_type), dbopeninfo);

    /* XXX return rc == errno without printing */
    if (dbi->dbi_db == NULL) rc = errno;
#endif	/* __USE_DB2 || __USE_DB3 */

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	dbi->dbi_vec = &db2vec;
	*dbip = dbi;
    } else
	db2close(dbi, 0);

    if (urlfn)
	free((void *)urlfn);

    return rc;
}

/** \ingroup db2
 */
struct _dbiVec db2vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db2open, db2close, db2sync, db2copen, db2cclose, db2cdel, db2cget, db2cput,
    db2byteswapped
};

#endif	/* DB_VERSION_MAJOR == 2 */
