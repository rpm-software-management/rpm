/** \ingroup rpmdb
 * \file lib/dbconfig.c
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

struct _dbiIndex db3dbi;

/** \ingroup db3
 *  Analogue to struct poptOption
 */
struct dbOption {
    const char * longName;	/* may be NULL */
    int argInfo;
    void * arg;			/* depends on argInfo */
    int val;			/* 0 means don't return, just update flag */
};

#define	_POPT_SET_BIT	(POPT_ARG_VAL|POPT_ARGFLAG_OR)

/*@-immediatetrans@*/
/** \ingroup db3
 */
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

 { NULL, 0, NULL, 0 }
};
/*@=immediatetrans@*/

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
	/*@notreached@*/ break;
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
	/*@notreached@*/ break;
    }
    return 0;
}

void db3Free(dbiIndex dbi) {
    if (dbi) {
	dbi->dbi_root = _free(dbi->dbi_root);
	dbi->dbi_home = _free(dbi->dbi_home);
	dbi->dbi_file = _free(dbi->dbi_file);
	dbi->dbi_subfile = _free(dbi->dbi_subfile);
	dbi->dbi_errpfx = _free(dbi->dbi_errpfx);
	dbi->dbi_re_source = _free(dbi->dbi_re_source);
	dbi->dbi_dbenv = _free(dbi->dbi_dbenv);
	dbi->dbi_dbinfo = _free(dbi->dbi_dbinfo);
	dbi = _free(dbi);
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

	    while (*o && xisspace(*o))
		o++;
	    for (oe = o; oe && *oe; oe++) {
		if (xisspace(*oe))
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
		rpmError(RPMERR_DBCONFIG,
			_("unrecognized db option: \"%s\" ignored\n"), o);
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
		*t = _free(*t);
		*t = xstrdup( (p ? p : "") );
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

		if ((opt->argInfo & POPT_ARG_MASK) == POPT_ARG_LONG) {
		    if (aLong == LONG_MIN || aLong == LONG_MAX) {
			rpmError(RPMERR_DBCONFIG,
				_("%s has too large or too small long value, skipped\n"),
				opt->longName);
			continue;
		    }
		    (void) dbSaveLong(opt, aLong);
		    break;
		} else {
		    if (aLong > INT_MAX || aLong < INT_MIN) {
			rpmError(RPMERR_DBCONFIG,
				_("%s has too large or too small integer value, skipped\n"),
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
    return dbi;
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
