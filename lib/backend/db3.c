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

static int cvtdberr(dbiIndex dbi, const char * msg, int error, int printit)
{
    if (printit && error) {
	int db_api = dbi->dbi_rpmdb->db_api;
	if (msg)
	    rpmlog(RPMLOG_ERR, _("db%d error(%d) from %s: %s\n"),
		db_api, error, msg, db_strerror(error));
	else
	    rpmlog(RPMLOG_ERR, _("db%d error(%d): %s\n"),
		db_api, error, db_strerror(error));
    }
    return error;
}

static int db_fini(dbiIndex dbi, const char * dbhome)
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    DB_ENV * dbenv = rpmdb->db_dbenv;
    int rc;

    if (dbenv == NULL)
	return 0;

    rc = dbenv->close(dbenv, 0);
    rc = cvtdberr(dbi, "dbenv->close", rc, _debug);

    rpmlog(RPMLOG_DEBUG, "closed   db environment %s\n", dbhome);

    if (rpmdb->db_remove_env) {
	int xx;

	xx = db_env_create(&dbenv, 0);
	xx = cvtdberr(dbi, "db_env_create", xx, _debug);
	xx = dbenv->remove(dbenv, dbhome, 0);
	/* filter out EBUSY as it just means somebody else gets to clean it */
	xx = cvtdberr(dbi, "dbenv->remove", xx, (xx == EBUSY ? 0 : _debug));

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

static int db_init(dbiIndex dbi, const char * dbhome, DB_ENV ** dbenvp)
{
    DB_ENV *dbenv = NULL;
    int rc, xx;
    int retry_open = 2;

    if (dbenvp == NULL)
	return 1;

    rc = db_env_create(&dbenv, 0);
    rc = cvtdberr(dbi, "db_env_create", rc, _debug);
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
	    (dbi->dbi_verbose & DB_VERB_DEADLOCK));
    dbenv->set_verbose(dbenv, DB_VERB_RECOVERY,
	    (dbi->dbi_verbose & DB_VERB_RECOVERY));
    dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR,
	    (dbi->dbi_verbose & DB_VERB_WAITSFOR));

    if (dbi->dbi_mmapsize) {
	xx = dbenv->set_mp_mmapsize(dbenv, dbi->dbi_mmapsize);
	xx = cvtdberr(dbi, "dbenv->set_mp_mmapsize", xx, _debug);
    }

    if (dbi->dbi_cachesize) {
	xx = dbenv->set_cachesize(dbenv, 0, dbi->dbi_cachesize, 0);
	xx = cvtdberr(dbi, "dbenv->set_cachesize", xx, _debug);
    }

    if (dbi->dbi_no_fsync) {
	xx = db_env_set_func_fsync(fsync_disable);
	xx = cvtdberr(dbi, "db_env_set_func_fsync", xx, _debug);
    }

    /*
     * Actually open the environment. Fall back to private environment
     * if we dont have permission to join/create shared environment.
     */
    while (retry_open) {
	char *fstr = prDbiOpenFlags(dbi->dbi_eflags, 1);
	rpmlog(RPMLOG_DEBUG, "opening  db environment %s %s\n", dbhome, fstr);
	free(fstr);

	rc = (dbenv->open)(dbenv, dbhome,
			   dbi->dbi_eflags, dbi->dbi_rpmdb->db_perms);
	if (rc == EACCES) {
	    dbi->dbi_eflags |= DB_PRIVATE;
	    retry_open--;
	} else {
	    retry_open = 0;
	}
    }
    rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
    if (rc)
	goto errxit;

    /* stale lock removal */
    rc = dbenv->failchk(dbenv, 0);
    rc = cvtdberr(dbi, "dbenv->failchk", rc, _debug);
    if (rc)
	goto errxit;

    *dbenvp = dbenv;

    return 0;

errxit:
    if (dbenv) {
	int xx;
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
    }
    return rc;
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

   /* XXX DB_WRITECURSOR cannot be used with sunrpc dbenv. */
    assert(db != NULL);
    if ((dbiflags & DB_WRITECURSOR) &&
	(dbi->dbi_eflags & DB_INIT_CDB) && !(dbi->dbi_oflags & DB_RDONLY))
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

int dbiVerify(dbiIndex dbi, unsigned int flags)
{
    dbi->dbi_verify_on_close = 1;
    return dbiClose(dbi, flags);
}

int dbiClose(dbiIndex dbi, unsigned int flags)
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    const char * dbhome = rpmdbHome(rpmdb);
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

    if (rpmdb->db_dbenv != NULL && rpmdb->db_use_env) {
	if (rpmdb->db_opens == 1) {
	    xx = db_fini(dbi, (dbhome ? dbhome : ""));
	    rpmdb->db_dbenv = NULL;
	}
	rpmdb->db_opens--;
    }

    if (dbi->dbi_verify_on_close) {
	DB_ENV * dbenv = NULL;

	rc = db_env_create(&dbenv, 0);
	rc = cvtdberr(dbi, "db_env_create", rc, _debug);
	if (rc || dbenv == NULL) goto exit;

	dbenv->set_errcall(dbenv, NULL);
	dbenv->set_errpfx(dbenv, _errpfx);
	xx = dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK,
		(dbi->dbi_verbose & DB_VERB_DEADLOCK));
	xx = dbenv->set_verbose(dbenv, DB_VERB_RECOVERY,
		(dbi->dbi_verbose & DB_VERB_RECOVERY));
	xx = dbenv->set_verbose(dbenv, DB_VERB_WAITSFOR,
		(dbi->dbi_verbose & DB_VERB_WAITSFOR));

	rc = (dbenv->open)(dbenv, dbhome,
            DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE, 0);
	rc = cvtdberr(dbi, "dbenv->open", rc, _debug);
	if (rc) goto exit;

	rc = db_create(&db, dbenv, 0);
	rc = cvtdberr(dbi, "db_create", rc, _debug);

	if (db != NULL) {
		char * dbf = rpmGetPath(dbhome, "/", dbi->dbi_file, NULL);

		rc = db->verify(db, dbf, NULL, NULL, flags);
		rc = cvtdberr(dbi, "db->verify", rc, _debug);

		rpmlog(RPMLOG_DEBUG, "verified db index       %s/%s\n",
			(dbhome ? dbhome : ""), dbi->dbi_file);

	        /*
		 * The DB handle may not be accessed again after
		 * DB->verify is called, regardless of its return.
		 */
		db = NULL;
		dbf = _free(dbf);
	}
	xx = dbenv->close(dbenv, 0);
	xx = cvtdberr(dbi, "dbenv->close", xx, _debug);
	if (rc == 0 && xx) rc = xx;
    }

exit:
    dbi->dbi_db = NULL;

    dbi = dbiFree(dbi);

    return rc;
}

int dbiOpen(rpmdb rpmdb, rpmTag rpmtag, dbiIndex * dbip)
{
    const char *dbhome = rpmdbHome(rpmdb);
    dbiIndex dbi = NULL;
    int rc = 0;
    int xx;

    DB * db = NULL;
    DB_ENV * dbenv = NULL;
    uint32_t oflags;
    int _printit;

    if (dbip)
	*dbip = NULL;

    /*
     * Parse db configuration parameters.
     */
    if ((dbi = dbiNew(rpmdb, rpmtag)) == NULL)
	return 1;

    oflags = dbi->dbi_oflags;

    /*
     * Map open mode flags onto configured database/environment flags.
     */
    if ((rpmdb->db_mode & O_ACCMODE) == O_RDONLY) oflags |= DB_RDONLY;
    if (rpmdb->db_mode & O_CREAT) {
	oflags |= DB_CREATE;
	dbi->dbi_oflags |= DB_CREATE;
    }

    /*
     * Avoid incompatible DB_CREATE/DB_RDONLY flags on DB->open.
     */
    if ((oflags & DB_CREATE) && (oflags & DB_RDONLY)) {
	/* dbhome is writable, and DB->open flags may conflict. */
	char * dbf = rpmGetPath(dbhome, "/", dbi->dbi_file, NULL);

	if (access(dbf, F_OK) == -1) {
	    /* File does not exist, DB->open might create ... */
	    oflags &= ~DB_RDONLY;
	} else {
	    /* File exists, DB->open need not create ... */
	    oflags &= ~DB_CREATE;
	}

	/* Only writers need DB_WRITECURSOR ... */
	if (!(oflags & DB_RDONLY) && access(dbf, W_OK) == 0) {
	    dbi->dbi_oflags &= ~DB_RDONLY;
	} else {
	    dbi->dbi_oflags |= DB_RDONLY;
	}
	dbf = _free(dbf);
    }

    /*
     * Turn off verify-on-close if opening read-only.
     */
    if (oflags & DB_RDONLY)
	dbi->dbi_verify_on_close = 0;

    if (rpmdb->db_use_env) {
	if (rpmdb->db_dbenv == NULL) {
	    rc = db_init(dbi, dbhome, &dbenv);
	    if (rc == 0) {
		rpmdb->db_dbenv = dbenv;
		rpmdb->db_opens = 1;
	    }
	} else {
	    dbenv = rpmdb->db_dbenv;
	    rpmdb->db_opens++;
	}
    }

    {	char *dbiflags = prDbiOpenFlags(oflags, 0);
    	rpmlog(RPMLOG_DEBUG, "opening  db index       %s/%s %s mode=0x%x\n",
		dbhome, dbi->dbi_file, dbiflags, rpmdb->db_mode);
	free(dbiflags);
    }

    if (rc == 0) {
	static int _lockdbfd = 0;

	rc = db_create(&db, dbenv, 0);
	rc = cvtdberr(dbi, "db_create", rc, _debug);
	if (rc == 0 && db != NULL) {
	    if (rc == 0 && !rpmdb->db_use_env) {
		rc = db->set_alloc(db, rmalloc, rrealloc, NULL);
		rc = cvtdberr(dbi, "db->set_alloc", rc, _debug);
	    }

	    if (rc == 0 && !rpmdb->db_use_env && dbi->dbi_cachesize) {
		rc = db->set_cachesize(db, 0, dbi->dbi_cachesize, 0);
		rc = cvtdberr(dbi, "db->set_cachesize", rc, _debug);
	    }

	    if (rc == 0 && dbi->dbi_pagesize) {
		rc = db->set_pagesize(db, dbi->dbi_pagesize);
		rc = cvtdberr(dbi, "db->set_pagesize", rc, _debug);
	    }

	    if (rc == 0) {
		const char *dbpath = dbi->dbi_file;
		char *fullpath = NULL;
		/* When not in environment, absolute path is needed */
		if (!rpmdb->db_use_env) {
		    fullpath = rpmGetPath(dbhome, "/", dbi->dbi_file, NULL);
		    dbpath = fullpath;
		}
		rc = (db->open)(db, NULL, dbpath, NULL,
		    dbi->dbi_dbtype, oflags, rpmdb->db_perms);

		if (rc == 0 && dbi->dbi_dbtype == DB_UNKNOWN) {
		    DBTYPE dbi_dbtype = DB_UNKNOWN;
		    xx = db->get_type(db, &dbi_dbtype);
		    if (xx == 0)
			dbi->dbi_dbtype = dbi_dbtype;
		}
		free(fullpath);
	    }

	    /* XXX return rc == errno without printing */
	    _printit = (rc > 0 ? 0 : _debug);
	    xx = cvtdberr(dbi, "db->open", rc, _printit);

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
	    if (rc == 0 && dbi->dbi_lockdbfd &&
		(!rpmdb->db_use_env || _lockdbfd++ == 0))
	    {
		int fdno = -1;

		if (!(db->fd(db, &fdno) == 0 && fdno >= 0)) {
		    rc = 1;
		} else {
		    struct flock l;
		    memset(&l, 0, sizeof(l));
		    l.l_whence = 0;
		    l.l_start = 0;
		    l.l_len = 0;
		    l.l_type = (rpmdb->db_mode & O_ACCMODE) == O_RDONLY
				? F_RDLCK : F_WRLCK;
		    l.l_pid = 0;

		    rc = fcntl(fdno, F_SETLK, (void *) &l);
		    if (rc) {
			/* Warning iff using non-private CDB locking. */
			rc = ((rpmdb->db_use_env &&
				(dbi->dbi_eflags & DB_INIT_CDB) &&
				!(dbi->dbi_eflags & DB_PRIVATE))
			    ? 0 : 1);
			rpmlog( (rc ? RPMLOG_ERR : RPMLOG_WARNING),
				_("cannot get %s lock on %s/%s\n"),
				((rpmdb->db_mode & O_ACCMODE) == O_RDONLY)
					? _("shared") : _("exclusive"),
				dbhome, dbi->dbi_file);
		    } else {
			rpmlog(RPMLOG_DEBUG,
				"locked   db index       %s/%s\n",
				dbhome, dbi->dbi_file);
		    }
		}
	    }
	}
    }

    dbi->dbi_db = db;

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	*dbip = dbi;
    } else {
	dbi->dbi_verify_on_close = 0;
	(void) dbiClose(dbi, 0);
    }

    return rc;
}
