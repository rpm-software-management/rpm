/** \ingroup rpmdb
 * \file lib/db3.c
 */

static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */

#include "system.h"

#include <errno.h>
#include <sys/wait.h>
#include <popt.h>
#include <db.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "lib/rpmdb_internal.h"

#include "debug.h"

static const char * _errpfx = "rpmdb";

struct dbiCursor_s {
    dbiIndex dbi;
    const void *key;
    unsigned int keylen;
    int flags;
    DBC *cursor;
};

static struct dbiConfig_s staticdbicfg;
static struct dbConfig_s staticcfg;

/** \ingroup dbi
 */
static const struct poptOption rdbOptions[] = {
 /* Environment options */
   
 { "cdb",	0,POPT_BIT_SET,	&staticcfg.db_eflags, DB_INIT_CDB,
	NULL, NULL },
 { "lock",	0,POPT_BIT_SET,	&staticcfg.db_eflags, DB_INIT_LOCK,
	NULL, NULL },
 { "log",	0,POPT_BIT_SET,	&staticcfg.db_eflags, DB_INIT_LOG,
	NULL, NULL },
 { "txn",	0,POPT_BIT_SET,	&staticcfg.db_eflags, DB_INIT_TXN,
	NULL, NULL },
 { "recover",	0,POPT_BIT_SET,	&staticcfg.db_eflags, DB_RECOVER,
	NULL, NULL },
 { "recover_fatal", 0,POPT_BIT_SET,	&staticcfg.db_eflags, DB_RECOVER_FATAL,
	NULL, NULL },
 { "lockdown",	0,POPT_BIT_SET,	&staticcfg.db_eflags, DB_LOCKDOWN,
	NULL, NULL },
 { "private",	0,POPT_BIT_SET,	&staticcfg.db_eflags, DB_PRIVATE,
	NULL, NULL },

 { "deadlock",	0,POPT_BIT_SET,	&staticcfg.db_verbose, DB_VERB_DEADLOCK,
	NULL, NULL },
 { "recovery",	0,POPT_BIT_SET,	&staticcfg.db_verbose, DB_VERB_RECOVERY,
	NULL, NULL },
 { "waitsfor",	0,POPT_BIT_SET,	&staticcfg.db_verbose, DB_VERB_WAITSFOR,
	NULL, NULL },
 { "verbose",	0,POPT_ARG_VAL,		&staticcfg.db_verbose, -1,
	NULL, NULL },

 { "cachesize",	0,POPT_ARG_INT,		&staticcfg.db_cachesize, 0,
	NULL, NULL },
 { "mmapsize", 0,POPT_ARG_INT,		&staticcfg.db_mmapsize, 0,
	NULL, NULL },
 { "mp_mmapsize", 0,POPT_ARG_INT,	&staticcfg.db_mmapsize, 0,
	NULL, NULL },
 { "mp_size",	0,POPT_ARG_INT,		&staticcfg.db_cachesize, 0,
	NULL, NULL },

 { "nofsync",	0,POPT_ARG_NONE,	&staticcfg.db_no_fsync, 0,
	NULL, NULL },

 /* Per-dbi options */
 { "nommap",	0,POPT_BIT_SET,		&staticdbicfg.dbi_oflags, DB_NOMMAP,
	NULL, NULL },

 { "nodbsync",	0,POPT_ARG_NONE,	&staticdbicfg.dbi_no_dbsync, 0,
	NULL, NULL },
 { "lockdbfd",	0,POPT_ARG_NONE,	&staticdbicfg.dbi_lockdbfd, 0,
	NULL, NULL },

    POPT_TABLEEND
};


static int dbapi_err(rpmdb rdb, const char * msg, int error, int printit)
{
    if (printit && error) {
	if (msg)
	    rpmlog(RPMLOG_ERR, _("%s error(%d) from %s: %s\n"),
		rdb->db_descr, error, msg, db_strerror(error));
	else
	    rpmlog(RPMLOG_ERR, _("%s error(%d): %s\n"),
		rdb->db_descr, error, db_strerror(error));
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

/*
 * Try to acquire db environment open/close serialization lock.
 * Return the open, locked fd on success, -1 on failure.
 */
static int serialize_env(const char *dbhome)
{
    char *lock_path = rstrscat(NULL, dbhome, "/.dbenv.lock", NULL);
    mode_t oldmask = umask(022);
    int fd = open(lock_path, (O_RDWR|O_CREAT), 0644);
    umask(oldmask);

    if (fd >= 0) {
	int rc;
	struct flock info;
	memset(&info, 0, sizeof(info));
	info.l_type = F_WRLCK;
	info.l_whence = SEEK_SET;
	do {
	    rc = fcntl(fd, F_SETLKW, &info);
	} while (rc == -1 && errno == EINTR);
	    
	if (rc == -1) {
	    close(fd);
	    fd = -1;
	}
    }

    free(lock_path);
    return fd;
}

static int db_fini(rpmdb rdb, const char * dbhome)
{
    DB_ENV * dbenv = rdb->db_dbenv;
    int rc;
    int lockfd = -1;
    uint32_t eflags = 0;

    if (dbenv == NULL)
	return 0;

    if (rdb->db_opens > 1) {
	rdb->db_opens--;
	return 0;
    }

    (void) dbenv->get_open_flags(dbenv, &eflags);
    if (!(eflags & DB_PRIVATE))
	lockfd = serialize_env(dbhome);

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

    if (lockfd >= 0)
	close(lockfd);

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


static void dbConfigure(rpmDbiTagVal rpmtag, struct dbConfig_s *cfg, struct dbiConfig_s  *dbicfg)
{
    char *dbOpts;

    dbOpts = rpmExpand("%{_dbi_config_", rpmTagGetName(rpmtag), "}", NULL);
    
    if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	dbOpts = _free(dbOpts);
	dbOpts = rpmExpand("%{_dbi_config}", NULL);
	if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	    dbOpts = _free(dbOpts);
	}
    }

    /* Parse the options for the database element(s). */
    if (dbOpts && *dbOpts && *dbOpts != '%') {
	char *o, *oe;
	char *p, *pe;

	memset(&staticdbicfg, 0, sizeof(staticdbicfg));
/*=========*/
	for (o = dbOpts; o && *o; o = oe) {
	    const struct poptOption *opt;
	    const char * tok;
	    unsigned int argInfo;

	    /* Skip leading white space. */
	    while (*o && risspace(*o))
		o++;

	    /* Find and terminate next key=value pair. Save next start point. */
	    for (oe = o; oe && *oe; oe++) {
		if (risspace(*oe))
		    break;
		if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		    break;
	    }
	    if (oe && *oe)
		*oe++ = '\0';
	    if (*o == '\0')
		continue;

	    /* Separate key from value, save value start (if any). */
	    for (pe = o; pe && *pe && *pe != '='; pe++)
		{};
	    p = (pe ? *pe++ = '\0', pe : NULL);

	    /* Skip over negation at start of token. */
	    for (tok = o; *tok == '!'; tok++)
		{};

	    /* Find key in option table. */
	    for (opt = rdbOptions; opt->longName != NULL; opt++) {
		if (!rstreq(tok, opt->longName))
		    continue;
		break;
	    }
	    if (opt->longName == NULL) {
		rpmlog(RPMLOG_ERR,
			_("unrecognized db option: \"%s\" ignored.\n"), o);
		continue;
	    }

	    /* Toggle the flags for negated tokens, if necessary. */
	    argInfo = opt->argInfo;
	    if (argInfo == POPT_BIT_SET && *o == '!' && ((tok - o) % 2))
		argInfo = POPT_BIT_CLR;

	    /* Save value in template as appropriate. */
	    switch (argInfo & POPT_ARG_MASK) {

	    case POPT_ARG_NONE:
		(void) poptSaveInt((int *)opt->arg, argInfo, 1L);
		break;
	    case POPT_ARG_VAL:
		(void) poptSaveInt((int *)opt->arg, argInfo, (long)opt->val);
	    	break;
	    case POPT_ARG_STRING:
	    {	char ** t = opt->arg;
		if (t) {
/* FIX: opt->arg annotation in popt.h */
		    *t = _free(*t);
		    *t = xstrdup( (p ? p : "") );
		}
	    }	break;

	    case POPT_ARG_INT:
	    case POPT_ARG_LONG:
	      {	long aLong = strtol(p, &pe, 0);
		if (pe) {
		    if (!rstrncasecmp(pe, "Mb", 2))
			aLong *= 1024 * 1024;
		    else if (!rstrncasecmp(pe, "Kb", 2))
			aLong *= 1024;
		    else if (*pe != '\0') {
			rpmlog(RPMLOG_ERR,
				_("%s has invalid numeric value, skipped\n"),
				opt->longName);
			continue;
		    }
		}

		if ((argInfo & POPT_ARG_MASK) == POPT_ARG_LONG) {
		    if (aLong == LONG_MIN || aLong == LONG_MAX) {
			rpmlog(RPMLOG_ERR,
				_("%s has too large or too small long value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) poptSaveLong((long *)opt->arg, argInfo, aLong);
		    break;
		} else {
		    if (aLong > INT_MAX || aLong < INT_MIN) {
			rpmlog(RPMLOG_ERR,
				_("%s has too large or too small integer value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) poptSaveInt((int *)opt->arg, argInfo, aLong);
		}
	      }	break;
	    default:
		break;
	    }
	}
/*=========*/
    }

    dbOpts = _free(dbOpts);
    if (cfg) {
	*cfg = staticcfg;	/* structure assignment */
	/* Throw in some defaults if configuration didn't set any */
	if (!cfg->db_mmapsize)
	    cfg->db_mmapsize = 16 * 1024 * 1024;
	if (!cfg->db_cachesize)
	    cfg->db_cachesize = 8 * 1024 * 1024;
    }
    if (dbicfg) {
	*dbicfg = staticdbicfg;
    }
}

static char * prDbiOpenFlags(int dbflags, int print_dbenv_flags)
{
    ARGV_t flags = NULL;
    const struct poptOption *opt;
    char *buf;

    for (opt = rdbOptions; opt->longName != NULL; opt++) {
        if (opt->argInfo != POPT_BIT_SET)
            continue;
        if (print_dbenv_flags) {
            if (!(opt->arg == &staticcfg.db_eflags))
                continue;
        } else {
            if (!(opt->arg == &staticdbicfg.dbi_oflags))
                continue;
        }
        if ((dbflags & opt->val) != opt->val)
            continue;
        argvAdd(&flags, opt->longName);
        dbflags &= ~opt->val;
    }   
    if (dbflags) {
        char *df = NULL;
        rasprintf(&df, "0x%x", (unsigned)dbflags);
        argvAdd(&flags, df);
        free(df);
    }   
    buf = argvJoin(flags, ":");
    argvFree(flags);
            
    return buf ? buf : xstrdup("(none)");
}

static int db_init(rpmdb rdb, const char * dbhome)
{
    DB_ENV *dbenv = NULL;
    int rc, xx;
    int retry_open = 2;
    int lockfd = -1;
    struct dbConfig_s * cfg = &rdb->cfg;
    /* This is our setup, thou shall not have other setups before us */
    uint32_t eflags = (DB_CREATE|DB_INIT_MPOOL|DB_INIT_CDB);

    if (rdb->db_dbenv != NULL) {
	rdb->db_opens++;
	return 0;
    } else {
	/* On first call, set backend description to something... */
	free(rdb->db_descr);
	rasprintf(&rdb->db_descr, "db%u", DB_VERSION_MAJOR);
    }

    /*
     * Both verify and rebuild are rather special, if for different reasons:
     * On rebuild we dont want to be affected by eg paniced environment, and
     * CDB only slows things down there. Verify is a quirky beast unlike
     * anything else in BDB, and does not like shared env or CDB.
     */
    if (rdb->db_flags & (RPMDB_FLAG_VERIFYONLY|RPMDB_FLAG_REBUILD)) {
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
     * Serialize shared environment open (and clock) via fcntl() lock.
     * Otherwise we can end up calling dbenv->failchk() while another
     * process is joining the environment, leading to transient
     * DB_RUNRECOVER errors. Also prevents races wrt removing the
     * environment (eg chrooted operation). Silently fall back to
     * private environment on failure to allow non-privileged queries
     * to "work", broken as it might be.
     */
    if (!(eflags & DB_PRIVATE)) {
	lockfd = serialize_env(dbhome);
	if (lockfd < 0) {
	    eflags |= DB_PRIVATE;
	    retry_open--;
	    rpmlog(RPMLOG_DEBUG, "serialize failed, using private dbenv\n");
	}
    }

    /*
     * Actually open the environment. Fall back to private environment
     * if we dont have permission to join/create shared environment or
     * system doesn't support it..
     */
    while (retry_open) {
	char *fstr = prDbiOpenFlags(eflags, 1);
	rpmlog(RPMLOG_DEBUG, "opening  db environment %s %s\n", dbhome, fstr);
	free(fstr);

	rc = (dbenv->open)(dbenv, dbhome, eflags, rdb->db_perms);
	if ((rc == EACCES || rc == EROFS) || (rc == EINVAL && errno == rc)) {
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

    if (lockfd >= 0)
	close(lockfd);
    return 0;

errxit:
    if (dbenv) {
	int xx;
	xx = dbenv->close(dbenv, 0);
	xx = dbapi_err(rdb, "dbenv->close", xx, _debug);
    }
    if (lockfd >= 0)
	close(lockfd);
    return rc;
}

static void db3_dbSetFSync(rpmdb rdb, int enable)
{
#ifdef HAVE_FDATASYNC
    db_env_set_func_fsync(enable ? fdatasync : fsync_disable);
#else
    db_env_set_func_fsync(enable ? fsync : fsync_disable);
#endif
}

static int db3_Ctrl(rpmdb rdb, dbCtrlOp ctrl)
{
    return 0;
}

static int dbiSync(dbiIndex dbi, unsigned int flags)
{
    DB * db = dbi->dbi_db;
    int rc = 0;

    if (db != NULL && !dbi->cfg.dbi_no_dbsync) {
	rc = db->sync(db, flags);
	rc = cvtdberr(dbi, "db->sync", rc, _debug);
    }
    return rc;
}

static dbiCursor db3_dbiCursorInit(dbiIndex dbi, unsigned int flags)
{
    dbiCursor dbc = NULL;
    
    if (dbi && dbi->dbi_db) {
	DB * db = dbi->dbi_db;
	DBC * cursor;
	int cflags;
	int rc = 0;
	uint32_t eflags = db_envflags(db);
	
       /* DB_WRITECURSOR requires CDB and writable db */
	if ((flags & DBC_WRITE) &&
	    (eflags & DB_INIT_CDB) && !(dbi->dbi_flags & DBI_RDONLY))
	{
	    cflags = DB_WRITECURSOR;
	} else
	    cflags = 0;

	/*
	 * Check for stale locks which could block writes "forever".
	 * XXX: Should we also do this on reads? Reads are less likely
	 *      to get blocked so it seems excessive...
	 * XXX: On DB_RUNRECOVER, we should abort everything. Now
	 *      we'll just fail to open a cursor again and again and again.
	 */
	if (cflags & DB_WRITECURSOR) {
	    DB_ENV *dbenv = db->get_env(db);
	    rc = dbenv->failchk(dbenv, 0);
	    rc = cvtdberr(dbi, "dbenv->failchk", rc, _debug);
	}

	if (rc == 0) {
	    rc = db->cursor(db, NULL, &cursor, cflags);
	    rc = cvtdberr(dbi, "db->cursor", rc, _debug);
	}

	if (rc == 0) {
	    dbc = xcalloc(1, sizeof(*dbc));
	    dbc->cursor = cursor;
	    dbc->dbi = dbi;
	    dbc->flags = flags;
	}
    }

    return dbc;
}

static dbiCursor db3_dbiCursorFree(dbiIndex dbi, dbiCursor dbc)
{
    if (dbc) {
	/* Automatically sync on write-cursor close */
	if (dbc->flags & DBC_WRITE)
	    dbiSync(dbc->dbi, 0);
	DBC * cursor = dbc->cursor;
	int rc = cursor->c_close(cursor);
	cvtdberr(dbc->dbi, "dbcursor->c_close", rc, _debug);
	free(dbc);
    }
    return NULL;
}

static int dbiCursorPut(dbiCursor dbc, DBT * key, DBT * data, unsigned int flags)
{
    int rc = EINVAL;
    int sane = (key->data != NULL && key->size > 0 &&
		data->data != NULL && data->size > 0);

    if (dbc && sane) {
	DBC * cursor = dbc->cursor;
	rpmdb rdb = dbc->dbi->dbi_rpmdb;
	rpmswEnter(&rdb->db_putops, (ssize_t) 0);

	rc = cursor->c_put(cursor, key, data, DB_KEYLAST);
	rc = cvtdberr(dbc->dbi, "dbcursor->c_put", rc, _debug);

	rpmswExit(&rdb->db_putops, (ssize_t) data->size);
    }
    return rc;
}

static int dbiCursorGet(dbiCursor dbc, DBT * key, DBT * data, unsigned int flags)
{
    int rc = EINVAL;
    int sane = ((flags == DB_NEXT) || (key->data != NULL && key->size > 0));

    if (dbc && sane) {
	DBC * cursor = dbc->cursor;
	rpmdb rdb = dbc->dbi->dbi_rpmdb;
	int _printit;
	rpmswEnter(&rdb->db_getops, 0);

	/* XXX db4 does DB_FIRST on uninitialized cursor */
	rc = cursor->c_get(cursor, key, data, flags);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbc->dbi, "dbcursor->c_get", rc, _printit);

	/* Remember the last key fetched */
	if (rc == 0) {
	    dbc->key = key->data;
	    dbc->keylen = key->size;
	} else {
	    dbc->key = NULL;
	    dbc->keylen = 0;
	}

	rpmswExit(&rdb->db_getops, data->size);
    }
    return rc;
}

static int dbiCursorDel(dbiCursor dbc, DBT * key, DBT * data, unsigned int flags)
{
    int rc = EINVAL;
    int sane = (key->data != NULL && key->size > 0);

    if (dbc && sane) {
	DBC * cursor = dbc->cursor;
	int _printit;
	rpmdb rdb = dbc->dbi->dbi_rpmdb;
	rpmswEnter(&rdb->db_delops, 0);

	/* XXX TODO: insure that cursor is positioned with duplicates */
	rc = cursor->c_get(cursor, key, data, DB_SET);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbc->dbi, "dbcursor->c_get", rc, _printit);

	if (rc == 0) {
	    rc = cursor->c_del(cursor, flags);
	    rc = cvtdberr(dbc->dbi, "dbcursor->c_del", rc, _debug);
	}
	rpmswExit(&rdb->db_delops, data->size);
    }
    return rc;
}

static int dbiByteSwapped(dbiIndex dbi)
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

static int db3_dbiVerify(dbiIndex dbi, unsigned int flags)
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

static int db3_dbiClose(dbiIndex dbi, unsigned int flags)
{
    rpmdb rdb = dbi->dbi_rpmdb;
    const char * dbhome = rpmdbHome(rdb);
    DB * db = dbi->dbi_db;
    int _printit;
    int rc = 0;

    if (db) {
	rc = db->close(db, flags);
	/* XXX ignore not found error messages. */
	_printit = (rc == ENOENT ? 0 : _debug);
	rc = cvtdberr(dbi, "db->close", rc, _printit);
	db = dbi->dbi_db = NULL;

	rpmlog(RPMLOG_DEBUG, "closed   db index       %s/%s\n",
		dbhome, dbi->dbi_file);
    }

    db_fini(rdb, dbhome ? dbhome : "");

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

static int db3_dbiOpen(rpmdb rdb, rpmDbiTagVal rpmtag, dbiIndex * dbip, int flags)
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

    if ((dbi = dbiNew(rdb, rpmtag)) == NULL)
	return 1;

    /*
     * Parse db configuration parameters.
     */
    dbConfigure(rpmtag, rdb->db_dbenv == NULL ? &rdb->cfg : NULL, &dbi->cfg);

    /*
     * Map open mode flags onto configured database/environment flags.
     */
    oflags = dbi->cfg.dbi_oflags;
    if ((rdb->db_mode & O_ACCMODE) == O_RDONLY)
	oflags |= DB_RDONLY;

    rc = db_init(rdb, dbhome);

    retry_open = (rc == 0) ? 2 : 0;

    while (retry_open) {
	rc = db_create(&db, rdb->db_dbenv, 0);
	rc = cvtdberr(dbi, "db_create", rc, _debug);

	/* For verify we only want the handle, not an open db */
	if (verifyonly)
	    break;

	if (rc == 0 && db != NULL) {
	    int _printit;
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
		dbtype = (rpmtag == RPMDBI_PACKAGES) ?  DB_HASH : DB_BTREE;
		retry_open--;
	    } else {
		retry_open = 0;
	    }

	    /* XXX return rc == errno without printing */
	    _printit = (rc > 0 ? 0 : _debug);
	    rc = cvtdberr(dbi, "db->open", rc, _printit);

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

    dbi->dbi_flags = 0;
    if (oflags & DB_CREATE)
	dbi->dbi_flags |= DBI_CREATED;
    if (oflags & DB_RDONLY)
	dbi->dbi_flags |= DBI_RDONLY;

    if (!verifyonly && rc == 0 && dbi->cfg.dbi_lockdbfd && _lockdbfd++ == 0) {
	rc = dbiFlock(dbi, rdb->db_mode);
    }

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	*dbip = dbi;
    } else {
	(void) dbiClose(dbi, 0);
    }

    return rc;
}

union _dbswap {
    unsigned int ui;
    unsigned char uc[4];
};

#define	_DBSWAP(_a) \
\
  { unsigned char _b, *_c = (_a).uc; \
    _b = _c[3]; _c[3] = _c[0]; _c[0] = _b; \
    _b = _c[2]; _c[2] = _c[1]; _c[1] = _b; \
\
  }

/**
 * Convert retrieved data to index set.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @retval setp		(malloc'ed) index set
 * @return		0 on success
 */
static int dbt2set(dbiIndex dbi, DBT * data, dbiIndexSet * setp)
{
    int _dbbyteswapped = dbiByteSwapped(dbi);
    const char * sdbir;
    dbiIndexSet set;
    unsigned int i;

    if (dbi == NULL || data == NULL || setp == NULL)
	return -1;

    if ((sdbir = data->data) == NULL) {
	*setp = NULL;
	return 0;
    }

    set = dbiIndexSetNew(data->size / (2 * sizeof(int32_t)));
    set->count = data->size / (2 * sizeof(int32_t));

    for (i = 0; i < set->count; i++) {
	union _dbswap hdrNum, tagNum;

	memcpy(&hdrNum.ui, sdbir, sizeof(hdrNum.ui));
	sdbir += sizeof(hdrNum.ui);
	memcpy(&tagNum.ui, sdbir, sizeof(tagNum.ui));
	sdbir += sizeof(tagNum.ui);
	if (_dbbyteswapped) {
	    _DBSWAP(hdrNum);
	    _DBSWAP(tagNum);
	}
	set->recs[i].hdrNum = hdrNum.ui;
	set->recs[i].tagNum = tagNum.ui;
    }
    *setp = set;
    return 0;
}

/**
 * Convert index set to database representation.
 * @param dbi		index database handle
 * @param data		retrieved data
 * @param set		index set
 * @return		0 on success
 */
static int set2dbt(dbiIndex dbi, DBT * data, dbiIndexSet set)
{
    int _dbbyteswapped = dbiByteSwapped(dbi);
    char * tdbir;
    unsigned int i;

    if (dbi == NULL || data == NULL || set == NULL)
	return -1;

    data->size = set->count * (2 * sizeof(int32_t));
    if (data->size == 0) {
	data->data = NULL;
	return 0;
    }
    tdbir = data->data = xmalloc(data->size);

    for (i = 0; i < set->count; i++) {
	union _dbswap hdrNum, tagNum;

	memset(&hdrNum, 0, sizeof(hdrNum));
	memset(&tagNum, 0, sizeof(tagNum));
	hdrNum.ui = set->recs[i].hdrNum;
	tagNum.ui = set->recs[i].tagNum;
	if (_dbbyteswapped) {
	    _DBSWAP(hdrNum);
	    _DBSWAP(tagNum);
	}
	memcpy(tdbir, &hdrNum.ui, sizeof(hdrNum.ui));
	tdbir += sizeof(hdrNum.ui);
	memcpy(tdbir, &tagNum.ui, sizeof(tagNum.ui));
	tdbir += sizeof(tagNum.ui);
    }
    return 0;
}

static rpmRC db3_idxdbGet(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
			  dbiIndexSet *set, int searchType)
{
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    if (dbi != NULL && dbc != NULL && set != NULL) {
	int cflags = DB_NEXT;
	int dbrc;
	DBT data, key;
	memset(&data, 0, sizeof(data));
	memset(&key, 0, sizeof(key));

	if (keyp) {
	    if (keylen == 0) {		/* XXX "/" fixup */ 
		keyp = "";
		keylen = 1;
	    }
	    key.data = (void *) keyp; /* discards const */
	    key.size = keylen;
	    cflags = searchType == DBC_PREFIX_SEARCH ? DB_SET_RANGE : DB_SET;
	}

	for (;;) {
	    dbiIndexSet newset = NULL;
	    dbrc = dbiCursorGet(dbc, &key, &data, cflags);
	    if (dbrc != 0)
		break;
	    if (searchType == DBC_PREFIX_SEARCH &&
		    (key.size < keylen || memcmp(key.data, keyp, keylen) != 0))
		break;
	    dbt2set(dbi, &data, &newset);
	    if (*set == NULL) {
		*set = newset;
	    } else {
		dbiIndexSetAppendSet(*set, newset, 0);
		dbiIndexSetFree(newset);
	    }
	    if (searchType != DBC_PREFIX_SEARCH)
		break;
	    key.data = NULL;
	    key.size = 0;
	    cflags = DB_NEXT;
	}

	/* fixup result status for prefix search */
	if (searchType == DBC_PREFIX_SEARCH) {
	    if (dbrc == DB_NOTFOUND && *set != NULL && (*set)->count > 0)
		dbrc = 0;
	    else if (dbrc == 0 && (*set == NULL || (*set)->count == 0))
		dbrc = DB_NOTFOUND;
	}

	if (dbrc == 0) {
	    rc = RPMRC_OK;
	} else if (dbrc == DB_NOTFOUND) {
	    rc = RPMRC_NOTFOUND;
	} else {
	    rpmlog(RPMLOG_ERR,
		   _("error(%d) getting \"%s\" records from %s index: %s\n"),
		   dbrc, keyp ? keyp : "???", dbiName(dbi), db_strerror(dbrc));
	}
    }
    return rc;
}

/* Update secondary index. NULL set deletes the key */
static rpmRC updateIndex(dbiCursor dbc, const char *keyp, unsigned int keylen,
			 dbiIndexSet set)
{
    rpmRC rc = RPMRC_FAIL;

    if (dbc && keyp) {
	dbiIndex dbi = dbc->dbi;
	int dbrc;
	DBT data, key;
	memset(&key, 0, sizeof(data));
	memset(&data, 0, sizeof(data));

	key.data = (void *) keyp; /* discards const */
	key.size = keylen;

	if (set)
	    set2dbt(dbi, &data, set);

	if (dbiIndexSetCount(set) > 0) {
	    dbrc = dbiCursorPut(dbc, &key, &data, DB_KEYLAST);
	    if (dbrc) {
		rpmlog(RPMLOG_ERR,
		       _("error(%d) storing record \"%s\" into %s\n"),
		       dbrc, (char*)key.data, dbiName(dbi));
	    }
	    free(data.data);
	} else {
	    dbrc = dbiCursorDel(dbc, &key, &data, 0);
	    if (dbrc) {
		rpmlog(RPMLOG_ERR,
		       _("error(%d) removing record \"%s\" from %s\n"),
		       dbrc, (char*)key.data, dbiName(dbi));
	    }
	}

	if (dbrc == 0)
	    rc = RPMRC_OK;
    }

    return rc;
}

static rpmRC db3_idxdbPut(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
	       dbiIndexItem rec)
{
    dbiIndexSet set = NULL;
    rpmRC rc;

    if (keyp && keylen == 0) {		/* XXX "/" fixup */
	keyp = "";
	keylen++;
    }
    rc = idxdbGet(dbi, dbc, keyp, keylen, &set, DBC_NORMAL_SEARCH);

    /* Not found means a new key and is not an error. */
    if (rc && rc != RPMRC_NOTFOUND)
	return rc;

    if (set == NULL)
	set = dbiIndexSetNew(1);
    dbiIndexSetAppend(set, rec, 1, 0);

    rc = updateIndex(dbc, keyp, keylen, set);

    dbiIndexSetFree(set);
    return rc;
}

static rpmRC db3_idxdbDel(dbiIndex dbi, dbiCursor dbc, const char *keyp, size_t keylen,
	       dbiIndexItem rec)
{
    dbiIndexSet set = NULL;
    rpmRC rc;

    if (keyp && keylen == 0) {		/* XXX "/" fixup */
	keyp = "";
	keylen++;
    }
    rc = idxdbGet(dbi, dbc, keyp, keylen, &set, DBC_NORMAL_SEARCH);
    if (rc)
	return rc;

    if (dbiIndexSetPrune(set, rec, 1, 1)) {
	/* Nothing was pruned. XXX: Can this actually happen? */
	rc = RPMRC_OK;
    } else {
	/* If there's data left, update data. Otherwise delete the key. */
	if (dbiIndexSetCount(set) > 0) {
	    rc = updateIndex(dbc, keyp, keylen, set);
	} else {
	    rc = updateIndex(dbc, keyp, keylen, NULL);
	}
    };
    dbiIndexSetFree(set);

    return rc;
}

static const void * db3_idxdbKey(dbiIndex dbi, dbiCursor dbc, unsigned int *keylen)
{
    const void *key = NULL;
    if (dbc) {
	key = dbc->key;
	if (key && keylen)
	    *keylen = dbc->keylen;
    }
    return key;
}


/* Update primary Packages index. NULL hdr means remove */
static rpmRC updatePackages(dbiCursor dbc, unsigned int hdrNum, DBT *hdr)
{
    union _dbswap mi_offset;
    int rc = 0;
    DBT key;

    if (dbc == NULL || hdrNum == 0)
	return RPMRC_FAIL;

    memset(&key, 0, sizeof(key));

    mi_offset.ui = hdrNum;
    if (dbiByteSwapped(dbc->dbi) == 1)
	_DBSWAP(mi_offset);
    key.data = (void *) &mi_offset;
    key.size = sizeof(mi_offset.ui);

    if (hdr) {
	rc = dbiCursorPut(dbc, &key, hdr, DB_KEYLAST);
	if (rc) {
	    rpmlog(RPMLOG_ERR,
		   _("error(%d) adding header #%d record\n"), rc, hdrNum);
	}
    } else {
	DBT data;

	memset(&data, 0, sizeof(data));
	rc = dbiCursorGet(dbc, &key, &data, DB_SET);
	if (rc) {
	    rpmlog(RPMLOG_ERR,
		   _("error(%d) removing header #%d record\n"), rc, hdrNum);
	} else
	    rc = dbiCursorDel(dbc, &key, &data, 0);
    }

    return rc == 0 ? RPMRC_OK : RPMRC_FAIL;
}

/* Get current header instance number or try to allocate a new one */
static unsigned int pkgInstance(dbiIndex dbi, int alloc)
{
    unsigned int hdrNum = 0;

    if (dbi != NULL && dbi->dbi_type == DBI_PRIMARY) {
	dbiCursor dbc;
	DBT key, data;
	unsigned int firstkey = 0;
	union _dbswap mi_offset;
	int ret;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	dbc = dbiCursorInit(dbi, alloc ? DBC_WRITE : 0);

	/* Key 0 holds the current largest instance, fetch it */
	key.data = &firstkey;
	key.size = sizeof(firstkey);
	ret = dbiCursorGet(dbc, &key, &data, DB_SET);

	if (ret == 0 && data.data) {
	    memcpy(&mi_offset, data.data, sizeof(mi_offset.ui));
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(mi_offset);
	    hdrNum = mi_offset.ui;
	}

	if (alloc) {
	    /* Rather complicated "increment by one", bswapping as needed */
	    ++hdrNum;
	    mi_offset.ui = hdrNum;
	    if (dbiByteSwapped(dbi) == 1)
		_DBSWAP(mi_offset);
	    if (ret == 0 && data.data) {
		memcpy(data.data, &mi_offset, sizeof(mi_offset.ui));
	    } else {
		data.data = &mi_offset;
		data.size = sizeof(mi_offset.ui);
	    }

	    /* Unless we manage to insert the new instance number, we failed */
	    ret = dbiCursorPut(dbc, &key, &data, DB_KEYLAST);
	    if (ret) {
		hdrNum = 0;
		rpmlog(RPMLOG_ERR,
		    _("error(%d) allocating new package instance\n"), ret);
	    }
	}
	dbiCursorFree(dbi, dbc);
    }
    
    return hdrNum;
}

static rpmRC db3_pkgdbPut(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum,
               unsigned char *hdrBlob, unsigned int hdrLen)
{
    DBT hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.data = hdrBlob;
    hdr.size = hdrLen;
    return updatePackages(dbc, hdrNum, &hdr);
}

static rpmRC db3_pkgdbDel(dbiIndex dbi, dbiCursor dbc,  unsigned int hdrNum)
{
    return updatePackages(dbc, hdrNum, NULL);
}

static rpmRC db3_pkgdbGet(dbiIndex dbi, dbiCursor dbc, unsigned int hdrNum,
	     unsigned char **hdrBlob, unsigned int *hdrLen)
{
    DBT key, data;
    union _dbswap mi_offset;
    int rc;

    if (dbc == NULL)
	return RPMRC_FAIL;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    if (hdrNum) {
	mi_offset.ui = hdrNum;
	if (dbiByteSwapped(dbc->dbi) == 1)
	    _DBSWAP(mi_offset);
	key.data = (void *) &mi_offset;
	key.size = sizeof(mi_offset.ui);
    }

#if !defined(_USE_COPY_LOAD)
    data.flags |= DB_DBT_MALLOC;
#endif
    rc = dbiCursorGet(dbc, &key, &data, hdrNum ? DB_SET : DB_NEXT);
    if (rc == 0) {
	if (hdrBlob)
	    *hdrBlob = data.data;
	if (hdrLen)
	    *hdrLen = data.size;
	return RPMRC_OK;
    } else if (rc == DB_NOTFOUND)
	return RPMRC_NOTFOUND;
    else
	return RPMRC_FAIL;
}

static unsigned int db3_pkgdbKey(dbiIndex dbi, dbiCursor dbc)
{
    union _dbswap mi_offset;

    if (dbc == NULL || dbc->key == NULL)
	return 0;
    memcpy(&mi_offset, dbc->key, sizeof(mi_offset.ui));
    if (dbiByteSwapped(dbc->dbi) == 1)
	_DBSWAP(mi_offset);
    return mi_offset.ui;
}

static rpmRC db3_pkgdbNew(dbiIndex dbi, dbiCursor dbc, unsigned int *hdrNum)
{
    unsigned int num;
    if (dbc == NULL)
	return RPMRC_FAIL;
    num = pkgInstance(dbc->dbi, 1);
    if (!num)
	return RPMRC_FAIL;
    *hdrNum = num;
    return RPMRC_OK;
}

struct rpmdbOps_s db3_dbops = {
    .open   = db3_dbiOpen,
    .close  = db3_dbiClose,
    .verify = db3_dbiVerify,

    .setFSync = db3_dbSetFSync,
    .ctrl = db3_Ctrl,

    .cursorInit = db3_dbiCursorInit,
    .cursorFree = db3_dbiCursorFree,

    .pkgdbGet = db3_pkgdbGet,
    .pkgdbPut = db3_pkgdbPut,
    .pkgdbDel = db3_pkgdbDel,
    .pkgdbNew = db3_pkgdbNew,
    .pkgdbKey = db3_pkgdbKey,

    .idxdbGet = db3_idxdbGet,
    .idxdbPut = db3_idxdbPut,
    .idxdbDel = db3_idxdbDel,
    .idxdbKey = db3_idxdbKey
};

