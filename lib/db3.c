#include "system.h"

static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */
static int _use_cursors = 0;
static int __do_dbcursor_rmw = 0;

#include <db3/db.h>

#include <rpmlib.h>
#include <rpmmacro.h>

#include "rpmdb.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

#if DB_VERSION_MAJOR == 3
#define	__USE_DB3	1

extern int __do_dbenv_remove;	/* XXX in dbindex.c, shared with rebuilddb.c */

#ifdef	DYING
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
#endif

#define	_mymemset(_a, _b, _c)	memset((_a), (_b), (_c))

static inline DBTYPE dbi_to_dbtype(DBI_TYPE dbitype)
{
    switch(dbitype) {
    case DBI_BTREE:	return DB_BTREE;
    case DBI_HASH:	return DB_HASH;
    case DBI_RECNO:	return DB_RECNO;
    case DBI_QUEUE:	return DB_QUEUE;
    case DBI_UNKNOWN:	return DB_UNKNOWN;
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
	
#else	/* __USE_DB3 */
    rc = db_appexit(dbenv);
    rc = cvtdberr(dbi, "db_appexit", rc, _debug);
    free(dbenv);
#endif	/* __USE_DB3 */
    dbi->dbi_dbenv = NULL;
    return rc;
}

static int db_init(dbiIndex dbi, const char *dbhome, int dbflags,
			DB_ENV **dbenvp)
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    DB_ENV *dbenv = NULL;
    int mydbopenflags;
    int rc;

    if (dbenvp == NULL)
	return 1;
    if (rpmdb->db_errfile == NULL)
	rpmdb->db_errfile = stderr;

    rc = db_env_create(&dbenv, 0);
    rc = cvtdberr(dbi, "db_env_create", rc, _debug);
    if (rc)
	goto errxit;

#if defined(__USE_DB3)
  { int xx;
    dbenv->set_errcall(dbenv, rpmdb->db_errcall);
    dbenv->set_errfile(dbenv, rpmdb->db_errfile);
    dbenv->set_errpfx(dbenv, rpmdb->db_errpfx);
 /* dbenv->set_paniccall(???) */
    dbenv->set_verbose(dbenv, DB_VERB_CHKPOINT, rpmdb->db_verbose);
    dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK, rpmdb->db_verbose);
    dbenv->set_verbose(dbenv, DB_VERB_RECOVERY, rpmdb->db_verbose);
    dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR, rpmdb->db_verbose);
 /* dbenv->set_lg_max(???) */
 /* dbenv->set_lk_conflicts(???) */
 /* dbenv->set_lk_detect(???) */
 /* dbenv->set_lk_max(???) */
    xx = dbenv->set_mp_mmapsize(dbenv, rpmdb->db_mp_mmapsize);
    xx = cvtdberr(dbi, "dbenv->set_mp_mmapsize", xx, _debug);
    xx = dbenv->set_cachesize(dbenv, 0, rpmdb->db_mp_size, 0);
    xx = cvtdberr(dbi, "dbenv->set_cachesize", xx, _debug);
 /* dbenv->set_tx_max(???) */
 /* dbenv->set_tx_recover(???) */
  }
#else	/* __USE_DB3 */
    dbenv->db_errcall = rpmdb->db_errcall;
    dbenv->db_errfile = rpmdb->db_errfile;
    dbenv->db_errpfx = rpmdb->db_errpfx;
    dbenv->db_verbose = rpmdb->db_verbose;
    dbenv->mp_mmapsize = rpmdb->db_mp_mmapsize;	/* XXX default is 10 Mb */
    dbenv->mp_size = rpmdb->db_mp_size;		/* XXX default is 128 Kb */
#endif	/* __USE_DB3 */

#define _DBFMASK	(DB_CREATE)
    mydbopenflags = (dbflags & _DBFMASK) | dbiOpenFlags();

#if defined(__USE_DB3)
    rc = dbenv->open(dbenv, dbhome, NULL, mydbopenflags, dbi->dbi_perms);
    rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
    if (rc)
	goto errxit;
#else	/* __USE_DB3 */
    rc = db_appinit(dbhome, NULL, dbenv, mydbopenflags);
    rc = cvtdberr(dbi, "db_appinit", rc, _debug);
    if (rc)
	goto errxit;
#endif	/* __USE_DB3 */

    *dbenvp = dbenv;
if (_debug < 0)
fprintf(stderr, "*** dbenv: %s (%s)\n", dbhome, prDbiOpenFlags(mydbopenflags));

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

static int db3sync(dbiIndex dbi, unsigned int flags)
{
    DB * db = GetDB(dbi);
    int rc;

#if defined(__USE_DB2) || defined(__USE_DB3)
    rc = db->sync(db, flags);
    rc = cvtdberr(dbi, "db->sync", rc, _debug);
#else	/* __USE_DB2 || __USE_DB3 */
    rc = db->sync(db, flags);
#endif	/* __USE_DB2 || __USE_DB3 */

    return rc;
}

static int db3c_del(dbiIndex dbi, DBC * dbcursor, u_int32_t flags)
{
    int rc;

    rc = dbcursor->c_del(dbcursor, flags);
    rc = cvtdberr(dbi, "dbcursor->c_del", rc, _debug);
    return rc;
}

static int db3c_dup(dbiIndex dbi, DBC * dbcursor, DBC ** dbcp, u_int32_t flags)
{
    int rc;

    rc = dbcursor->c_dup(dbcursor, dbcp, flags);
    rc = cvtdberr(dbi, "dbcursor->c_dup", rc, _debug);
    return rc;
}

static int db3c_get(dbiIndex dbi, DBC * dbcursor,
	DBT * key, DBT * data, u_int32_t flags)
{
    int rc;
    int _printit;

    rc = dbcursor->c_get(dbcursor, key, data, flags);
    _printit = (rc == DB_NOTFOUND ? 0 : _debug);
    rc = cvtdberr(dbi, "dbcursor->c_get", rc, _printit);
    return rc;
}

static int db3c_put(dbiIndex dbi, DBC * dbcursor,
	DBT * key, DBT * data, u_int32_t flags)
{
    int rc;

    rc = dbcursor->c_put(dbcursor, key, data, flags);
    rc = cvtdberr(dbi, "dbcursor->c_put", rc, _debug);
    return rc;
}

static int db3c_close(dbiIndex dbi, DBC * dbcursor)
{
    int rc;

    rc = dbcursor->c_close(dbcursor);
    rc = cvtdberr(dbi, "dbcursor->c_close", rc, _debug);
    return rc;
}

static int db3c_open(dbiIndex dbi, DB_TXN * txnid, DBC ** dbcp, u_int32_t flags)
{
    DB * db = GetDB(dbi);
    int rc;

#if defined(__USE_DB3)
    rc = db->cursor(db, txnid, dbcp, flags);
#else	/* __USE_DB3 */
    rc = db->cursor(db, txnid, dbcp);
#endif	/* __USE_DB3 */
    rc = cvtdberr(dbi, "db3c_open", rc, _debug);
    return rc;
}

union _dbswap {
    unsigned int ui;
    unsigned char uc[4];
};

#define	_DBSWAP(_a) \
  { unsigned char _b, *_c = (_a).uc; \
    _b = _c[3]; _c[3] = _c[0]; _c[0] = _b; \
    _b = _c[2]; _c[2] = _c[1]; _c[1] = _b; \
  }

static int db3SearchIndex(dbiIndex dbi, const void * str, size_t len,
		dbiIndexSet * set)
{
    DBT key, data;
    DB * db = GetDB(dbi);
    int rc;

    if (set) *set = NULL;
    if (len == 0) len = strlen(str);
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    key.data = (void *)str;
    key.size = len;
    data.data = NULL;
    data.size = 0;

#if defined(__USE_DB2) || defined(__USE_DB3)
  { DB_TXN * txnid = NULL;
    if (!_use_cursors) {
	int _printit;
	rc = db->get(db, txnid, &key, &data, 0);
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "db->get", rc, _printit);
    } else {
	DBC * dbcursor;
	int xx;

	rc = db3c_open(dbi, txnid, &dbcursor, 0);
	if (rc)
	    return rc;

	/* XXX TODO: loop over duplicates */
	rc = db3c_get(dbi, dbcursor, &key, &data, DB_SET);

	xx = db3c_close(dbi, dbcursor);
    }
  }
#else	/* __USE_DB2 || __USE_DB3 */
    rc = db->get(db, &key, &data, 0);
#endif	/* __USE_DB2 || __USE_DB3 */

    if (rc == 0 && set) {
	int _dbbyteswapped = db->get_byteswapped(db);
	const char * sdbir = data.data;
	int i;

	*set = xmalloc(sizeof(**set));

	/* Convert to database internal format */
	switch (dbi->dbi_jlen) {
	case 2*sizeof(int_32):
	    (*set)->count = data.size / (2*sizeof(int_32));
	    (*set)->recs = xmalloc((*set)->count * sizeof(*((*set)->recs)));
	    for (i = 0; i < (*set)->count; i++) {
		union _dbswap recOffset, fileNumber;

		memcpy(&recOffset.ui, sdbir, sizeof(recOffset.ui));
		sdbir += sizeof(recOffset.ui);
		memcpy(&fileNumber.ui, sdbir, sizeof(fileNumber.ui));
		sdbir += sizeof(fileNumber.ui);
		if (_dbbyteswapped) {
		    _DBSWAP(recOffset);
		    _DBSWAP(fileNumber);
		}
		(*set)->recs[i].recOffset = recOffset.ui;
		(*set)->recs[i].fileNumber = fileNumber.ui;
		(*set)->recs[i].fpNum = 0;
		(*set)->recs[i].dbNum = 0;
	    }
	    break;
	default:
	case 1*sizeof(int_32):
	    (*set)->count = data.size / (1*sizeof(int_32));
	    (*set)->recs = xmalloc((*set)->count * sizeof(*((*set)->recs)));
	    for (i = 0; i < (*set)->count; i++) {
		union _dbswap recOffset;

		memcpy(&recOffset.ui, sdbir, sizeof(recOffset.ui));
		sdbir += sizeof(recOffset.ui);
		if (_dbbyteswapped) {
		    _DBSWAP(recOffset);
		}
		(*set)->recs[i].recOffset = recOffset.ui;
		(*set)->recs[i].fileNumber = 0;
		(*set)->recs[i].fpNum = 0;
		(*set)->recs[i].dbNum = 0;
	    }
	    break;
	}
    }
    return rc;
}

/*@-compmempass@*/
static int db3UpdateIndex(dbiIndex dbi, const char * str, dbiIndexSet set)
{
    DB * db = GetDB(dbi);
    DBT key;
    DBT data;
    DB_TXN * txnid = NULL;
    int rc;

    _mymemset(&key, 0, sizeof(key));
    key.data = (void *)str;
    key.size = strlen(str);
    _mymemset(&data, 0, sizeof(data));

    if (set->count) {
	char * tdbir;
	int i;
	int _dbbyteswapped = 0;

#if defined(__USE_DB3)
	_dbbyteswapped = db->get_byteswapped(db);
#endif	/* __USE_DB3 */

	/* Convert to database internal format */

	switch (dbi->dbi_jlen) {
	case 2*sizeof(int_32):
	    data.size = set->count * (2 * sizeof(int_32));
	    data.data = tdbir = alloca(data.size);
	    for (i = 0; i < set->count; i++) {
		union _dbswap recOffset, fileNumber;

		recOffset.ui = set->recs[i].recOffset;
		fileNumber.ui = set->recs[i].fileNumber;
		if (_dbbyteswapped) {
		    _DBSWAP(recOffset);
		    _DBSWAP(fileNumber);
		}
		memcpy(tdbir, &recOffset.ui, sizeof(recOffset.ui));
		tdbir += sizeof(recOffset.ui);
		memcpy(tdbir, &fileNumber.ui, sizeof(fileNumber.ui));
		tdbir += sizeof(fileNumber.ui);
	    }
	    break;
	default:
	case 1*sizeof(int_32):
	    data.size = set->count * (1 * sizeof(int_32));
	    data.data = tdbir = alloca(data.size);
	    for (i = 0; i < set->count; i++) {
		union _dbswap recOffset;

		recOffset.ui = set->recs[i].recOffset;
		if (_dbbyteswapped) {
		    _DBSWAP(recOffset);
		}
		memcpy(tdbir, &recOffset.ui, sizeof(recOffset.ui));
		tdbir += sizeof(recOffset.ui);
	    }
	    break;
	}

#if defined(__USE_DB2) || defined(__USE_DB3)
	if (!_use_cursors) {
	    rc = db->put(db, txnid, &key, &data, 0);
	    rc = cvtdberr(dbi, "db->put", rc, _debug);
	} else {
	    DBC * dbcursor;

	    rc = db3c_open(dbi, txnid, &dbcursor, 0);
	    if (rc)
		return rc;

	    /* XXX TODO: loop over duplicates */
	    rc = db3c_put(dbi, dbcursor, &key, &data, DB_KEYLAST);

	    (void) db3c_close(dbi, dbcursor);
	}
#else	/* __USE_DB2 || __USE_DB3 */
	rc = db->put(db, &key, &data, 0);
#endif	/* __USE_DB2 || __USE_DB3 */

    } else {

#if defined(__USE_DB2) || defined(__USE_DB3)
	if (!_use_cursors) {
	    rc = db->del(db, txnid, &key, 0);
	    rc = cvtdberr(dbi, "db->del", rc, _debug);
	} else {
	    DBC * dbcursor;

	    rc = db3c_open(dbi, txnid, &dbcursor, 0);
	    if (rc)
		return rc;

	    rc = db3c_get(dbi, dbcursor, &key, &data, DB_RMW | DB_SET);

	    /* XXX TODO: loop over duplicates */
	    rc = db3c_del(dbi, dbcursor, 0);

	    (void) db3c_close(dbi, dbcursor);
	}
#else	/* __USE_DB2 || __USE_DB3 */
	rc = db->del(db, &key, 0);
#endif	/* __USE_DB2 || __USE_DB3 */

    }

    return rc;
}
/*@=compmempass@*/

static int db3copen(dbiIndex dbi)
{
    DBC * dbcursor;
    int rc = 0;

    if ((dbcursor = dbi->dbi_dbcursor) == NULL) {
	DB_TXN * txnid = NULL;
	rc = db3c_open(dbi, txnid, &dbcursor, 0);
	if (rc == 0)
	    dbi->dbi_dbcursor = dbcursor;
    }
    dbi->dbi_lastoffset = 0;
    return rc;
}

static int db3cclose(dbiIndex dbi)
{
    DBC * dbcursor;
    int rc = 0;

    if ((dbcursor = dbi->dbi_dbcursor) != NULL) {
	rc = db3c_close(dbi, dbcursor);
	dbi->dbi_dbcursor = NULL;
    }
    if ((dbcursor = dbi->dbi_dbjoin) != NULL) {
	rc = db3c_close(dbi, dbcursor);
	dbi->dbi_dbjoin = NULL;
    }
    dbi->dbi_lastoffset = 0;
    return rc;
}

static int db3join(dbiIndex dbi)
{
    DB * db = GetDB(dbi);
    int rc;

    {	DBC * dbjoin;
	DBC * dbcs[2];
	dbcs[0] = dbi->dbi_dbcursor;
	dbcs[1] = NULL;
	rc = db->join(db, dbcs, &dbjoin, 0);
	rc = cvtdberr(dbi, "db->join", rc, _debug);
	if (rc == 0)
	    dbi->dbi_dbjoin = dbjoin;
    }

    return rc;
}

static int db3cget(dbiIndex dbi, void ** keyp, size_t * keylen,
		void ** datap, size_t * datalen)
{
    DBT key, data;
    int rc;

    if (keyp) *keyp = NULL;
    if (keylen) *keylen = 0;
    if (datap) *datap = NULL;
    if (datalen) *datalen = 0;
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    {	DBC * dbcursor;

	if ((dbcursor = dbi->dbi_dbcursor) == NULL) {
	    rc = db3copen(dbi);
	    if (rc)
		return rc;
	    dbcursor = dbi->dbi_dbcursor;
	}

	/* XXX db3 does DB_FIRST on uninitialized cursor */
	rc = db3c_get(dbi, dbcursor, &key, &data, DB_NEXT);
	if (rc == 0) {
	    if (keyp)
		*keyp = key.data;
	    if (keylen)
		*keylen = key.size;
	    if (datap)
		*datap = data.data;
	    if (datalen)
		*datalen = data.size;
	} else if (rc > 0) {	/* DB_NOTFOUND */
	    (void) db3cclose(dbi);
	}
    }

    return rc;
}

static int db3get(dbiIndex dbi, void * keyp, size_t keylen,
		void ** datap, size_t * datalen)
{
    DB_TXN * txnid = NULL;
    DBT key, data;
    DB * db = GetDB(dbi);
    int rc;

    if (datap) *datap = NULL;
    if (datalen) *datalen = 0;
    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    {	int _printit;
	key.data = keyp;
	key.size = keylen;
	rc = db->get(db, txnid, &key, &data, 0);
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "db->get", rc, _printit);
    }

    if (rc == 0) {
	if (datap)
	    *datap = data.data;
	if (datalen)
	    *datalen = data.size;
    }

    return rc;
}

static int db3put(dbiIndex dbi, void * keyp, size_t keylen,
		void * datap, size_t datalen)
{
    DB_TXN * txnid = NULL;
    DBT key, data;
    DB * db = GetDB(dbi);
    int rc;

    _mymemset(&key, 0, sizeof(key));
    _mymemset(&data, 0, sizeof(data));

    key.data = keyp;
    key.size = keylen;
    data.data = datap;
    data.size = datalen;

    rc = db->put(db, txnid, &key, &data, 0);
    rc = cvtdberr(dbi, "db->get", rc, _debug);

    return rc;
}

static int db3del(dbiIndex dbi, void * keyp, size_t keylen)
{
    DB_TXN * txnid = NULL;
    DBT key;
    DB * db = GetDB(dbi);
    int rc;

    _mymemset(&key, 0, sizeof(key));

    key.data = keyp;
    key.size = keylen;

    rc = db->del(db, txnid, &key, 0);
    rc = cvtdberr(dbi, "db->del", rc, _debug);

    return rc;
}

static int db3close(dbiIndex dbi, unsigned int flags)
{
    DB * db = GetDB(dbi);
    int rc = 0, xx;

#if defined(__USE_DB2) || defined(__USE_DB3)

    if (dbi->dbi_dbcursor)
	db3cclose(dbi);

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

#else	/* __USE_DB2 || __USE_DB3 */

    rc = db->close(db);

#endif	/* __USE_DB2 || __USE_DB3 */
    dbi->dbi_db = NULL;

    return rc;
}

static int db3open(dbiIndex dbi)
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    int rc = 0;

#if defined(__USE_DB2) || defined(__USE_DB3)
    DB * db = NULL;
    char * dbhome;
    char * dbfile;
    u_int32_t dbflags;
    DB_ENV * dbenv = NULL;
    DB_TXN * txnid = NULL;

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
	    if (rpmdb->db_lorder) {
		rc = db->set_lorder(db, rpmdb->db_lorder);
		rc = cvtdberr(dbi, "db->set_lorder", rc, _debug);
	    }
	    if (rpmdb->db_cachesize) {
		rc = db->set_cachesize(db, 0, rpmdb->db_cachesize, 0);
		rc = cvtdberr(dbi, "db->set_cachesize", rc, _debug);
	    }
	    if (rpmdb->db_pagesize) {
		rc = db->set_pagesize(db, rpmdb->db_pagesize);
		rc = cvtdberr(dbi, "db->set_pagesize", rc, _debug);
	    }
	    if (rpmdb->db_malloc) {
		rc = db->set_malloc(db, rpmdb->db_malloc);
		rc = cvtdberr(dbi, "db->set_malloc", rc, _debug);
	    }
	    if (dbflags & DB_CREATE) {
		rc = db->set_h_ffactor(db, rpmdb->db_h_ffactor);
		rc = cvtdberr(dbi, "db->set_h_ffactor", rc, _debug);
		rc = db->set_h_hash(db, rpmdb->db_h_hash_fcn);
		rc = cvtdberr(dbi, "db->set_h_hash", rc, _debug);
		rc = db->set_h_nelem(db, rpmdb->db_h_nelem);
		rc = cvtdberr(dbi, "db->set_h_nelem", rc, _debug);
	    }
	    if (rpmdb->db_h_flags) {
		rc = db->set_flags(db, rpmdb->db_h_flags);
		rc = cvtdberr(dbi, "db->set_flags", rc, _debug);
	    }
	    if (rpmdb->db_h_dup_compare_fcn) {
		rc = db->set_dup_compare(db, rpmdb->db_h_dup_compare_fcn);
		rc = cvtdberr(dbi, "db->set_dup_compare", rc, _debug);
	    }
	    dbi->dbi_dbinfo = NULL;
	    rc = db->open(db, "packages.db3", dbfile, dbi_to_dbtype(dbi->dbi_type),
			dbflags, dbi->dbi_perms);
	    rc = cvtdberr(dbi, "db->open", rc, _debug);

	    __do_dbcursor_rmw = rpmExpandNumeric("%{_db3_dbcursor_rmw}");
	    if (__do_dbcursor_rmw) {
		DBC * dbcursor = NULL;
		int xx;
		xx = db->cursor(db, txnid, &dbcursor,
			((dbflags & DB_RDONLY) ? 0 : DB_WRITECURSOR));
		xx = cvtdberr(dbi, "db->cursor", xx, _debug);
		dbi->dbi_dbcursor = dbcursor;
	    } else
		dbi->dbi_dbcursor = NULL;
	}
#else
      {	DB_INFO * dbinfo = xcalloc(1, sizeof(*dbinfo));
	dbinfo->db_cachesize = rpmdb->db_cachesize;
	dbinfo->db_lorder = rpmdb->db_lorder;
	dbinfo->db_pagesize = rpmdb->db_pagesize;
	dbinfo->db_malloc = rpmdb->db_malloc;
	if (dbflags & DB_CREATE) {
	    dbinfo->h_ffactor = rpmdb->db_h_ffactor;
	    dbinfo->h_hash = rpmdb->db_h_hash_fcn;
	    dbinfo->h_nelem = rpmdb->db_h_nelem;
	    dbinfo->flags = rpmdb->db_h_flags;
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

#else	/* __USE_DB2 || __USE_DB3 */
    void * dbopeninfo = NULL;
    dbi->dbi_db = dbopen(dbfile, dbi->dbi_flags, dbi->dbi_perms,
		dbi_to_dbtype(dbi->dbi_type), dbopeninfo);
#endif	/* __USE_DB2 || __USE_DB3 */

    if (rc == 0 && dbi->dbi_db != NULL) {
	rc = 0;
if (_debug < 0)
fprintf(stderr, "*** db%dopen: %s\n", dbi->dbi_major, dbfile);
    } else {
	rc = 1;
    }

    return rc;
}

struct _dbiVec db3vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db3open, db3close, db3sync, db3SearchIndex, db3UpdateIndex,
    db3del, db3get, db3put, db3copen, db3cclose, db3join, db3cget
};

#endif	/* DB_VERSION_MAJOR == 3 */
