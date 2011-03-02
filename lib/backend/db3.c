/** \ingroup rpmdb
 * \file lib/db3.c
 */

static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */

#include "system.h"

#include <errno.h>
#include <sys/wait.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "lib/rpmdb_internal.h"

#include "debug.h"

static const char * _errpfx = "rpmdb";

static int dbapi_err(rpmdb rdb, const char * msg, int error, int printit)
{
    if (printit && error) {
	int db_api = rdb->db_ver;
	if (msg)
	    rpmlog(RPMLOG_ERR, _("db%d error(%d) from %s: %s\n"),
		db_api, error, msg, db_strerror(error));
	else
	    rpmlog(RPMLOG_ERR, _("db%d error(%d): %s\n"),
		db_api, error, db_strerror(error));
    }
    return error;
}

static int cvtdberr(dbiIndex dbi, const char * msg, int error, int printit)
{
    return dbapi_err(dbi->dbi_rpmdb, msg, error, printit);
}

static void errlog(const DB_ENV * env, const char *errpfx, const char *msg)
{
    rpmlog(RPMLOG_ERR, "%s: %s\n", errpfx, msg);
}

static uint32_t db_envflags(DB * db)
{
    DB_ENV * env = db->get_env(db);
    uint32_t eflags = 0;
    (void) env->get_open_flags(env, &eflags);
    return eflags;
}

static int db_fini(rpmdb rdb, const char * dbhome)
{
    DB_ENV * dbenv = rdb->db_dbenv;
    int rc;
    uint32_t eflags = 0;

    if (dbenv == NULL)
	return 0;

    if (rdb->db_opens > 1) {
	rdb->db_opens--;
	return 0;
    }

    (void) dbenv->get_open_flags(dbenv, &eflags);
    rc = dbenv->close(dbenv, 0);
    rc = dbapi_err(rdb, "dbenv->close", rc, _debug);

    rpmlog(RPMLOG_DEBUG, "closed   db environment %s\n", dbhome);

    if (!(eflags & DB_PRIVATE) && rdb->db_remove_env) {
	int xx;

	xx = db_env_create(&dbenv, 0);
	xx = dbapi_err(rdb, "db_env_create", xx, _debug);
	xx = dbenv->remove(dbenv, dbhome, 0);
	/* filter out EBUSY as it just means somebody else gets to clean it */
	xx = dbapi_err(rdb, "dbenv->remove", xx, (xx == EBUSY ? 0 : _debug));

	rpmlog(RPMLOG_DEBUG, "removed  db environment %s\n", dbhome);

    }
    return rc;
}

static int fsync_disable(int fd)
{
    return 0;
}

/*
 * dbenv->failchk() callback method for determining is the given pid/tid 
 * is alive. We only care about pid's though. 
 */ 
static int isalive(DB_ENV *dbenv, pid_t pid, db_threadid_t tid, uint32_t flags)
{
    int alive = 0;

    if (pid == getpid()) {
	alive = 1;
    } else if (kill(pid, 0) == 0) {
	alive = 1;
    /* only existing processes can fail with EPERM */
    } else if (errno == EPERM) {
    	alive = 1;
    }
    
    return alive;
}

static int db_init(rpmdb rdb, const char * dbhome)
{
    DB_ENV *dbenv = NULL;
    int rc, xx;
    int retry_open = 2;
    struct _dbConfig * cfg = &rdb->cfg;
    /* This is our setup, thou shall not have other setups before us */
    uint32_t eflags = (DB_CREATE|DB_INIT_MPOOL|DB_INIT_CDB);

    if (rdb->db_dbenv != NULL) {
	rdb->db_opens++;
	return 0;
    }

    /* By no means necessary but speeds things up a bit */
    if (rdb->db_flags & RPMDB_FLAG_REBUILD)
	eflags &= ~DB_INIT_CDB;
    /* XXX Something bizarre with verify... use private environment, no cdb */
    if (rdb->db_flags & RPMDB_FLAG_VERIFYONLY) {
	eflags |= DB_PRIVATE;
	eflags &= ~DB_INIT_CDB;
    }

    rc = db_env_create(&dbenv, 0);
    rc = dbapi_err(rdb, "db_env_create", rc, _debug);
    if (dbenv == NULL || rc)
	goto errxit;

    dbenv->set_alloc(dbenv, rmalloc, rrealloc, NULL);
    dbenv->set_errcall(dbenv, NULL);
    dbenv->set_errpfx(dbenv, _errpfx);

    /* 
     * These enable automatic stale lock removal. 
     * thread_count 8 is some kind of "magic minimum" value...
     */
    dbenv->set_thread_count(dbenv, 8);
    dbenv->set_isalive(dbenv, isalive);

    dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK,
	    (cfg->db_verbose & DB_VERB_DEADLOCK));
    dbenv->set_verbose(dbenv, DB_VERB_RECOVERY,
	    (cfg->db_verbose & DB_VERB_RECOVERY));
    dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR,
	    (cfg->db_verbose & DB_VERB_WAITSFOR));

    if (cfg->db_mmapsize) {
	xx = dbenv->set_mp_mmapsize(dbenv, cfg->db_mmapsize);
	xx = dbapi_err(rdb, "dbenv->set_mp_mmapsize", xx, _debug);
    }

    if (cfg->db_cachesize) {
	xx = dbenv->set_cachesize(dbenv, 0, cfg->db_cachesize, 0);
	xx = dbapi_err(rdb, "dbenv->set_cachesize", xx, _debug);
    }

    /*
     * Actually open the environment. Fall back to private environment
     * if we dont have permission to join/create shared environment.
     */
    while (retry_open) {
	char *fstr = prDbiOpenFlags(eflags, 1);
	rpmlog(RPMLOG_DEBUG, "opening  db environment %s %s\n", dbhome, fstr);
	free(fstr);

	rc = (dbenv->open)(dbenv, dbhome, eflags, rdb->db_perms);
	if (rc == EACCES || rc == EROFS) {
	    eflags |= DB_PRIVATE;
	    retry_open--;
	} else {
	    retry_open = 0;
	}
    }
    rc = dbapi_err(rdb, "dbenv->open", rc, _debug);
    if (rc)
	goto errxit;

    dbenv->set_errcall(dbenv, errlog);

    /* stale lock removal */
    rc = dbenv->failchk(dbenv, 0);
    rc = dbapi_err(rdb, "dbenv->failchk", rc, _debug);
    if (rc)
	goto errxit;

    rdb->db_dbenv = dbenv;
    rdb->db_opens = 1;

    return 0;

errxit:
    if (dbenv) {
	int xx;
	xx = dbenv->close(dbenv, 0);
	xx = dbapi_err(rdb, "dbenv->close", xx, _debug);
    }
    return rc;
}

void dbSetFSync(void *dbenv, int enable)
{
    db_env_set_func_fsync(enable ? fdatasync : fsync_disable);
}

int dbiSync(dbiIndex dbi, unsigned int flags)
{
    DB * db = dbi->dbi_db;
    int rc = 0;

    if (db != NULL && !dbi->dbi_no_dbsync) {
	rc = db->sync(db, flags);
	rc = cvtdberr(dbi, "db->sync", rc, _debug);
    }
    return rc;
}

int dbiCclose(dbiIndex dbi, DBC * dbcursor,
		unsigned int flags)
{
    int rc = -2;

    /* XXX dbiCopen error pathways come through here. */
    if (dbcursor != NULL) {
	rc = dbcursor->c_close(dbcursor);
	rc = cvtdberr(dbi, "dbcursor->c_close", rc, _debug);
    }
    return rc;
}

int dbiCopen(dbiIndex dbi, DBC ** dbcp, unsigned int dbiflags)
{
    DB * db = dbi->dbi_db;
    DBC * dbcursor = NULL;
    int flags;
    int rc;
    uint32_t eflags = db_envflags(db);
    
   /* XXX DB_WRITECURSOR cannot be used with sunrpc dbenv. */
    assert(db != NULL);
    if ((dbiflags & DB_WRITECURSOR) &&
	(eflags & DB_INIT_CDB) && !(dbi->dbi_oflags & DB_RDONLY))
    {
	flags = DB_WRITECURSOR;
    } else
	flags = 0;

    rc = db->cursor(db, NULL, &dbcursor, flags);
    rc = cvtdberr(dbi, "db->cursor", rc, _debug);

    if (dbcp)
	*dbcp = dbcursor;
    else
	(void) dbiCclose(dbi, dbcursor, 0);

    return rc;
}


/* Store (key,data) pair in index database. */
int dbiPut(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
{
    DB * db = dbi->dbi_db;
    int rc;

    assert(key->data != NULL && key->size > 0 && data->data != NULL && data->size > 0);
    assert(db != NULL);

    (void) rpmswEnter(&dbi->dbi_rpmdb->db_putops, (ssize_t) 0);
    if (dbcursor == NULL) {
	rc = db->put(db, NULL, key, data, 0);
	rc = cvtdberr(dbi, "db->put", rc, _debug);
    } else {
	rc = dbcursor->c_put(dbcursor, key, data, DB_KEYLAST);
	rc = cvtdberr(dbi, "dbcursor->c_put", rc, _debug);
    }

    (void) rpmswExit(&dbi->dbi_rpmdb->db_putops, (ssize_t) data->size);
    return rc;
}

int dbiDel(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
	   unsigned int flags)
{
    DB * db = dbi->dbi_db;
    int rc;

    assert(db != NULL);
    assert(key->data != NULL && key->size > 0);
    (void) rpmswEnter(&dbi->dbi_rpmdb->db_delops, 0);

    if (dbcursor == NULL) {
	rc = db->del(db, NULL, key, flags);
	rc = cvtdberr(dbi, "db->del", rc, _debug);
    } else {
	int _printit;

	/* XXX TODO: insure that cursor is positioned with duplicates */
	rc = dbcursor->c_get(dbcursor, key, data, DB_SET);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "dbcursor->c_get", rc, _printit);

	if (rc == 0) {
	    rc = dbcursor->c_del(dbcursor, flags);
	    rc = cvtdberr(dbi, "dbcursor->c_del", rc, _debug);
	}
    }

    (void) rpmswExit(&dbi->dbi_rpmdb->db_delops, data->size);
    return rc;
}

/* Retrieve (key,data) pair from index database. */
int dbiGet(dbiIndex dbi, DBC * dbcursor, DBT * key, DBT * data,
		unsigned int flags)
{

    DB * db = dbi->dbi_db;
    int _printit;
    int rc;

    assert((flags == DB_NEXT) || (key->data != NULL && key->size > 0));
    (void) rpmswEnter(&dbi->dbi_rpmdb->db_getops, 0);

    assert(db != NULL);
    if (dbcursor == NULL) {
	/* XXX duplicates require cursors. */
	rc = db->get(db, NULL, key, data, 0);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "db->get", rc, _printit);
    } else {
	/* XXX db4 does DB_FIRST on uninitialized cursor */
	rc = dbcursor->c_get(dbcursor, key, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "dbcursor->c_get", rc, _printit);
    }

    (void) rpmswExit(&dbi->dbi_rpmdb->db_getops, data->size);
    return rc;
}

int dbiCount(dbiIndex dbi, DBC * dbcursor,
	     unsigned int * countp,
	     unsigned int flags)
{
    db_recno_t count = 0;
    int rc = 0;

    flags = 0;
    rc = dbcursor->c_count(dbcursor, &count, flags);
    rc = cvtdberr(dbi, "dbcursor->c_count", rc, _debug);
    if (rc) return rc;
    if (countp) *countp = count;

    return rc;
}

int dbiByteSwapped(dbiIndex dbi)
{
    DB * db = dbi->dbi_db;
    int rc = 0;

    if (dbi->dbi_byteswapped != -1)
	return dbi->dbi_byteswapped;

    if (db != NULL) {
	int isswapped = 0;
	rc = db->get_byteswapped(db, &isswapped);
	if (rc == 0)
	    dbi->dbi_byteswapped = rc = isswapped;
    }
    return rc;
}

dbiIndexType dbiType(dbiIndex dbi)
{
    return dbi->dbi_type;
}

int dbiFlags(dbiIndex dbi)
{
    DB *db = dbi->dbi_db;
    int flags = DBI_NONE;
    uint32_t oflags = 0;

    if (db && db->get_open_flags(db, &oflags) == 0) {
	if (oflags & DB_CREATE)
	    flags |= DBI_CREATED;
	if (oflags & DB_RDONLY)
	    flags |= DBI_RDONLY;
    }
    return flags;
}

const char * dbiName(dbiIndex dbi)
{
    return dbi->dbi_file;
}

int dbiVerify(dbiIndex dbi, unsigned int flags)
{
    int rc = 0;

    if (dbi && dbi->dbi_db) {
	DB * db = dbi->dbi_db;
	
	rc = db->verify(db, dbi->dbi_file, NULL, NULL, flags);
	rc = cvtdberr(dbi, "db->verify", rc, _debug);

	rpmlog(RPMLOG_DEBUG, "verified db index       %s\n", dbi->dbi_file);

	/* db->verify() destroys the handle, make sure nobody accesses it */
	dbi->dbi_db = NULL;
    }
    return rc;
}

int dbiClose(dbiIndex dbi, unsigned int flags)
{
    rpmdb rdb = dbi->dbi_rpmdb;
    const char * dbhome = rpmdbHome(rdb);
    DB * db = dbi->dbi_db;
    int _printit;
    int rc = 0, xx;

    flags = 0;	/* XXX unused */

    if (db) {
	rc = db->close(db, 0);
	/* XXX ignore not found error messages. */
	_printit = (rc == ENOENT ? 0 : _debug);
	rc = cvtdberr(dbi, "db->close", rc, _printit);
	db = dbi->dbi_db = NULL;

	rpmlog(RPMLOG_DEBUG, "closed   db index       %s/%s\n",
		dbhome, dbi->dbi_file);
    }

    xx = db_fini(rdb, dbhome ? dbhome : "");

    dbi->dbi_db = NULL;

    dbi = dbiFree(dbi);

    return rc;
}

/*
 * Lock a file using fcntl(2). Traditionally this is Packages,
 * the file used to store metadata of installed header(s),
 * as Packages is always opened, and should be opened first,
 * for any rpmdb access.
 *
 * If no DBENV is used, then access is protected with a
 * shared/exclusive locking scheme, as always.
 *
 * With a DBENV, the fcntl(2) lock is necessary only to keep
 * the riff-raff from playing where they don't belong, as
 * the DBENV should provide it's own locking scheme. So try to
 * acquire a lock, but permit failures, as some other
 * DBENV player may already have acquired the lock.
 *
 * With NPTL posix mutexes, revert to fcntl lock on non-functioning
 * glibc/kernel combinations.
 */
static int dbiFlock(dbiIndex dbi, int mode)
{
    int fdno = -1;
    int rc = 0;
    DB * db = dbi->dbi_db;

    if (!(db->fd(db, &fdno) == 0 && fdno >= 0)) {
	rc = 1;
    } else {
	const char *dbhome = rpmdbHome(dbi->dbi_rpmdb);
	struct flock l;
	memset(&l, 0, sizeof(l));
	l.l_whence = 0;
	l.l_start = 0;
	l.l_len = 0;
	l.l_type = (mode & O_ACCMODE) == O_RDONLY
		    ? F_RDLCK : F_WRLCK;
	l.l_pid = 0;

	rc = fcntl(fdno, F_SETLK, (void *) &l);
	if (rc) {
	    uint32_t eflags = db_envflags(db);
	    /* Warning iff using non-private CDB locking. */
	    rc = (((eflags & DB_INIT_CDB) && !(eflags & DB_PRIVATE)) ? 0 : 1);
	    rpmlog( (rc ? RPMLOG_ERR : RPMLOG_WARNING),
		    _("cannot get %s lock on %s/%s\n"),
		    ((mode & O_ACCMODE) == O_RDONLY)
			    ? _("shared") : _("exclusive"),
		    dbhome, dbi->dbi_file);
	} else {
	    rpmlog(RPMLOG_DEBUG,
		    "locked   db index       %s/%s\n",
		    dbhome, dbi->dbi_file);
	}
    }
    return rc;
}

int dbiOpen(rpmdb rdb, rpmDbiTagVal rpmtag, dbiIndex * dbip, int flags)
{
    const char *dbhome = rpmdbHome(rdb);
    dbiIndex dbi = NULL;
    int rc = 0;
    int retry_open;
    int verifyonly = (flags & RPMDB_FLAG_VERIFYONLY);

    DB * db = NULL;
    DBTYPE dbtype = DB_UNKNOWN;
    uint32_t oflags;
    static int _lockdbfd = 0;

    if (dbip)
	*dbip = NULL;

    /*
     * Parse db configuration parameters.
     */
    if ((dbi = dbiNew(rdb, rpmtag)) == NULL)
	return 1;

    oflags = dbi->dbi_oflags;

    /*
     * Map open mode flags onto configured database/environment flags.
     */
    if ((rdb->db_mode & O_ACCMODE) == O_RDONLY) oflags |= DB_RDONLY;

    rc = db_init(rdb, dbhome);

    retry_open = (rc == 0) ? 2 : 0;

    while (retry_open) {
	rc = db_create(&db, rdb->db_dbenv, 0);
	rc = cvtdberr(dbi, "db_create", rc, _debug);

	/* For verify we only want the handle, not an open db */
	if (verifyonly)
	    break;

	if (rc == 0 && db != NULL) {
	    int _printit, xx;
	    char *dbfs = prDbiOpenFlags(oflags, 0);
	    rpmlog(RPMLOG_DEBUG, "opening  db index       %s/%s %s mode=0x%x\n",
		    dbhome, dbi->dbi_file, dbfs, rdb->db_mode);
	    free(dbfs);

	    rc = (db->open)(db, NULL, dbi->dbi_file, NULL,
			    dbtype, oflags, rdb->db_perms);

	    /* Attempt to create if missing, discarding DB_RDONLY (!) */
	    if (rc == ENOENT) {
		oflags |= DB_CREATE;
		oflags &= ~DB_RDONLY;
		dbtype = (dbiType(dbi) == DBI_PRIMARY) ?  DB_HASH : DB_BTREE;
		retry_open--;
	    } else {
		retry_open = 0;
	    }

	    /* XXX return rc == errno without printing */
	    _printit = (rc > 0 ? 0 : _debug);
	    xx = cvtdberr(dbi, "db->open", rc, _printit);

	    /* Validate the index type is something we can support */
	    if ((rc == 0) && (dbtype == DB_UNKNOWN)) {
		db->get_type(db, &dbtype);
		if (dbtype != DB_HASH && dbtype != DB_BTREE) {
		    rpmlog(RPMLOG_ERR, _("invalid index type %x on %s/%s\n"),
				dbtype, dbhome, dbi->dbi_file);
		    rc = 1;
		}
	    }

	    if (rc != 0) {
		db->close(db, 0);
		db = NULL;
	    }
	}
    }

    dbi->dbi_db = db;
    dbi->dbi_oflags = oflags;

    if (!verifyonly && rc == 0 && dbi->dbi_lockdbfd && _lockdbfd++ == 0) {
	rc = dbiFlock(dbi, rdb->db_mode);
    }

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	*dbip = dbi;
    } else {
	(void) dbiClose(dbi, 0);
    }

    return rc;
}
