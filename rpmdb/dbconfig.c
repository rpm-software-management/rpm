/** \ingroup rpmdb
 * \file rpmdb/dbconfig.c
 */

#include "system.h"

#include <db3/db.h>

#include <rpmlib.h>
#include <rpmmacro.h>

#include "rpmdb.h"
#include "debug.h"

/*@access rpmdb@*/
/*@access dbiIndex@*/
/*@access dbiIndexSet@*/

#if DB_VERSION_MAJOR == 3
#define	__USE_DB3	1

/*@-exportlocal -exportheadervar@*/
struct _dbiIndex db3dbi;
/*@=exportlocal =exportheadervar@*/

/** \ingroup db3
 *  Analogue to struct poptOption
 */
struct dbOption {
/*@observer@*/ /*@null@*/const char * longName;	/* may be NULL */
    const char shortName;	/* may be '\0' */
    int argInfo;
/*@null@*/ void * arg;		/* depends on argInfo */
    int val;			/* 0 means don't return, just update flag */
/*@observer@*/ /*@null@*/ const char * descrip; /*!< description for autohelp -- may be NULL */
/*@observer@*/ /*@null@*/ const char * argDescrip; /*!< argument description for autohelp */
};

#define	_POPT_SET_BIT	(POPT_ARG_VAL|POPT_ARGFLAG_OR)
#define	_POPT_UNSET_BIT	(POPT_ARG_VAL|POPT_ARGFLAG_NAND)

/*@-immediatetrans -exportlocal -exportheadervar@*/
/** \ingroup db3
 */
struct dbOption rdbOptions[] = {
 /* XXX DB_CXX_NO_EXCEPTIONS */
 { "client",	0,_POPT_SET_BIT,	&db3dbi.dbi_ecflags, DB_CLIENT,
	NULL, NULL },

 { "xa_create",	0,_POPT_SET_BIT,	&db3dbi.dbi_cflags, DB_XA_CREATE,
	NULL, NULL },

 { "create",	0,_POPT_SET_BIT,	&db3dbi.dbi_oeflags, DB_CREATE,
	NULL, NULL },
 { "thread",	0,_POPT_SET_BIT,	&db3dbi.dbi_oeflags, DB_THREAD,
	NULL, NULL },

 { "force",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_FORCE,
	NULL, NULL },
 { "cdb",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_INIT_CDB,
	NULL, NULL },
 { "lock",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_INIT_LOCK,
	NULL, NULL },
 { "log",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_INIT_LOG,
	NULL, NULL },
 { "mpool",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_INIT_MPOOL,
	NULL, NULL },
 { "txn",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_INIT_TXN,
	NULL, NULL },
 { "joinenv",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_JOINENV,
	NULL, NULL },
 { "recover",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_RECOVER,
	NULL, NULL },
 { "recover_fatal", 0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_RECOVER_FATAL,
	NULL, NULL },
 { "shared",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_SYSTEM_MEM,
	NULL, NULL },
 { "txn_nosync", 0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_TXN_NOSYNC,
	NULL, NULL },
 { "use_environ_root", 0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_USE_ENVIRON_ROOT,
	NULL, NULL },
 { "use_environ", 0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_USE_ENVIRON,
	NULL, NULL },
 { "lockdown",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_LOCKDOWN,
	NULL, NULL },
 { "private",	0,_POPT_SET_BIT,	&db3dbi.dbi_eflags, DB_PRIVATE,
	NULL, NULL },

 { "txn_sync",	0,_POPT_SET_BIT,	&db3dbi.dbi_tflags, DB_TXN_SYNC,
	NULL, NULL },
 { "txn_nowait",0,_POPT_SET_BIT,	&db3dbi.dbi_tflags, DB_TXN_NOWAIT,
	NULL, NULL },

 { "excl",	0,_POPT_SET_BIT,	&db3dbi.dbi_oflags, DB_EXCL,
	NULL, NULL },
 { "nommap",	0,_POPT_SET_BIT,	&db3dbi.dbi_oflags, DB_NOMMAP,
	NULL, NULL },
 { "rdonly",	0,_POPT_SET_BIT,	&db3dbi.dbi_oflags, DB_RDONLY,
	NULL, NULL },
 { "truncate",	0,_POPT_SET_BIT,	&db3dbi.dbi_oflags, DB_TRUNCATE,
	NULL, NULL },
 { "fcntl_locking",0,_POPT_SET_BIT,	&db3dbi.dbi_oflags, DB_FCNTL_LOCKING,
	NULL, NULL },

 { "btree",	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_BTREE,
	NULL, NULL },
 { "hash", 	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_HASH,
	NULL, NULL },
 { "recno",	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_RECNO,
	NULL, NULL },
 { "queue",	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_QUEUE,
	NULL, NULL },
 { "unknown",	0,POPT_ARG_VAL,		&db3dbi.dbi_type, DB_UNKNOWN,
	NULL, NULL },

 { "root",	0,POPT_ARG_STRING,	&db3dbi.dbi_root, 0,
	NULL, NULL },
 { "home",	0,POPT_ARG_STRING,	&db3dbi.dbi_home, 0,
	NULL, NULL },
 { "file",	0,POPT_ARG_STRING,	&db3dbi.dbi_file, 0,
	NULL, NULL },
 { "subfile",	0,POPT_ARG_STRING,	&db3dbi.dbi_subfile, 0,
	NULL, NULL },
 { "mode",	0,POPT_ARG_INT,		&db3dbi.dbi_mode, 0,
	NULL, NULL },
 { "perms",	0,POPT_ARG_INT,		&db3dbi.dbi_perms, 0,
	NULL, NULL },
 { "shmkey",	0,POPT_ARG_LONG,	&db3dbi.dbi_shmkey, 0,
	NULL, NULL },

 { "host",	0,POPT_ARG_STRING,	&db3dbi.dbi_host, 0,
	NULL, NULL },
 { "server",	0,POPT_ARG_STRING,	&db3dbi.dbi_host, 0,
	NULL, NULL },
 { "cl_timeout", 0,POPT_ARG_LONG,	&db3dbi.dbi_cl_timeout, 0,
	NULL, NULL },
 { "sv_timeout", 0,POPT_ARG_LONG,	&db3dbi.dbi_sv_timeout, 0,
	NULL, NULL },

 { "verify",	0,POPT_ARG_NONE,	&db3dbi.dbi_verify_on_close, 0,
	NULL, NULL },
 { "teardown",	0,POPT_ARG_NONE,	&db3dbi.dbi_tear_down, 0,
	NULL, NULL },
 { "usecursors",0,POPT_ARG_NONE,	&db3dbi.dbi_use_cursors, 0,
	NULL, NULL },
 { "usedbenv",	0,POPT_ARG_NONE,	&db3dbi.dbi_use_dbenv, 0,
	NULL, NULL },
 { "rmwcursor",	0,POPT_ARG_NONE,	&db3dbi.dbi_get_rmw_cursor, 0,
	NULL, NULL },
 { "nofsync",	0,POPT_ARG_NONE,	&db3dbi.dbi_no_fsync, 0,
	NULL, NULL },
 { "nodbsync",	0,POPT_ARG_NONE,	&db3dbi.dbi_no_dbsync, 0,
	NULL, NULL },
 { "lockdbfd",	0,POPT_ARG_NONE,	&db3dbi.dbi_lockdbfd, 0,
	NULL, NULL },
 { "temporary",	0,POPT_ARG_NONE,	&db3dbi.dbi_temporary, 0,
	NULL, NULL },
 { "debug",	0,POPT_ARG_NONE,	&db3dbi.dbi_debug, 0,
	NULL, NULL },

 { "cachesize",	0,POPT_ARG_INT,		&db3dbi.dbi_cachesize, 0,
	NULL, NULL },
 { "errpfx",	0,POPT_ARG_STRING,	&db3dbi.dbi_errpfx, 0,
	NULL, NULL },
 { "region_init", 0,POPT_ARG_VAL,	&db3dbi.dbi_region_init, 1,
	NULL, NULL },
 { "tas_spins",	0,POPT_ARG_INT,		&db3dbi.dbi_tas_spins, 0,
	NULL, NULL },

 { "chkpoint",	0,_POPT_SET_BIT,	&db3dbi.dbi_verbose, DB_VERB_CHKPOINT,
	NULL, NULL },
 { "deadlock",	0,_POPT_SET_BIT,	&db3dbi.dbi_verbose, DB_VERB_DEADLOCK,
	NULL, NULL },
 { "recovery",	0,_POPT_SET_BIT,	&db3dbi.dbi_verbose, DB_VERB_RECOVERY,
	NULL, NULL },
 { "waitsfor",	0,_POPT_SET_BIT,	&db3dbi.dbi_verbose, DB_VERB_WAITSFOR,
	NULL, NULL },
 { "verbose",	0,POPT_ARG_VAL,		&db3dbi.dbi_verbose, -1,
	NULL, NULL },

 { "lk_oldest",	0,POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_OLDEST,
	NULL, NULL },
 { "lk_random",	0,POPT_ARG_VAL,		&db3dbi.dbi_lk_detect, DB_LOCK_RANDOM,
	NULL, NULL },
 { "lk_youngest",0, POPT_ARG_VAL,	&db3dbi.dbi_lk_detect, DB_LOCK_YOUNGEST,
	NULL, NULL },
/* XXX lk_conflicts matrix */
 { "lk_max",	0,POPT_ARG_INT,		&db3dbi.dbi_lk_max, 0,
	NULL, NULL },

 { "lg_bsize",	0,POPT_ARG_INT,		&db3dbi.dbi_lg_bsize, 0,
	NULL, NULL },
 { "lg_max",	0,POPT_ARG_INT,		&db3dbi.dbi_lg_max, 0,
	NULL, NULL },

/* XXX tx_recover */
 { "tx_max",	0,POPT_ARG_INT,		&db3dbi.dbi_tx_max, 0,
	NULL, NULL },

 { "lorder",	0,POPT_ARG_INT,		&db3dbi.dbi_lorder, 0,
	NULL, NULL },

 { "mp_mmapsize", 0,POPT_ARG_INT,	&db3dbi.dbi_mp_mmapsize, 0,
	NULL, NULL },
 { "mp_size",	0,POPT_ARG_INT,		&db3dbi.dbi_mp_size, 0,
	NULL, NULL },
 { "pagesize",	0,POPT_ARG_INT,		&db3dbi.dbi_pagesize, 0,
	NULL, NULL },

/* XXX bt_minkey */
/* XXX bt_compare */
/* XXX bt_dup_compare */
/* XXX bt_prefix */
 { "bt_dup",	0,_POPT_SET_BIT,	&db3dbi.dbi_bt_flags, DB_DUP,
	NULL, NULL },
 { "bt_dupsort",0,_POPT_SET_BIT,	&db3dbi.dbi_bt_flags, DB_DUPSORT,
	NULL, NULL },
 { "bt_recnum",	0,_POPT_SET_BIT,	&db3dbi.dbi_bt_flags, DB_RECNUM,
	NULL, NULL },
 { "bt_revsplitoff", 0,_POPT_SET_BIT,	&db3dbi.dbi_bt_flags, DB_REVSPLITOFF,
	NULL, NULL },

 { "h_dup",	0,_POPT_SET_BIT,	&db3dbi.dbi_h_flags, DB_DUP,
	NULL, NULL },
 { "h_dupsort",	0,_POPT_SET_BIT,	&db3dbi.dbi_h_flags, DB_DUPSORT,
	NULL, NULL },
 { "h_ffactor",	0,POPT_ARG_INT,		&db3dbi.dbi_h_ffactor, 0,
	NULL, NULL },
 { "h_nelem",	0,POPT_ARG_INT,		&db3dbi.dbi_h_nelem, 0,
	NULL, NULL },

 { "re_renumber", 0,_POPT_SET_BIT,	&db3dbi.dbi_re_flags, DB_RENUMBER,
	NULL, NULL },
 { "re_snapshot",0,_POPT_SET_BIT,	&db3dbi.dbi_re_flags, DB_SNAPSHOT,
	NULL, NULL },
 { "re_delim",	0,POPT_ARG_INT,		&db3dbi.dbi_re_delim, 0,
	NULL, NULL },
 { "re_len",	0,POPT_ARG_INT,		&db3dbi.dbi_re_len, 0,
	NULL, NULL },
 { "re_pad",	0,POPT_ARG_INT,		&db3dbi.dbi_re_pad, 0,
	NULL, NULL },
 { "re_source",	0,POPT_ARG_STRING,	&db3dbi.dbi_re_source, 0,
	NULL, NULL },

 { "q_extentsize", 0,POPT_ARG_INT,	&db3dbi.dbi_q_extentsize, 0,
	NULL, NULL },

 { NULL, 0,0, NULL, 0, NULL, NULL }
};
/*@=immediatetrans =exportlocal =exportheadervar@*/

static int dbSaveLong(const struct dbOption * opt, int argInfo, long aLong) {
    if (argInfo & POPT_ARGFLAG_NOT)
	aLong = ~aLong;
    if (opt->arg != NULL)
    switch (argInfo & POPT_ARGFLAG_LOGICALOPS) {
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
	/*@notreached@*/ break;
    }
    return 0;
}

static int dbSaveInt(const struct dbOption * opt, int argInfo, long aLong) {
    if (argInfo & POPT_ARGFLAG_NOT)
	aLong = ~aLong;
    if (opt->arg != NULL)
    switch (argInfo & POPT_ARGFLAG_LOGICALOPS) {
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
	/*@notreached@*/ break;
    }
    return 0;
}

dbiIndex db3Free(dbiIndex dbi) {
    if (dbi) {
	dbi->dbi_root = _free(dbi->dbi_root);
	dbi->dbi_home = _free(dbi->dbi_home);
	dbi->dbi_file = _free(dbi->dbi_file);
	dbi->dbi_subfile = _free(dbi->dbi_subfile);
	dbi->dbi_host = _free(dbi->dbi_host);
	dbi->dbi_errpfx = _free(dbi->dbi_errpfx);
	dbi->dbi_re_source = _free(dbi->dbi_re_source);
	dbi->dbi_dbenv = _free(dbi->dbi_dbenv);
	dbi->dbi_dbinfo = _free(dbi->dbi_dbinfo);
	dbi->dbi_stats = _free(dbi->dbi_stats);
	dbi = _free(dbi);
    }
    return dbi;
}

/** @todo Set a reasonable "last gasp" default db config. */
/*@observer@*/ static const char *db3_config_default =
    "db3:hash:mpool:cdb:usecursors:verbose:mp_mmapsize=8Mb:mp_size=512Kb:pagesize=512:perms=0644";

dbiIndex db3New(rpmdb rpmdb, int rpmtag)
{
    dbiIndex dbi = xcalloc(1, sizeof(*dbi));
    char dbiTagMacro[128];
    char * dbOpts;

    sprintf(dbiTagMacro, "%%{_dbi_config_%s}", tagName(rpmtag));
    /*@-nullpass@*/
    dbOpts = rpmExpand(dbiTagMacro, NULL);
    /*@=nullpass@*/
    if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	dbOpts = _free(dbOpts);
	/*@-nullpass@*/
	dbOpts = rpmExpand("%{_dbi_config}", NULL);
	/*@=nullpass@*/
	if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	    /*@-nullpass@*/
	    dbOpts = rpmExpand(db3_config_default, NULL);
	    /*@=nullpass@*/
	}
    }

    /* Parse the options for the database element(s). */
    if (dbOpts && *dbOpts && *dbOpts != '%') {
	char *o, *oe;
	char *p, *pe;
	for (o = dbOpts; o && *o; o = oe) {
	    struct dbOption *opt;
	    const char * tok;
	    int argInfo;

	    /* Skip leading white space. */
	    while (*o && xisspace(*o))
		o++;

	    /* Find and terminate next key=value pair. Save next start point. */
	    for (oe = o; oe && *oe; oe++) {
		if (xisspace(*oe))
		    /*@innerbreak@*/ break;
		if (oe[0] == ':' && !(oe[1] == '/' && oe[2] == '/'))
		    /*@innerbreak@*/ break;
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
		if (strcmp(tok, opt->longName))
		    continue;
		/*@innerbreak@*/ break;
	    }
	    if (opt->longName == NULL) {
		rpmError(RPMERR_DBCONFIG,
			_("unrecognized db option: \"%s\" ignored.\n"), o);
		continue;
	    }

	    /* Toggle the flags for negated tokens, if necessary. */
	    argInfo = opt->argInfo;
	    if (argInfo == _POPT_SET_BIT && *o == '!' && ((tok - o) % 2))
		argInfo = _POPT_UNSET_BIT;

	    /* Save value in template as appropriate. */
	    switch (argInfo & POPT_ARG_MASK) {
	    long aLong;

	    case POPT_ARG_NONE:
		(void) dbSaveInt(opt, argInfo, 1L);
		break;
	    case POPT_ARG_VAL:
		(void) dbSaveInt(opt, argInfo, (long)opt->val);
	    	break;
	    case POPT_ARG_STRING:
	    {	const char ** t = opt->arg;
		if (t) {
		    *t = _free(*t);
		    *t = xstrdup( (p ? p : "") );
		}
	    }	break;

	    case POPT_ARG_INT:
	    case POPT_ARG_LONG:
		aLong = strtol(p, &pe, 0);
		if (pe) {
		    if (!xstrncasecmp(pe, "Mb", 2))
			aLong *= 1024 * 1024;
		    else if (!xstrncasecmp(pe, "Kb", 2))
			aLong *= 1024;
		    else if (*pe != '\0') {
			rpmError(RPMERR_DBCONFIG,
				_("%s has invalid numeric value, skipped\n"),
				opt->longName);
			continue;
		    }
		}

		if ((argInfo & POPT_ARG_MASK) == POPT_ARG_LONG) {
		    if (aLong == LONG_MIN || aLong == LONG_MAX) {
			rpmError(RPMERR_DBCONFIG,
				_("%s has too large or too small long value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) dbSaveLong(opt, argInfo, aLong);
		    break;
		} else {
		    if (aLong > INT_MAX || aLong < INT_MIN) {
			rpmError(RPMERR_DBCONFIG,
				_("%s has too large or too small integer value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) dbSaveInt(opt, argInfo, aLong);
		}
		break;
	    default:
		break;
	    }
	}
    }

    dbOpts = _free(dbOpts);

    memset(&db3dbi, 0, sizeof(db3dbi));
    /*@-assignexpose@*/
    *dbi = db3dbi;	/* structure assignment */
    /*@=assignexpose@*/

    if (!(dbi->dbi_perms & 0600))
	dbi->dbi_perms = 0644;
    dbi->dbi_mode = rpmdb->db_mode;
    /*@-keeptrans@*/
    dbi->dbi_rpmdb = rpmdb;
    /*@=keeptrans@*/
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
    /*@-globstate@*/
    return dbi;
    /*@=globstate@*/
}

const char *const prDbiOpenFlags(int dbflags, int print_dbenv_flags)
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
	    sprintf(oe, "0x%x", (unsigned)dbflags);
    }
    return buf;
}

#endif
