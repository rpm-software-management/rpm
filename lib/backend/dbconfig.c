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

static struct dbiIndex_s staticdbi;
static struct dbConfig_s staticcfg;
static int db_eflags;

/** \ingroup dbi
 */
static const struct poptOption rdbOptions[] = {
 /* Environment options */
   
 { "cdb",	0,POPT_BIT_SET,	&db_eflags, DB_INIT_CDB,
	NULL, NULL },
 { "lock",	0,POPT_BIT_SET,	&db_eflags, DB_INIT_LOCK,
	NULL, NULL },
 { "log",	0,POPT_BIT_SET,	&db_eflags, DB_INIT_LOG,
	NULL, NULL },
 { "txn",	0,POPT_BIT_SET,	&db_eflags, DB_INIT_TXN,
	NULL, NULL },
 { "recover",	0,POPT_BIT_SET,	&db_eflags, DB_RECOVER,
	NULL, NULL },
 { "recover_fatal", 0,POPT_BIT_SET,	&db_eflags, DB_RECOVER_FATAL,
	NULL, NULL },
 { "lockdown",	0,POPT_BIT_SET,	&db_eflags, DB_LOCKDOWN,
	NULL, NULL },
 { "private",	0,POPT_BIT_SET,	&db_eflags, DB_PRIVATE,
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
 { "nommap",	0,POPT_BIT_SET,		&staticdbi.dbi_oflags, DB_NOMMAP,
	NULL, NULL },

 { "nodbsync",	0,POPT_ARG_NONE,	&staticdbi.dbi_no_dbsync, 0,
	NULL, NULL },
 { "lockdbfd",	0,POPT_ARG_NONE,	&staticdbi.dbi_lockdbfd, 0,
	NULL, NULL },

    POPT_TABLEEND
};

dbiIndex dbiFree(dbiIndex dbi)
{
    if (dbi) {
	free(dbi);
    }
    return NULL;
}

dbiIndex dbiNew(rpmdb rdb, rpmDbiTagVal rpmtag)
{
    dbiIndex dbi = xcalloc(1, sizeof(*dbi));
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

	memset(&staticdbi, 0, sizeof(staticdbi));
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

    *dbi = staticdbi;	/* structure assignment */
    memset(&staticdbi, 0, sizeof(staticdbi));

    /* FIX: figger lib/dbi refcounts */
    dbi->dbi_rpmdb = rdb;
    dbi->dbi_file = rpmTagGetName(rpmtag);
    dbi->dbi_type = (rpmtag == RPMDBI_PACKAGES) ? DBI_PRIMARY : DBI_SECONDARY;
    dbi->dbi_byteswapped = -1;	/* -1 unknown, 0 native order, 1 alien order */

    /* XXX FIXME: Get environment configuration out of here! */
    if (rdb->db_dbenv == NULL) {
	struct dbConfig_s * cfg = &rdb->cfg;
	*cfg = staticcfg;	/* structure assignment */
	/* Throw in some defaults if configuration didn't set any */
	if (!cfg->db_mmapsize) cfg->db_mmapsize = 16 * 1024 * 1024;
	if (!cfg->db_cachesize) cfg->db_cachesize = 8 * 1024 * 1024;
    }

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
	    if (!(opt->arg == &db_eflags))
		continue;
	} else {
	    if (!(opt->arg == &staticdbi.dbi_oflags))
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
