#include "system.h"

static int _debug = 1;

#include <db.h>

#include <rpmlib.h>

#include "dbindex.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

#if DB_VERSION_MAJOR == 2
#define	__USE_DB2	1

/* XXX dbenv parameters */
static int db_lorder = 0;			/* 0 is native order */
static void (*db_errcall) (const char *db_errpfx, char *buffer) = NULL;
static FILE * db_errfile = NULL;
static const char * db_errpfx = "rpmdb";
static int db_verbose = 1;

static int dbmp_mmapsize = 16 * 1024 * 1024;	/* XXX db2 default: 10 Mb */
static int dbmp_size = 16 * 1024 * 1024;	/* XXX db2 default: 128 Kb */

/* XXX dbinfo parameters */
static int db_cachesize = 0;
static int db_pagesize = 16 * 1024;	/* 512 - 64K, 0 is fs blksize */
static void * (*db_malloc) (size_t nbytes) = NULL;

static u_int32_t (*dbh_hash) (const void *, u_int32_t) = NULL;
static u_int32_t dbh_ffactor = 0;	/* db1 default: 8 */
static u_int32_t dbh_nelem = 0;		/* db1 default: 1 */
static u_int32_t dbh_flags = 0;

#if defined(INIT_CDB)
static int dbopenflags = DB_INIT_CDB;
#else
static int dbopenflags = DB_INIT_MPOOL;
#endif

#define	_mymemset(_a, _b, _c)	memset((_a), (_b), (_c))

static inline DBTYPE dbi_to_dbtype(DBI_TYPE dbitype)
{
    switch(dbitype) {
    case DBI_BTREE:	return DB_BTREE;
    case DBI_HASH:	return DB_HASH;
    case DBI_RECNO:	return DB_RECNO;
    }
    /*@notreached@*/ return DB_HASH;
}

static inline /*@observer@*/ /*@null@*/ DB * GetDB(dbiIndex dbi) {
    return ((DB *)dbi->dbi_db);
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
#else
    case DB_LOCK_NOTHELD:
	return ("DB_LOCK_NOTHELD:");
    case DB_REGISTERED:
	return ("DB_REGISTERED:");
#endif
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

    if (error == 0)
	rc = 0;
    else if (error < 0)
	rc = 1;
    else if (error > 0)
	rc = -1;

    if (printit && rc) {
	fprintf(stderr, "*** db%d %s rc %d %s\n", dbi->dbi_major, msg,
		rc, db_strerror(error));
    }

    return rc;
}

static int db_init(dbiIndex dbi, const char *home, int dbflags,
			DB_ENV **dbenvp, void **dbinfop)
{
    DB_ENV *dbenv = NULL;
    int mydbopenflags;
    int rc;

    if (dbenvp == NULL || dbinfop == NULL)
	return 1;
    if (db_errfile == NULL)
	db_errfile = stderr;

    rc = db_env_create(&dbenv, 0);
    rc = cvtdberr(dbi, "db_env_create", rc, _debug);
    if (rc)
	goto errxit;

#if defined(__USE_DB3)
  { int xx;
    dbenv->set_errcall(dbenv, db_errcall);
    dbenv->set_errfile(dbenv, db_errfile);
    dbenv->set_errpfx(dbenv, db_errpfx);
 /* dbenv->set_paniccall(???) */
    dbenv->set_verbose(dbenv, DB_VERB_CHKPOINT, db_verbose);
    dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK, db_verbose);
    dbenv->set_verbose(dbenv, DB_VERB_RECOVERY, db_verbose);
    dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR, db_verbose);
 /* dbenv->set_lg_max(???) */
 /* dbenv->set_lk_conflicts(???) */
 /* dbenv->set_lk_detect(???) */
 /* dbenv->set_lk_max(???) */
    xx = dbenv->set_mp_mmapsize(dbenv, dbmp_mmapsize);
    xx = cvtdberr(dbi, "dbenv->set_mp_mmapsize", xx, _debug);
    xx = dbenv->set_cachesize(dbenv, 0, dbmp_size, 0);
    xx = cvtdberr(dbi, "dbenv->set_cachesize", xx, _debug);
 /* dbenv->set_tx_max(???) */
 /* dbenv->set_tx_recover(???) */
  }
#else
    dbenv->db_errcall = db_errcall;
    dbenv->db_errfile = db_errfile;
    dbenv->db_errpfx = db_errpfx;
    dbenv->db_verbose = db_verbose;
    dbenv->mp_mmapsize = dbmp_mmapsize;	/* XXX default is 10 Mb */
    dbenv->mp_size = dbmp_size;		/* XXX default is 128 Kb */
#endif

#define _DBFMASK	(DB_CREATE|DB_NOMMAP|DB_THREAD)
    mydbopenflags = (dbflags & _DBFMASK) | dbopenflags;

#if defined(__USE_DB3)
    rc = dbenv->open(dbenv, home, NULL, mydbopenflags, 0);
    rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
    if (rc)
	goto errxit;
    *dbinfop = NULL;
#else
    rc = db_appinit(home, NULL, dbenv, mydbopenflags);
    rc = cvtdberr(dbi, "db_appinit", rc, _debug);
    if (rc)
	goto errxit;
    {	DB_INFO * dbinfo = xcalloc(1, sizeof(*dbinfo));
	/* XXX W2DO? */
	dbinfo->db_cachesize = db_cachesize;
	dbinfo->db_lorder = db_lorder;
	dbinfo->db_pagesize = db_pagesize;
	dbinfo->db_malloc = db_malloc;
	if (dbflags & DB_CREATE) {
	    dbinfo->h_ffactor = dbh_ffactor;
	    dbinfo->h_hash = dbh_hash;
	    dbinfo->h_nelem = dbh_nelem;
	    dbinfo->flags = dbh_flags;
	}
	*dbinfop = dbinfo;
     }
#endif

    *dbenvp = dbenv;

    return 0;

errxit:

#if defined(__USE_DB3)
    if (dbenv) {
	int xx;
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
    }
#else
    if (dbenv)	free(dbenv);
#endif
    return rc;
}
#endif	/* __USE_DB2 || __USE_DB3 */

static int db2close(dbiIndex dbi, unsigned int flags)
{
    DB * db = GetDB(dbi);
    int rc = 0, xx;

#if defined(__USE_DB2) || defined(__USE_DB3)

    if (dbi->dbi_dbcursor) {
	DBC * dbcursor = (DBC *)dbi->dbi_dbcursor;
	xx = dbcursor->c_close(dbcursor);
	xx = cvtdberr(dbi, "dbcursor->c_close", xx, _debug);
	dbi->dbi_dbcursor = NULL;
    }

    if (db) {
	rc = db->close(db, 0);
	rc = cvtdberr(dbi, "db->close", rc, _debug);
	db = dbi->dbi_db = NULL;
    }

    if (dbi->dbi_dbinfo) {
	free(dbi->dbi_dbinfo);
	dbi->dbi_dbinfo = NULL;
    }
    if (dbi->dbi_dbenv) {
	DB_ENV * dbenv = (DB_ENV *)dbi->dbi_dbenv;
#if defined(__USE_DB3)
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
#else
	xx = db_appexit(dbenv);
	xx = cvtdberr(dbi, "db_appexit", xx, _debug);
#endif
	free(dbi->dbi_dbenv);
	dbi->dbi_dbenv = NULL;
    }
#else
    rc = db->close(db);
#endif

    return rc;
}

static int db2sync(dbiIndex dbi, unsigned int flags)
{
    DB * db = GetDB(dbi);
    int rc;

#if defined(__USE_DB2) || defined(__USE_DB3)
    rc = db->sync(db, flags);
    rc = cvtdberr(dbi, "db->sync", rc, _debug);
#else
    rc = db->sync(db, flags);
#endif

    return rc;
}

static int db2GetFirstKey(dbiIndex dbi, const char ** keyp)
{
    DBT key, data;
    DB * db;
    int _printit;
    int rc, xx;

    if (dbi == NULL || dbi->dbi_db == NULL)
	return 1;

    db = GetDB(dbi);
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    key.data = NULL;
    key.size = 0;

#if defined(__USE_DB2) || defined(__USE_DB3)
    {	DBC * dbcursor = NULL;
#if defined(__USE_DB3)
	rc = db->cursor(db, NULL, &dbcursor, 0);
#else
	rc = db->cursor(db, NULL, &dbcursor);
#endif
	rc = cvtdberr(dbi, "db->cursor", rc, _debug);
	if (rc == 0) {
	    rc = dbcursor->c_get(dbcursor, &key, &data, DB_FIRST);
	    _printit = (rc == DB_NOTFOUND ? 0 : _debug);
	    rc = cvtdberr(dbi, "dbcursor->c_get", rc, _printit);
	}
	if (dbcursor) {
	    xx = dbcursor->c_close(dbcursor);
	    xx = cvtdberr(dbi, "dbcursor->c_close", xx, _debug);
	}
    }
#else
    rc = db->seq(db, &key, &data, R_FIRST);
#endif

    if (rc == 0 && keyp) {
    	char *k = xmalloc(key.size + 1);
	memcpy(k, key.data, key.size);
	k[key.size] = '\0';
	*keyp = k;
    }

    return rc;
}

static int db2SearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set)
{
    DBT key, data;
    DB * db = GetDB(dbi);
    int _printit;
    int rc;

    if (set) *set = NULL;
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    key.data = (void *)str;
    key.size = strlen(str);
    data.data = NULL;
    data.size = 0;

#if defined(__USE_DB2) || defined(__USE_DB3)
    rc = db->get(db, NULL, &key, &data, 0);
    _printit = (rc == DB_NOTFOUND ? 0 : _debug);
    rc = cvtdberr(dbi, "db->get", rc, _printit);
#else
    rc = db->get(db, &key, &data, 0);
#endif

    if (rc == 0 && set) {
	*set = dbiCreateIndexSet();
	(*set)->recs = xmalloc(data.size);
	memcpy((*set)->recs, data.data, data.size);
	(*set)->count = data.size / sizeof(*(*set)->recs);
    }
    return rc;
}

/*@-compmempass@*/
static int db2UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set)
{
    DBT key;
    DB * db = GetDB(dbi);
    int rc;

    _mymemset(&key, 0, sizeof(key));
    key.data = (void *)str;
    key.size = strlen(str);

    if (set->count) {
	DBT data;

	_mymemset(&data, 0, sizeof(data));
	data.data = set->recs;
	data.size = set->count * sizeof(*(set->recs));

#if defined(__USE_DB2) || defined(__USE_DB3)
	rc = db->put(db, NULL, &key, &data, 0);
	rc = cvtdberr(dbi, "db->put", rc, _debug);
#else
	rc = db->put(db, &key, &data, 0);
#endif

    } else {

#if defined(__USE_DB2) || defined(__USE_DB3)
	rc = db->del(db, NULL, &key, 0);
	rc = cvtdberr(dbi, "db->del", rc, _debug);
#else
	rc = db->del(db, &key, 0);
#endif

    }

    return rc;
}
/*@=compmempass@*/

static int db2open(dbiIndex dbi)
{
    int rc = 0;

#if defined(__USE_DB2) || defined(__USE_DB3)
    char * dbhome = NULL;
    DB * db = NULL;
    DB_ENV * dbenv = NULL;
    void * dbinfo = NULL;
    u_int32_t dbflags;

    dbflags = (	!(dbi->dbi_flags & O_RDWR) ? DB_RDONLY :
		((dbi->dbi_flags & O_CREAT) ? DB_CREATE : 0));

    rc = db_init(dbi, dbhome, dbflags, &dbenv, &dbinfo);

    if (rc == 0) {
#if defined(__USE_DB3)
	rc = db_create(&db, dbenv, 0);
	rc = cvtdberr(dbi, "db_create", rc, _debug);
	if (rc == 0) {
	    rc = db->set_pagesize(db, db_pagesize);
	    rc = cvtdberr(dbi, "db->set_pagesize", rc, _debug);
	    rc = db->open(db, dbi->dbi_file, NULL, dbi_to_dbtype(dbi->dbi_type),
			dbflags, dbi->dbi_perms);
	    rc = cvtdberr(dbi, "db->open", rc, _debug);
	}
#else
	rc = db_open(dbi->dbi_file, dbi_to_dbtype(dbi->dbi_type), dbflags,
			dbi->dbi_perms, dbenv, dbinfo, &db);
	rc = cvtdberr(dbi, "db_open", rc, _debug);
#endif	/* __USE_DB3 */
    }

    dbi->dbi_db = db;
    dbi->dbi_dbenv = dbenv;
    dbi->dbi_dbinfo = dbinfo;

#else
    dbi->dbi_db = dbopen(dbi->dbi_file, dbi->dbi_flags, dbi->dbi_perms,
		dbi_to_dbtype(dbi->dbi_type), dbi->dbi_openinfo);
#endif	/* __USE_DB2 || __USE_DB3 */

    if (rc == 0 && dbi->dbi_db != NULL) {
	rc = 0;
fprintf(stderr, "*** db%dopen: %s\n", dbi->dbi_major, dbi->dbi_file);
    } else {
	rc = 1;
    }

    return rc;
}

struct _dbiVec db2vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db2open, db2close, db2sync, db2GetFirstKey, db2SearchIndex, db2UpdateIndex
};

#endif	/* DB_VERSION_MAJOR == 2 */
