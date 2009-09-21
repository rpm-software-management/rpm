/** \ingroup rpmdb
 * \file lib/dbconfig.c
 */

#include "system.h"

#include <popt.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmlog.h>
#include <rpm/argv.h>
#include "lib/rpmdb_internal.h"
#include "debug.h"

static struct _dbiIndex db3dbi;

/** \ingroup db3
 */
static const struct poptOption rdbOptions[] = {
 /* XXX DB_CXX_NO_EXCEPTIONS */

 { "create",	0,POPT_BIT_SET,	&db3dbi.dbi_oeflags, DB_CREATE,
	NULL, NULL },
 { "thread",	0,POPT_BIT_SET,	&db3dbi.dbi_oeflags, DB_THREAD,
	NULL, NULL },

 { "force",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_FORCE,
	NULL, NULL },
 { "cdb",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_CDB,
	NULL, NULL },
 { "lock",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_LOCK,
	NULL, NULL },
 { "log",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_LOG,
	NULL, NULL },
 { "mpool",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_MPOOL,
	NULL, NULL },
 { "txn",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_INIT_TXN,
	NULL, NULL },
 { "joinenv",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_JOINENV,
	NULL, NULL },
 { "recover",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_RECOVER,
	NULL, NULL },
 { "recover_fatal", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_RECOVER_FATAL,
	NULL, NULL },
 { "shared",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_SYSTEM_MEM,
	NULL, NULL },
 { "txn_nosync", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_TXN_NOSYNC,
	NULL, NULL },
 { "use_environ_root", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_USE_ENVIRON_ROOT,
	NULL, NULL },
 { "use_environ", 0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_USE_ENVIRON,
	NULL, NULL },
 { "lockdown",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_LOCKDOWN,
	NULL, NULL },
 { "private",	0,POPT_BIT_SET,	&db3dbi.dbi_eflags, DB_PRIVATE,
	NULL, NULL },

 { "txn_sync",	0,POPT_BIT_SET,	&db3dbi.dbi_tflags, DB_TXN_SYNC,
	NULL, NULL },
 { "txn_nowait",0,POPT_BIT_SET,	&db3dbi.dbi_tflags, DB_TXN_NOWAIT,
	NULL, NULL },

 { "excl",	0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_EXCL,
	NULL, NULL },
 { "nommap",	0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_NOMMAP,
	NULL, NULL },
 { "rdonly",	0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_RDONLY,
	NULL, NULL },
 { "truncate",	0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_TRUNCATE,
	NULL, NULL },
 { "fcntl_locking",0,POPT_BIT_SET,	&db3dbi.dbi_oflags, DB_FCNTL_LOCKING,
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
 { "tmpdir",	0,POPT_ARG_STRING,	&db3dbi.dbi_tmpdir, 0,
	NULL, NULL },

 { "verify",	0,POPT_ARG_NONE,	&db3dbi.dbi_verify_on_close, 0,
	NULL, NULL },
 { "usedbenv",	0,POPT_ARG_NONE,	&db3dbi.dbi_use_dbenv, 0,
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

#if defined(DB_VERB_CHKPOINT)
 { "chkpoint",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_CHKPOINT,
	NULL, NULL },
#endif
 { "deadlock",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_DEADLOCK,
	NULL, NULL },
 { "recovery",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_RECOVERY,
	NULL, NULL },
 { "waitsfor",	0,POPT_BIT_SET,	&db3dbi.dbi_verbose, DB_VERB_WAITSFOR,
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

 { "mmapsize", 0,POPT_ARG_INT,		&db3dbi.dbi_mmapsize, 0,
	NULL, NULL },
 { "mp_mmapsize", 0,POPT_ARG_INT,	&db3dbi.dbi_mmapsize, 0,
	NULL, NULL },
 { "mp_size",	0,POPT_ARG_INT,		&db3dbi.dbi_cachesize, 0,
	NULL, NULL },
 { "pagesize",	0,POPT_ARG_INT,		&db3dbi.dbi_pagesize, 0,
	NULL, NULL },

/* XXX bt_minkey */
/* XXX bt_compare */
/* XXX bt_dup_compare */
/* XXX bt_prefix */
 { "bt_dup",	0,POPT_BIT_SET,	&db3dbi.dbi_bt_flags, DB_DUP,
	NULL, NULL },
 { "bt_dupsort",0,POPT_BIT_SET,	&db3dbi.dbi_bt_flags, DB_DUPSORT,
	NULL, NULL },
 { "bt_recnum",	0,POPT_BIT_SET,	&db3dbi.dbi_bt_flags, DB_RECNUM,
	NULL, NULL },
 { "bt_revsplitoff", 0,POPT_BIT_SET,	&db3dbi.dbi_bt_flags, DB_REVSPLITOFF,
	NULL, NULL },

 { "h_dup",	0,POPT_BIT_SET,	&db3dbi.dbi_h_flags, DB_DUP,
	NULL, NULL },
 { "h_dupsort",	0,POPT_BIT_SET,	&db3dbi.dbi_h_flags, DB_DUPSORT,
	NULL, NULL },
 { "h_ffactor",	0,POPT_ARG_INT,		&db3dbi.dbi_h_ffactor, 0,
	NULL, NULL },
 { "h_nelem",	0,POPT_ARG_INT,		&db3dbi.dbi_h_nelem, 0,
	NULL, NULL },

 { "re_renumber", 0,POPT_BIT_SET,	&db3dbi.dbi_re_flags, DB_RENUMBER,
	NULL, NULL },
 { "re_snapshot",0,POPT_BIT_SET,	&db3dbi.dbi_re_flags, DB_SNAPSHOT,
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

    POPT_TABLEEND
};

dbiIndex db3Free(dbiIndex dbi)
{
    if (dbi) {
	dbi->dbi_file = _free(dbi->dbi_file);
	dbi->dbi_subfile = _free(dbi->dbi_subfile);
	dbi->dbi_tmpdir = _free(dbi->dbi_tmpdir);
	dbi->dbi_errpfx = _free(dbi->dbi_errpfx);
	dbi->dbi_re_source = _free(dbi->dbi_re_source);
	dbi->dbi_stats = _free(dbi->dbi_stats);
	dbi = _free(dbi);
    }
    return dbi;
}

/** @todo Set a reasonable "last gasp" default db config. */
static const char * const db3_config_default =
    "db3:hash:mpool:cdb:verbose:mp_mmapsize=8Mb:cachesize=512Kb:pagesize=512:perms=0644";

dbiIndex db3New(rpmdb rpmdb, rpmTag rpmtag)
{
    dbiIndex dbi = xcalloc(1, sizeof(*dbi));
    char *dbOpts;

    dbOpts = rpmExpand("%{_dbi_config_", rpmTagGetName(rpmtag), "}", NULL);
    
    if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	dbOpts = _free(dbOpts);
	dbOpts = rpmExpand("%{_dbi_config}", NULL);
	if (!(dbOpts && *dbOpts && *dbOpts != '%')) {
	    dbOpts = rpmExpand(db3_config_default, NULL);
	}
    }

    /* Parse the options for the database element(s). */
    if (dbOpts && *dbOpts && *dbOpts != '%') {
	char *o, *oe;
	char *p, *pe;

	memset(&db3dbi, 0, sizeof(db3dbi));
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

    *dbi = db3dbi;	/* structure assignment */
    memset(&db3dbi, 0, sizeof(db3dbi));

    if (!(dbi->dbi_perms & 0600))
	dbi->dbi_perms = 0644;
    dbi->dbi_mode = rpmdb->db_mode;
    /* FIX: figger lib/dbi refcounts */
    dbi->dbi_rpmdb = rpmdb;
    dbi->dbi_rpmtag = rpmtag;
    if (!dbi->dbi_file) dbi->dbi_file = xstrdup(rpmTagGetName(rpmtag));
    
    /*
     * Inverted lists have join length of 2, primary data has join length of 1.
     */
    switch (rpmtag) {
    case RPMDBI_PACKAGES:
    case RPMDBI_DEPENDS:
	dbi->dbi_jlen = 1 * sizeof(int32_t);
	break;
    default:
	dbi->dbi_jlen = 2 * sizeof(int32_t);
	break;
    }

    dbi->dbi_byteswapped = -1;	/* -1 unknown, 0 native order, 1 alien order */

    if (!dbi->dbi_use_dbenv) {		/* db3 dbenv is always used now. */
	dbi->dbi_use_dbenv = 1;
	dbi->dbi_eflags |= (DB_INIT_MPOOL|DB_JOINENV);
	dbi->dbi_mmapsize = 16 * 1024 * 1024;
	dbi->dbi_cachesize = 1 * 1024 * 1024;
    }

    if ((dbi->dbi_bt_flags | dbi->dbi_h_flags) & DB_DUP)
	dbi->dbi_permit_dups = 1;

    /* FIX: *(rdbOptions->arg) reachable */
    return dbi;
}

char * prDbiOpenFlags(int dbflags, int print_dbenv_flags)
{
    ARGV_t flags = NULL;
    const struct poptOption *opt;
    char *buf;

    for (opt = rdbOptions; opt->longName != NULL; opt++) {
	if (opt->argInfo != POPT_BIT_SET)
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
	
    return buf;
}
