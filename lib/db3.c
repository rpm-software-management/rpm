#include "system.h"

static int _debug = 0;

#include <db3/db.h>

#include <rpmlib.h>
#include <rpmmacro.h>

#include "dbindex.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

#if DB_VERSION_MAJOR == 3
#define	__USE_DB3	1

extern int __do_dbenv_remove;	/* XXX in dbindex.c, shared with rebuilddb.c */

/* XXX dbenv parameters */
static int db_lorder = 0;			/* 0 is native order */
static void (*db_errcall) (const char *db_errpfx, char *buffer) = NULL;
static FILE * db_errfile = NULL;
static const char * db_errpfx = "rpmdb";
static int db_verbose = 1;

static int dbmp_mmapsize = 16 * 1024 * 1024;	/* XXX db2 default: 10 Mb */
static int dbmp_size = 2 * 1024 * 1024;		/* XXX db2 default: 128 Kb */

/* XXX dbinfo parameters */
static int db_cachesize = 0;
static int db_pagesize = 0;			/* 512 - 64K, 0 is fs blksize */
static void * (*db_malloc) (size_t nbytes) = NULL;

static u_int32_t (*dbh_hash) (const void *, u_int32_t) = NULL;
static u_int32_t dbh_ffactor = 0;	/* db1 default: 8 */
static u_int32_t dbh_nelem = 0;		/* db1 default: 1 */
static u_int32_t dbh_flags = 0;

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

typedef struct DBO {
    const char * name;
    int val;
} DBO_t;

DBO_t dbOptions[] = {
#if defined(DB_RECOVER)
    { "recover",	DB_RECOVER },
#endif
#if defined(DB_RECOVER_FATAL)
    { "recover_fatal",	DB_RECOVER_FATAL },
#endif
#if defined(DB_CREATE)
    { "create",		DB_CREATE },
#endif
#if defined(DB_LOCKDOWN)
    { "lockdown",	DB_LOCKDOWN },
#endif
#if defined(DB_INIT_CDB)
    { "cdb",		DB_INIT_CDB },
#endif
#if defined(DB_INIT_LOCK)
    { "lock",		DB_INIT_LOCK },
#endif
#if defined(DB_INIT_LOG)
    { "log",		DB_INIT_LOG },
#endif
#if defined(DB_INIT_MPOOL)
    { "mpool",		DB_INIT_MPOOL },
#endif
#if defined(DB_INIT_TXN)
    { "txn",		DB_INIT_TXN },
#endif
#if defined(DB_NOMMAP)
    { "nommap",		DB_NOMMAP },
#endif
#if defined(DB_PRIVATE)
    { "private",	DB_PRIVATE },
#endif
#if defined(DB_SYSTEM_MEM)
    { "shared",		DB_SYSTEM_MEM },
#endif
#if defined(DB_THREAD)
    { "thread",		DB_THREAD },
#endif
#if defined(DB_TXN_NOSYNC)
    { "txn_nosync",	DB_TXN_NOSYNC },
#endif
    { NULL, 0 }
};

static int dbiOpenFlags(void)
{
    char * dbopts = rpmExpand("%{_db3_flags}", NULL);
    int dbflags = 0;

    if (dbopts && *dbopts && *dbopts != '%') {
	char *o, *oe;
	dbflags = 0;
	for (o = dbopts; o != NULL; o = oe) {
	    DBO_t *dbo;
	    if ((oe = strchr(o, ':')) != NULL)
		*oe++ = '\0';
	    if (*o == '\0')
		continue;
	    for (dbo = dbOptions; dbo->name != NULL; dbo++) {
		if (strcmp(o, dbo->name))
		    continue;
		dbflags |= dbo->val;
		break;
	    }
	    if (dbo->name == NULL)
		fprintf(stderr, _("dbiOpenFlags: unrecognized db option: \"%s\" ignored\n"), o);
	}
    }
    free(dbopts);
    return dbflags;
}

static const char *const prDbiOpenFlags(int dbflags)
{
    static char buf[256];
    DBO_t *dbo;
    char * oe;

    oe = buf;
    *oe = '\0';
    for (dbo = dbOptions; dbo->name != NULL; dbo++) {
	if ((dbflags & dbo->val) != dbo->val)
	    continue;
	if (oe != buf)
	    *oe++ = ':';
	oe = stpcpy(oe, dbo->name);
	dbflags &= ~dbo->val;
    }
    if (dbflags) {
	if (oe != buf)
	    *oe++ = ':';
	    sprintf(oe, "0x%x", dbflags);
    }
    return buf;
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

static int db_fini(dbiIndex dbi)
{
    DB_ENV * dbenv = (DB_ENV *)dbi->dbi_dbenv;
#if defined(__USE_DB3)
    char **dbconfig = NULL;
    char * dbhome;
    char * dbfile;
    int rc;

    if (dbenv == NULL) {
	dbi->dbi_dbenv = NULL;
	return 0;
    }

    dbhome = alloca(strlen(dbi->dbi_file) + 1);
    strcpy(dbhome, dbi->dbi_file);
    dbfile = strrchr(dbhome, '/');
    if (dbfile)
	    *dbfile++ = '\0';
    else
	    dbfile = dbhome;

    rc = dbenv->close(dbenv, 0);
    rc = cvtdberr(dbi, "dbenv->close", rc, _debug);

    if (__do_dbenv_remove < 0)
	__do_dbenv_remove = rpmExpandNumeric("%{_db3_dbenv_remove}");
    if (__do_dbenv_remove) {
	int xx;
	xx = db_env_create(&dbenv, 0);
	xx = cvtdberr(dbi, "db_env_create", rc, _debug);
	xx = dbenv->remove(dbenv, dbhome, dbconfig, 0);
	xx = cvtdberr(dbi, "dbenv->remove", rc, _debug);
    }
	
#else
    rc = db_appexit(dbenv);
    rc = cvtdberr(dbi, "db_appexit", rc, _debug);
    free(dbenv);
#endif
    dbi->dbi_dbenv = NULL;
    return rc;
}

static int db_init(dbiIndex dbi, const char *dbhome, int dbflags,
			DB_ENV **dbenvp)
{
    DB_ENV *dbenv = NULL;
    int mydbopenflags;
    int rc;

    if (dbenvp == NULL)
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

#define _DBFMASK	(DB_CREATE)
    mydbopenflags = (dbflags & _DBFMASK) | dbiOpenFlags();

#if defined(__USE_DB3)
    rc = dbenv->open(dbenv, dbhome, NULL, mydbopenflags, dbi->dbi_perms);
    rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
    if (rc)
	goto errxit;
#else
    rc = db_appinit(dbhome, NULL, dbenv, mydbopenflags);
    rc = cvtdberr(dbi, "db_appinit", rc, _debug);
    if (rc)
	goto errxit;
#endif

    *dbenvp = dbenv;
if (_debug)
fprintf(stderr, "*** dbenv: %s (%s)\n", dbhome, prDbiOpenFlags(mydbopenflags));

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

static int db3close(dbiIndex dbi, unsigned int flags)
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

    xx = db_fini(dbi);

#else

    rc = db->close(db);

#endif
    dbi->dbi_db = NULL;

    return rc;
}

static int db3sync(dbiIndex dbi, unsigned int flags)
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

static int db3GetFirstKey(dbiIndex dbi, const char ** keyp)
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

static int db3SearchIndex(dbiIndex dbi, const char * str, dbiIndexSet * set)
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
#if defined(__USE_DB3)
    if (dbi->dbi_dbcursor) {
	DBC *dbcursor = dbi->dbi_dbcursor;
	rc = dbcursor->c_get(dbcursor, &key, &data, DB_SET);	/* DB_RMW */
    } else
#endif
    rc = db->get(db, NULL, &key, &data, 0);
#if 0
    _printit = (rc == DB_NOTFOUND ? 0 : _debug);
#else
    _printit = _debug;
#endif
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
static int db3UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set)
{
    DBT key;
    DBT data;

    DB * db = GetDB(dbi);
    int rc;

    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));
    key.data = (void *)str;
    key.size = strlen(str);

    if (set->count) {

#if defined(__USE_DB2) || defined(__USE_DB3)
#if defined(__USE_DB3)
	if (dbi->dbi_dbcursor) {
	    DBC *dbcursor = dbi->dbi_dbcursor;
	    int xx;

	    rc = dbcursor->c_get(dbcursor, &key, &data, (DB_SET|DB_RMW));
	    xx = cvtdberr(dbi, "db->c_get RMW put", rc, _debug);
	    data.data = set->recs;
	    data.size = set->count * sizeof(*(set->recs));
	    rc = dbcursor->c_put(dbcursor, &key, &data,
		(rc == DB_NOTFOUND ? DB_KEYFIRST : DB_CURRENT));
	    rc = cvtdberr(dbi, "db->c_put RMW", rc, _debug);
	} else
#endif
	{
	    data.data = set->recs;
	    data.size = set->count * sizeof(*(set->recs));
	    rc = db->put(db, NULL, &key, &data, 0);
	    rc = cvtdberr(dbi, "db->put", rc, _debug);
	}
#else
	rc = db->put(db, &key, &data, 0);
#endif

    } else {

#if defined(__USE_DB2) || defined(__USE_DB3)
#if defined(__USE_DB3)
	if (dbi->dbi_dbcursor) {
	    DBC *dbcursor = dbi->dbi_dbcursor;
	    int xx;

	    rc = dbcursor->c_get(dbcursor, &key, &data, (DB_SET|DB_RMW));
	    xx = cvtdberr(dbi, "db->c_get RMW del", rc, _debug);
	    if (rc != DB_NOTFOUND) {
		rc = dbcursor->c_del(dbcursor, 0);
		rc = cvtdberr(dbi, "db->c_del RMW", rc, _debug);
	    }
	} else
#endif
	{
	    rc = db->del(db, NULL, &key, 0);
	    rc = cvtdberr(dbi, "db->del", rc, _debug);
	}
#else
	rc = db->del(db, &key, 0);
#endif

    }

    return rc;
}
/*@=compmempass@*/

static int db3open(dbiIndex dbi)
{
    int rc = 0;

#if defined(__USE_DB2) || defined(__USE_DB3)
    DB * db = NULL;
    char * dbhome;
    char * dbfile;
    u_int32_t dbflags;
    DB_ENV * dbenv = NULL;
    int __do_dbcursor_rmw = 0;

    dbhome = alloca(strlen(dbi->dbi_file) + 1);
    strcpy(dbhome, dbi->dbi_file);
    dbfile = strrchr(dbhome, '/');
    if (dbfile)
	*dbfile++ = '\0';
    else
	dbfile = dbhome;

    dbflags = (	!(dbi->dbi_flags & O_RDWR) ? DB_RDONLY :
		((dbi->dbi_flags & O_CREAT) ? DB_CREATE : 0));

    rc = db_init(dbi, dbhome, dbflags, &dbenv);
    dbi->dbi_dbinfo = NULL;

    if (rc == 0) {
#if defined(__USE_DB3)
	rc = db_create(&db, dbenv, 0);
	rc = cvtdberr(dbi, "db_create", rc, _debug);
	if (rc == 0) {
	    if (db_lorder) {
		rc = db->set_lorder(db, db_lorder);
		rc = cvtdberr(dbi, "db->set_lorder", rc, _debug);
	    }
	    if (db_cachesize) {
		rc = db->set_cachesize(db, 0, db_cachesize, 0);
		rc = cvtdberr(dbi, "db->set_cachesize", rc, _debug);
	    }
	    if (db_pagesize) {
		rc = db->set_pagesize(db, db_pagesize);
		rc = cvtdberr(dbi, "db->set_pagesize", rc, _debug);
	    }
	    if (db_malloc) {
		rc = db->set_malloc(db, db_malloc);
		rc = cvtdberr(dbi, "db->set_malloc", rc, _debug);
	    }
	    if (dbflags & DB_CREATE) {
		rc = db->set_h_ffactor(db, dbh_ffactor);
		rc = cvtdberr(dbi, "db->set_h_ffactor", rc, _debug);
		rc = db->set_h_hash(db, dbh_hash);
		rc = cvtdberr(dbi, "db->set_h_hash", rc, _debug);
		rc = db->set_h_nelem(db, dbh_nelem);
		rc = cvtdberr(dbi, "db->set_h_nelem", rc, _debug);
		rc = db->set_flags(db, dbh_flags);
		rc = cvtdberr(dbi, "db->set_flags", rc, _debug);
	    }
	    dbi->dbi_dbinfo = NULL;
	    rc = db->open(db, dbfile, NULL, dbi_to_dbtype(dbi->dbi_type),
			dbflags, dbi->dbi_perms);
	    rc = cvtdberr(dbi, "db->open", rc, _debug);

	    __do_dbcursor_rmw = rpmExpandNumeric("%{_db3_dbcursor_rmw}");
	    if (__do_dbcursor_rmw) {
		DBC * dbcursor = NULL;
		int xx;
		xx = db->cursor(db, NULL, &dbcursor,
			((dbflags & DB_RDONLY) ? 0 : DB_WRITECURSOR));
		xx = cvtdberr(dbi, "db->cursor", xx, _debug);
		dbi->dbi_dbcursor = dbcursor;
	    } else
		dbi->dbi_dbcursor = NULL;
	}
#else
      {	DB_INFO * dbinfo = xcalloc(1, sizeof(*dbinfo));
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
	dbi->dbi_dbinfo = dbinfo;
	rc = db_open(dbfile, dbi_to_dbtype(dbi->dbi_type), dbflags,
			dbi->dbi_perms, dbenv, dbinfo, &db);
	rc = cvtdberr(dbi, "db_open", rc, _debug);
      }
#endif	/* __USE_DB3 */
    }

    dbi->dbi_db = db;
    dbi->dbi_dbenv = dbenv;

#else
    dbi->dbi_db = dbopen(dbfile, dbi->dbi_flags, dbi->dbi_perms,
		dbi_to_dbtype(dbi->dbi_type), dbi->dbi_openinfo);
#endif	/* __USE_DB2 || __USE_DB3 */

    if (rc == 0 && dbi->dbi_db != NULL) {
	rc = 0;
if (_debug)
fprintf(stderr, "*** db%dopen: %s\n", dbi->dbi_major, dbfile);
    } else {
	rc = 1;
    }

    return rc;
}

struct _dbiVec db3vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db3open, db3close, db3sync, db3GetFirstKey, db3SearchIndex, db3UpdateIndex
};

#endif	/* DB_VERSION_MAJOR == 3 */
