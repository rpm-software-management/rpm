#include "system.h"

static int _debug = 1;	/* XXX if < 0 debugging, > 0 unusual error returns */

#include <db3/db.h>

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>	/* XXX urlPath proto */

#include "rpmdb.h"
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

static const char * db3basename = "packages.db3";

#if DB_VERSION_MAJOR == 3
#define	__USE_DB3	1

struct _dbiIndex db3dbi;

/* Analogue to struct poptOption */
struct dbOption {
    const char * longName;	/* may be NULL */
    int argInfo;
    void * arg;			/* depends on argInfo */
    int val;			/* 0 means don't return, just update flag */
};

#define	_POPT_SET_BIT	(POPT_ARG_VAL|POPT_ARGFLAG_OR)

struct dbOption rdbOptions[] = {
 /* XXX DB_CXX_NO_EXCEPTIONS */
 { "xa_create",	_POPT_SET_BIT,		&db3dbi.dbi_cflags, DB_XA_CREATE },

 { "create",	_POPT_SET_BIT,		&db3dbi.dbi_oeflags, DB_CREATE },
 { "nommap",	_POPT_SET_BIT,		&db3dbi.dbi_oeflags, DB_NOMMAP },
 { "thread",	_POPT_SET_BIT,		&db3dbi.dbi_oeflags, DB_THREAD },

 { "force",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_FORCE },
 { "cdb",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_INIT_CDB },
 { "lock",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_INIT_LOCK },
 { "log",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_INIT_LOG },
 { "mpool",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_INIT_MPOOL },
 { "txn",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_INIT_TXN },
 { "recover",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_RECOVER },
 { "recover_fatal", _POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_RECOVER_FATAL },
 { "shared",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_SYSTEM_MEM },
 { "txn_nosync", _POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_TXN_NOSYNC },
 { "use_environ_root", _POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_USE_ENVIRON_ROOT },
 { "use_environ", _POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_USE_ENVIRON },
 { "lockdown",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_LOCKDOWN },
 { "private",	_POPT_SET_BIT,		&db3dbi.dbi_eflags, DB_PRIVATE },

 { "txn_sync",	_POPT_SET_BIT,		&db3dbi.dbi_tflags, DB_TXN_SYNC },
 { "txn_nowait",_POPT_SET_BIT,		&db3dbi.dbi_tflags, DB_TXN_NOWAIT },

 { "excl",	_POPT_SET_BIT,		&db3dbi.dbi_oflags, DB_EXCL },
 { "rdonly",	_POPT_SET_BIT,		&db3dbi.dbi_oflags, DB_RDONLY },
 { "truncate",	_POPT_SET_BIT,		&db3dbi.dbi_oflags, DB_TRUNCATE },
 { "fcntl_locking",_POPT_SET_BIT,	&db3dbi.dbi_oflags, DB_FCNTL_LOCKING },

 { "btree",	POPT_ARG_VAL,		&db3dbi.dbi_type, DB_BTREE },
 { "hash", 	POPT_ARG_VAL,		&db3dbi.dbi_type, DB_HASH },
 { "recno",	POPT_ARG_VAL,		&db3dbi.dbi_type, DB_RECNO },
 { "queue",	POPT_ARG_VAL,		&db3dbi.dbi_type, DB_QUEUE },
 { "unknown",	POPT_ARG_VAL,		&db3dbi.dbi_type, DB_UNKNOWN },

 { "root",	POPT_ARG_STRING,	&db3dbi.dbi_root, 0 },
 { "home",	POPT_ARG_STRING,	&db3dbi.dbi_home, 0 },
 { "file",	POPT_ARG_STRING,	&db3dbi.dbi_file, 0 },
 { "subfile",	POPT_ARG_STRING,	&db3dbi.dbi_subfile, 0 },
 { "mode",	POPT_ARG_INT,		&db3dbi.dbi_mode, 0 },
 { "perms",	POPT_ARG_INT,		&db3dbi.dbi_perms, 0 },

 { "teardown",	POPT_ARG_NONE,		&db3dbi.dbi_tear_down, 0 },
 { "usecursors",POPT_ARG_NONE,		&db3dbi.dbi_use_cursors, 0 },
 { "usedbenv",	POPT_ARG_NONE,		&db3dbi.dbi_use_dbenv, 0 },
 { "rmwcursor",	POPT_ARG_NONE,		&db3dbi.dbi_get_rmw_cursor, 0 },
 { "nofsync",	POPT_ARG_NONE,		&db3dbi.dbi_no_fsync, 0 },
 { "nodbsync",	POPT_ARG_NONE,		&db3dbi.dbi_no_dbsync, 0 },
 { "lockdbfd",	POPT_ARG_NONE,		&db3dbi.dbi_lockdbfd, 0 },
 { "temporary",	POPT_ARG_NONE,		&db3dbi.dbi_temporary, 0 },
 { "debug",	POPT_ARG_NONE,		&db3dbi.dbi_debug, 0 },

 { "cachesize",	POPT_ARG_INT,		&db3dbi.dbi_cachesize, 0 },
 { "errpfx",	POPT_ARG_STRING,	&db3dbi.dbi_errpfx, 0 },
 { "region_init", POPT_ARG_VAL,		&db3dbi.dbi_region_init, 1 },
 { "tas_spins",	POPT_ARG_INT,		&db3dbi.dbi_tas_spins, 0 },

 { "chkpoint",	_POPT_SET_BIT,		&db3dbi.dbi_verbose, DB_VERB_CHKPOINT },
 { "deadlock",	_POPT_SET_BIT,		&db3dbi.dbi_verbose, DB_VERB_DEADLOCK },
 { "recovery",	_POPT_SET_BIT,		&db3dbi.dbi_verbose, DB_VERB_RECOVERY },
 { "waitsfor",	_POPT_SET_BIT,		&db3dbi.dbi_verbose, DB_VERB_WAITSFOR },
 { "verbose",	POPT_ARG_VAL,		&db3dbi.dbi_verbose, -1 },

 { "lk_oldest",	POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_OLDEST },
 { "lk_random",	POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_RANDOM },
 { "lk_youngest", POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_YOUNGEST },
/* XXX lk_conflicts matrix */
 { "lk_max",	POPT_ARG_INT,		&db3dbi.dbi_lk_max, 0 },

 { "lg_bsize",	POPT_ARG_INT,		&db3dbi.dbi_lg_bsize, 0 },
 { "lg_max",	POPT_ARG_INT,		&db3dbi.dbi_lg_max, 0 },

/* XXX tx_recover */
 { "tx_max",	POPT_ARG_INT,		&db3dbi.dbi_tx_max, 0 },

 { "lorder",	POPT_ARG_INT,		&db3dbi.dbi_lorder, 0 },

 { "mp_mmapsize", POPT_ARG_INT,		&db3dbi.dbi_mp_mmapsize, 0 },
 { "mp_size",	POPT_ARG_INT,		&db3dbi.dbi_mp_size, 0 },
 { "pagesize",	POPT_ARG_INT,		&db3dbi.dbi_pagesize, 0 },

/* XXX bt_minkey */
/* XXX bt_compare */
/* XXX bt_dup_compare */
/* XXX bt_prefix */
 { "bt_dup",	_POPT_SET_BIT,		&db3dbi.dbi_bt_flags, DB_DUP },
 { "bt_dupsort",_POPT_SET_BIT,		&db3dbi.dbi_bt_flags, DB_DUPSORT },
 { "bt_recnum",	_POPT_SET_BIT,		&db3dbi.dbi_bt_flags, DB_RECNUM },
 { "bt_revsplitoff", _POPT_SET_BIT,	&db3dbi.dbi_bt_flags, DB_REVSPLITOFF },

 { "h_dup",	_POPT_SET_BIT,		&db3dbi.dbi_h_flags, DB_DUP },
 { "h_dupsort",	_POPT_SET_BIT,		&db3dbi.dbi_h_flags, DB_DUPSORT },
 { "h_ffactor",	POPT_ARG_INT,		&db3dbi.dbi_h_ffactor, 0 },
 { "h_nelem",	POPT_ARG_INT,		&db3dbi.dbi_h_nelem, 0 },

 { "re_renumber", _POPT_SET_BIT,	&db3dbi.dbi_re_flags, DB_RENUMBER },
 { "re_snapshot",_POPT_SET_BIT,		&db3dbi.dbi_re_flags, DB_SNAPSHOT },
 { "re_delim",	POPT_ARG_INT,		&db3dbi.dbi_re_delim, 0 },
 { "re_len",	POPT_ARG_INT,		&db3dbi.dbi_re_len, 0 },
 { "re_pad",	POPT_ARG_INT,		&db3dbi.dbi_re_pad, 0 },
 { "re_source",	POPT_ARG_STRING,	&db3dbi.dbi_re_source, 0 },

 { NULL, 0 }
};

static int dbSaveLong(const struct dbOption * opt, long aLong) {
    if (opt->argInfo & POPT_ARGFLAG_NOT)
	aLong = ~aLong;
    switch (opt->argInfo & POPT_ARGFLAG_LOGICALOPS) {
    case 0:
	*((long *) opt->arg) = aLong;
	break;
    case POPT_ARGFLAG_OR:
	*((long *) opt->arg) |= aLong;
	break;
    case POPT_ARGFLAG_AND:
	*((long *) opt->arg) &= aLong;
	break;
    case POPT_ARGFLAG_XOR:
	*((long *) opt->arg) ^= aLong;
	break;
    default:
	return POPT_ERROR_BADOPERATION;
	break;
    }
    return 0;
}

static int dbSaveInt(const struct dbOption * opt, long aLong) {
    if (opt->argInfo & POPT_ARGFLAG_NOT)
	aLong = ~aLong;
    switch (opt->argInfo & POPT_ARGFLAG_LOGICALOPS) {
    case 0:
	*((int *) opt->arg) = aLong;
	break;
    case POPT_ARGFLAG_OR:
	*((int *) opt->arg) |= aLong;
	break;
    case POPT_ARGFLAG_AND:
	*((int *) opt->arg) &= aLong;
	break;
    case POPT_ARGFLAG_XOR:
	*((int *) opt->arg) ^= aLong;
	break;
    default:
	return POPT_ERROR_BADOPERATION;
	break;
    }
    return 0;
}

void db3Free(dbiIndex dbi) {
    if (dbi) {
	if (dbi->dbi_root)	xfree(dbi->dbi_root);
	if (dbi->dbi_home)	xfree(dbi->dbi_home);
	if (dbi->dbi_file)	xfree(dbi->dbi_file);
	if (dbi->dbi_subfile)	xfree(dbi->dbi_subfile);
	if (dbi->dbi_errpfx)	xfree(dbi->dbi_errpfx);
	if (dbi->dbi_re_source)	xfree(dbi->dbi_re_source);
	if (dbi->dbi_dbenv)	free(dbi->dbi_dbenv);
	if (dbi->dbi_dbinfo)	free(dbi->dbi_dbinfo);
	xfree(dbi);
    }
}

static const char *db3_config_default =
    "db3:hash:mpool:cdb:usecursors:verbose:mp_mmapsize=8Mb:mp_size=512Kb:pagesize=512:perms=0644";

dbiIndex db3New(rpmdb rpmdb, int rpmtag)
{
    dbiIndex dbi = xcalloc(1, sizeof(*dbi));
    char dbiTagMacro[128];
    char * dbOpts;

    sprintf(dbiTagMacro, "%%{_dbi_config_%s}", tagName(rpmtag));
    dbOpts = rpmExpand(dbiTagMacro, NULL);
    if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	if (dbOpts) {
	    free(dbOpts);
	    dbOpts = NULL;
	}
	dbOpts = rpmExpand("%{_dbi_config}", NULL);
	if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	    dbOpts = rpmExpand(db3_config_default, NULL);
	}
    }

    if (dbOpts && *dbOpts && *dbOpts != '%') {
	char *o, *oe;
	char *p, *pe;
	for (o = dbOpts; o && *o; o = oe) {
	    struct dbOption *opt;

	    while (*o && isspace(*o))
		o++;
	    for (oe = o; oe && *oe; oe++) {
		if (isspace(*oe))
		    break;
		if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		    break;
	    }
	    if (oe && *oe)
		*oe++ = '\0';
	    if (*o == '\0')
		continue;
	    for (pe = o; pe && *pe && *pe != '='; pe++)
		;
	    p = (pe ? *pe++ = '\0', pe : NULL);

	    for (opt = rdbOptions; opt->longName != NULL; opt++) {
		if (strcmp(o, opt->longName))
		    continue;
		break;
	    }
	    if (opt->longName == NULL) {
		fprintf(stderr, _("dbiSetConfig: unrecognized db option: \"%s\" ignored\n"), o);
		continue;
	    }

	    switch (opt->argInfo & POPT_ARG_MASK) {
	    long aLong;

	    case POPT_ARG_NONE:
		(void) dbSaveInt(opt, 1L);
		break;
	    case POPT_ARG_VAL:
		(void) dbSaveInt(opt, (long)opt->val);
	    	break;
	    case POPT_ARG_STRING:
	    {	const char ** t = opt->arg;
		if (*t) xfree(*t);
		*t = xstrdup( (p ? p : "") );
	    }	break;

	    case POPT_ARG_INT:
	    case POPT_ARG_LONG:
		aLong = strtol(p, &pe, 0);
		if (pe) {
		    if (!strncasecmp(pe, "Mb", 2))
			aLong *= 1024 * 1024;
		    else if (!strncasecmp(pe, "Kb", 2))
			aLong *= 1024;
		    else if (*pe != '\0') {
			fprintf(stderr,_("%s has invalid numeric value, skipped\n"),
				opt->longName);
			continue;
		    }
		}

		if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_LONG) {
		    if (aLong == LONG_MIN || aLong == LONG_MAX) {
			fprintf(stderr, _("%s has too large or too small long value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) dbSaveLong(opt, aLong);
		    break;
		} else {
		    if (aLong > INT_MAX || aLong < INT_MIN) {
			fprintf(stderr, _("%s has too large or too small integer value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) dbSaveInt(opt, aLong);
		}
		break;
	    default:
		break;
	    }
	}
    }

    free(dbOpts);

    *dbi = db3dbi;	/* structure assignment */
    memset(&db3dbi, 0, sizeof(db3dbi));

    if (!(dbi->dbi_perms & 0600))
	dbi->dbi_perms = 0644;
    dbi->dbi_mode = rpmdb->db_mode;
    dbi->dbi_rpmdb = rpmdb;
    dbi->dbi_rpmtag = rpmtag;
    
    switch (rpmtag) {
    case RPMDBI_PACKAGES:
    case RPMDBI_DEPENDS:
	dbi->dbi_jlen = 1 * sizeof(int_32);
	break;
    default:
	dbi->dbi_jlen = 2 * sizeof(int_32);
	break;
    }
    return dbi;
}

static const char *const prDbiOpenFlags(int dbflags, int print_dbenv_flags)
{
    static char buf[256];
    struct dbOption *opt;
    char * oe;

    oe = buf;
    *oe = '\0';
    for (opt = rdbOptions; opt->longName != NULL; opt++) {
	if (opt->argInfo != _POPT_SET_BIT)
	    continue;
	if (print_dbenv_flags) {
	    if (!(opt->arg == &db3dbi.dbi_oeflags ||
		  opt->arg == &db3dbi.dbi_eflags))
		continue;
	} else {
	    if (!(opt->arg == &db3dbi.dbi_oeflags ||
		  opt->arg == &db3dbi.dbi_oflags))
		continue;
	}
	if ((dbflags & opt->val) != opt->val)
	    continue;
	if (oe != buf)
	    *oe++ = ':';
	oe = stpcpy(oe, opt->longName);
	dbflags &= ~opt->val;
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
	fprintf(stderr, "*** db%d %s rc %d %s\n", dbi->dbi_api, msg,
		rc, db_strerror(error));
    }

    return rc;
}

static int db_fini(dbiIndex dbi, const char * dbhome, const char * dbfile,
		const char * dbsubfile)
{
    rpmdb rpmdb = dbi->dbi_rpmdb;
    DB_ENV * dbenv = dbi->dbi_dbenv;

#if defined(__USE_DB3)
    char **dbconfig = NULL;
    int rc;

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
	xx = dbenv->remove(dbenv, dbhome, dbconfig, 0);
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

static int db3_fsync_disable(int fd) {
    return 0;
}

static int db_init(dbiIndex dbi, const char *dbhome, const char *dbfile,
		const char * dbsubfile, DB_ENV **dbenvp)
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
	xx = dbenv->set_func_fsync(dbenv, db3_fsync_disable);
	xx = cvtdberr(dbi, "dbenv->set_func_fsync", xx, _debug);
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
    rc = dbenv->open(dbenv, dbhome, NULL, eflags, dbi->dbi_perms);
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

static int db3sync(dbiIndex dbi, unsigned int flags)
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

static int db3c_put(dbiIndex dbi, DBC * dbcursor,
	DBT * key, DBT * data, u_int32_t flags)
{
    int rc;

    rc = dbcursor->c_put(dbcursor, key, data, flags);

    rc = cvtdberr(dbi, "dbcursor->c_put", rc, _debug);
    return rc;
}

static inline int db3c_close(dbiIndex dbi, DBC * dbcursor)
{
    int rc;

    rc = dbcursor->c_close(dbcursor);
    rc = cvtdberr(dbi, "dbcursor->c_close", rc, _debug);
    return rc;
}

static inline int db3c_open(dbiIndex dbi, DBC ** dbcp)
{
    DB * db = dbi->dbi_db;
    DB_TXN * txnid = NULL;
    int flags;
    int rc;

#if defined(__USE_DB3)
    if ((dbi->dbi_eflags & DB_INIT_CDB) && !(dbi->dbi_oflags & DB_RDONLY))
	flags = DB_WRITECURSOR;
    else
	flags = 0;
    rc = db->cursor(db, txnid, dbcp, flags);
#else	/* __USE_DB3 */
    rc = db->cursor(db, txnid, dbcp);
#endif	/* __USE_DB3 */
    rc = cvtdberr(dbi, "db3copen", rc, _debug);

    return rc;
}

static int db3cclose(dbiIndex dbi, DBC * dbcursor, unsigned int flags)
{
    int rc = 0;

    if (dbcursor == NULL)
	dbcursor = dbi->dbi_rmw;
    if (dbcursor) {
	rc = db3c_close(dbi, dbcursor);
	if (dbcursor == dbi->dbi_rmw)
	    dbi->dbi_rmw = NULL;
    }
    return rc;
}

static int db3copen(dbiIndex dbi, DBC ** dbcp, unsigned int flags)
{
    DBC * dbcursor;
    int rc = 0;

    if ((dbcursor = dbi->dbi_rmw) == NULL) {
	if ((rc = db3c_open(dbi, &dbcursor)) == 0)
	    dbi->dbi_rmw = dbcursor;
    }

    if (dbcp)
	*dbcp = dbi->dbi_rmw;

    return rc;
}

static int db3cput(dbiIndex dbi, const void * keyp, size_t keylen,
		const void * datap, size_t datalen, unsigned int flags)
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

    if (!dbi->dbi_use_cursors) {
	rc = db->put(db, txnid, &key, &data, 0);
	rc = cvtdberr(dbi, "db->put", rc, _debug);
    } else {
	DBC * dbcursor;

	if ((rc = db3copen(dbi, &dbcursor, 0)) != 0)
	    return rc;

	rc = db3c_put(dbi, dbcursor, &key, &data, DB_KEYLAST);

	(void) db3cclose(dbi, dbcursor, 0);
    }

    return rc;
}

static int db3cdel(dbiIndex dbi, const void * keyp, size_t keylen, unsigned int flags)
{
    DB * db = dbi->dbi_db;
    DB_TXN * txnid = NULL;
    DBT key, data;
    int rc;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = (void *)keyp;
    key.size = keylen;

    if (!dbi->dbi_use_cursors) {
	rc = db->del(db, txnid, &key, 0);
	rc = cvtdberr(dbi, "db->del", rc, _debug);
    } else {
	DBC * dbcursor;

	if ((rc = db3copen(dbi, &dbcursor, 0)) != 0)
	    return rc;

	rc = db3c_get(dbi, dbcursor, &key, &data, DB_SET);

	if (rc == 0) {
	    /* XXX TODO: loop over duplicates */
	    rc = db3c_del(dbi, dbcursor, 0);
	}

	(void) db3c_close(dbi, dbcursor);

    }

    return rc;
}

static int db3cget(dbiIndex dbi, void ** keyp, size_t * keylen,
		void ** datap, size_t * datalen, unsigned int flags)
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

    if (!dbi->dbi_use_cursors) {
	int _printit;
	rc = db->get(db, txnid, &key, &data, 0);
	/* XXX DB_NOTFOUND can be returned */
	_printit = (rc == DB_NOTFOUND ? 0 : _debug);
	rc = cvtdberr(dbi, "db->get", rc, _printit);
    } else {
	DBC * dbcursor;

	if ((rc = db3copen(dbi, &dbcursor, 0)) != 0)
	    return rc;

	/* XXX db3 does DB_FIRST on uninitialized cursor */
	rc = db3c_get(dbi, dbcursor, &key, &data,
		key.data == NULL ? DB_NEXT : DB_SET);

	if (rc > 0)	/* DB_NOTFOUND */
	    (void) db3cclose(dbi, dbcursor, 0);
    }

    if (rc == 0) {
	if (keyp)	*keyp = key.data;
	if (keylen)	*keylen = key.size;
	if (datap)	*datap = data.data;
	if (datalen)	*datalen = data.size;
    }

    return rc;
}

static int db3byteswapped(dbiIndex dbi)
{
    DB * db = dbi->dbi_db;
    int rc = 0;

#if defined(__USE_DB3)
    rc = db->get_byteswapped(db);
#endif	/* __USE_DB3 */

    return rc;
}

static int db3close(dbiIndex dbi, unsigned int flags)
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
	dbfile = (dbi->dbi_file ? dbi->dbi_file : db3basename);
	dbsubfile = (dbi->dbi_subfile ? dbi->dbi_subfile : tagName(dbi->dbi_rpmtag));
#else
	dbfile = (dbi->dbi_file ? dbi->dbi_file : tagName(dbi->dbi_rpmtag));
	dbsubfile = NULL;
#endif
    }

#if defined(__USE_DB2) || defined(__USE_DB3)

    if (dbi->dbi_rmw)
	db3cclose(dbi, NULL, 0);

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
	xfree(urlfn);

    db3Free(dbi);

    return rc;
}

static int db3open(rpmdb rpmdb, int rpmtag, dbiIndex * dbip)
{
    const char * urlfn = NULL;
    const char * dbhome;
    const char * dbfile;
    const char * dbsubfile;
    const char * dbpath;
    extern struct _dbiVec db3vec;
    dbiIndex dbi = NULL;
    int rc = 0;
    int xx;

#if defined(__USE_DB2) || defined(__USE_DB3)
    DB * db = NULL;
    DB_ENV * dbenv = NULL;
    DB_TXN * txnid = NULL;
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
	dbfile = (dbi->dbi_file ? dbi->dbi_file : db3basename);
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
		char * t;
		int nb;

		nb = strlen(dbhome);
		if (dbfile)	nb += 1 + strlen(dbfile);
		dbfullpath = t = alloca(nb);

		t = stpcpy(t, dbhome);
		if (dbfile)
		    t = stpcpy( stpcpy( t, "/"), dbfile);
		dbpath = (!dbi->dbi_use_dbenv && !dbi->dbi_temporary)
			? dbfullpath : dbfile;
	    }

	    rc = db->open(db, dbpath, dbsubfile,
		    dbi->dbi_type, oflags, dbi->dbi_perms);
	    /* XXX return rc == errno without printing */
	    _printit = (rc > 0 ? 0 : _debug);
	    xx = cvtdberr(dbi, "db->open", rc, _printit);

	    if (rc == 0 && dbi->dbi_get_rmw_cursor) {
		DBC * dbcursor = NULL;
		int xx;
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
#else	/* __USE_DB3 */
      {	DB_INFO * dbinfo = xcalloc(1, sizeof(*dbinfo));
	dbinfo->db_cachesize = dbi->dbi_cachesize;
	dbinfo->db_lorder = dbi->dbi_lorder;
	dbinfo->db_pagesize = dbi->dbi_pagesize;
	dbinfo->db_malloc = rpmdb->db_malloc;
	if (oflags & DB_CREATE) {
	    switch(dbi->dbi_type) {
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
	rc = db_open(dbfile, dbi->dbi_type, oflags,
			dbi->dbi_perms, dbenv, dbinfo, &db);
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
		dbi->dbi_type, dbopeninfo);
    /* XXX return rc == errno without printing */
    if (dbi->dbi_db == NULL) rc = errno;
#endif	/* __USE_DB2 || __USE_DB3 */

    if (rc == 0 && dbi->dbi_db != NULL && dbip != NULL) {
	dbi->dbi_vec = &db3vec;
	*dbip = dbi;
    } else
	db3close(dbi, 0);

    if (urlfn)
	xfree(urlfn);

    return rc;
}

struct _dbiVec db3vec = {
    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
    db3open, db3close, db3sync, db3copen, db3cclose, db3cdel, db3cget, db3cput,
    db3byteswapped
};

#endif	/* DB_VERSION_MAJOR == 3 */
