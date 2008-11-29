/** \ingroup rpmcli
 * \file lib/poptALL.c
 *  Popt tables for all rpm modes.
 */

#include "system.h"
const char *__progname;

#include <rpm/rpmcli.h>
#include <rpm/rpmlib.h>		/* rpmEVR, rpmReadConfigFiles etc */
#include <rpm/rpmgi.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmfileutil.h>

#include "debug.h"

#define POPT_SHOWVERSION	-999
#define POPT_SHOWRC		-998
#define POPT_QUERYTAGS		-997
#define POPT_PREDEFINE		-996
#ifdef  NOTYET
#define POPT_RCFILE		-995
#endif

static int _debug = 0;

extern int _rpmds_nopromote;

extern int _fsm_debug;

extern int _fsm_threads;

extern int _hdr_debug;

extern int _print_pkts;

extern int _psm_debug;

extern int _psm_threads;

extern int _rpmal_debug;

extern int _rpmdb_debug;

extern int _rpmds_debug;

/* XXX avoid -lrpmbuild linkage. */
       int _rpmfc_debug;

extern int _rpmfi_debug;

extern int _rpmgi_debug;

extern int _rpmps_debug;

extern int _rpmsq_debug;

extern int _rpmte_debug;

extern int _rpmts_debug;

extern int _rpmts_stats;

const char * rpmcliPipeOutput = NULL;

const char * rpmcliRcfile = NULL;

const char * rpmcliRootDir = "/";

rpmQueryFlags rpmcliQueryFlags;

extern int _rpmio_debug;

static int rpmcliInitialized = -1;

/**
 * Display rpm version.
 */
static void printVersion(FILE * fp)
{
    fprintf(fp, _("RPM version %s\n"), rpmEVR);
}

/**
 * Make sure that config files have been read.
 * @warning Options like --rcfile and --verbose must precede callers option.
 */
void rpmcliConfigured(void)
{

    if (rpmcliInitialized < 0)
	rpmcliInitialized = rpmReadConfigFiles(rpmcliRcfile, NULL);
    if (rpmcliInitialized)
	exit(EXIT_FAILURE);
}

/**
 */
static void rpmcliAllArgCallback( poptContext con,
                enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                const void * data)
{

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'q':
	rpmSetVerbosity(RPMLOG_WARNING);
	break;
    case 'v':
	rpmIncreaseVerbosity();
	break;
    case POPT_PREDEFINE:
	(void) rpmDefineMacro(NULL, arg, RMIL_CMDLINE);
	break;
    case 'D':
    {   char *s, *t;
	/* XXX Convert '-' in macro name to underscore, skip leading %. */
	s = t = xstrdup(arg);
	while (*t && !risspace(*t)) {
	    if (*t == '-') *t = '_';
	    t++;
	}
	t = s;
	if (*t == '%') t++;
	/* XXX Predefine macro if not initialized yet. */
	if (rpmcliInitialized < 0)
	    (void) rpmDefineMacro(NULL, t, RMIL_CMDLINE);
	rpmcliConfigured();
	(void) rpmDefineMacro(NULL, t, RMIL_CMDLINE);
	(void) rpmDefineMacro(rpmCLIMacroContext, t, RMIL_CMDLINE);
	s = _free(s);
	break;
    }
    case 'E':
	rpmcliConfigured();
	{   char *val = rpmExpand(arg, NULL);
	    fprintf(stdout, "%s\n", val);
	    val = _free(val);
	}
	break;
    case POPT_SHOWVERSION:
	printVersion(stdout);
	exit(EXIT_SUCCESS);
	break;
    case POPT_SHOWRC:
	rpmcliConfigured();
	(void) rpmShowRC(stdout);
	exit(EXIT_SUCCESS);
	break;
    case POPT_QUERYTAGS:
	rpmDisplayQueryTags(stdout);
	exit(EXIT_SUCCESS);
	break;
#if defined(POPT_RCFILE)
    case POPT_RCFILE:		/* XXX FIXME: noop for now */
	break;
#endif
    case RPMCLI_POPT_NODIGEST:
	rpmcliQueryFlags |= VERIFY_DIGEST;
	break;

    case RPMCLI_POPT_NOSIGNATURE:
	rpmcliQueryFlags |= VERIFY_SIGNATURE;
	break;

    case RPMCLI_POPT_NOHDRCHK:
	rpmcliQueryFlags |= VERIFY_HDRCHK;
	break;
    }
}

int ftsOpts = 0;

struct poptOption rpmcliFtsPoptTable[] = {
 { "comfollow", '\0', POPT_BIT_SET,	&ftsOpts, RPMGI_COMFOLLOW,
	N_("follow command line symlinks"), NULL },
 { "logical", '\0', POPT_BIT_SET,	&ftsOpts, RPMGI_LOGICAL,
	N_("logical walk"), NULL },
 { "nochdir", '\0', POPT_BIT_SET,	&ftsOpts, RPMGI_NOCHDIR,
	N_("don't change directories"), NULL },
 { "nostat", '\0', POPT_BIT_SET,	&ftsOpts, RPMGI_NOSTAT,
	N_("don't get stat info"), NULL },
 { "physical", '\0', POPT_BIT_SET,	&ftsOpts, RPMGI_PHYSICAL,
	N_("physical walk"), NULL },
 { "seedot", '\0', POPT_BIT_SET,	&ftsOpts, RPMGI_SEEDOT,
	N_("return dot and dot-dot"), NULL },
 { "xdev", '\0', POPT_BIT_SET,		&ftsOpts, RPMGI_XDEV,
	N_("don't cross devices"), NULL },
 { "whiteout", '\0', POPT_BIT_SET,	&ftsOpts, RPMGI_WHITEOUT,
	N_("return whiteout information"), NULL },
   POPT_TABLEEND
};

struct poptOption rpmcliAllPoptTable[] = {
/* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmcliAllArgCallback, 0, NULL, NULL },

 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_debug, -1,
        NULL, NULL },

 { "predefine", 'D', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_PREDEFINE,
	N_("predefine MACRO with value EXPR"),
	N_("'MACRO EXPR'") },
 { "define", 'D', POPT_ARG_STRING, 0, 'D',
	N_("define MACRO with value EXPR"),
	N_("'MACRO EXPR'") },
 { "eval", 'E', POPT_ARG_STRING, 0, 'E',
	N_("print macro expansion of EXPR"),
	N_("'EXPR'") },
 { "macros", '\0', POPT_ARG_STRING, &macrofiles, 0,
	N_("read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },

 { "nodigest", '\0', 0, 0, RPMCLI_POPT_NODIGEST,
        N_("don't verify package digest(s)"), NULL },
 { "nohdrchk", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, RPMCLI_POPT_NOHDRCHK,
        N_("don't verify database header(s) when retrieved"), NULL },
 { "nosignature", '\0', 0, 0, RPMCLI_POPT_NOSIGNATURE,
        N_("don't verify package signature(s)"), NULL },

 { "pipe", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &rpmcliPipeOutput, 0,
	N_("send stdout to CMD"),
	N_("CMD") },
#if !defined(POPT_RCFILE)
 { "rcfile", '\0', POPT_ARG_STRING, &rpmcliRcfile, 0,
	N_("read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },
#else
 { "rcfile", '\0', 0, NULL, POPT_RCFILE,	
	N_("read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },
#endif
 { "root", 'r', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &rpmcliRootDir, 0,
	N_("use ROOT as top level directory"),
	N_("ROOT") },

 { "querytags", '\0', 0, 0, POPT_QUERYTAGS,
        N_("display known query tags"), NULL },
 { "showrc", '\0', 0, NULL, POPT_SHOWRC,
	N_("display final rpmrc and macro configuration"), NULL },
 { "quiet", '\0', 0, NULL, 'q',
	N_("provide less detailed output"), NULL},
 { "verbose", 'v', 0, NULL, 'v',
	N_("provide more detailed output"), NULL},
 { "version", '\0', 0, NULL, POPT_SHOWVERSION,
	N_("print the version of rpm being used"), NULL },

 { "promoteepoch", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmds_nopromote, 0,
	NULL, NULL},

 { "fsmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fsm_debug, -1,
	N_("debug payload file state machine"), NULL},
 { "fsmthreads", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fsm_threads, -1,
	N_("use threads for file state machine"), NULL},
 { "hdrdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_hdr_debug, -1,
	NULL, NULL},
 { "prtpkts", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_print_pkts, -1,
	NULL, NULL},
 { "psmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_psm_debug, -1,
	N_("debug package state machine"), NULL},
 { "psmthreads", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_psm_threads, -1,
	N_("use threads for package state machine"), NULL},
 { "rpmaldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmal_debug, -1,
	NULL, NULL},
 { "rpmdbdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmdb_debug, -1,
	NULL, NULL},
 { "rpmdsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmds_debug, -1,
	NULL, NULL},
 { "rpmfcdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmfc_debug, -1,
	NULL, NULL},
 { "rpmfidebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmfi_debug, -1,
	NULL, NULL},
 { "rpmgidebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmgi_debug, -1,
	NULL, NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "rpmpsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmps_debug, -1,
	NULL, NULL},
 { "rpmsqdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmsq_debug, -1,
	NULL, NULL},
 { "rpmtedebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmte_debug, -1,
	NULL, NULL},
 { "rpmtsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_debug, -1,
	NULL, NULL},
 { "stats", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_stats, -1,
	NULL, NULL},

   POPT_TABLEEND
};

poptContext
rpmcliFini(poptContext optCon)
{
    optCon = poptFreeContext(optCon);

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    return NULL;
}

poptContext
rpmcliInit(int argc, char *const argv[], struct poptOption * optionsTable)
{
    const char * optArg;
    poptContext optCon;
    int rc;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);       /* Retrofit glibc __progname */

    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }

#if defined(ENABLE_NLS)
    (void) setlocale(LC_ALL, "" );

    (void) bindtextdomain(PACKAGE, LOCALEDIR);
    (void) textdomain(PACKAGE);
#endif

    rpmSetVerbosity(RPMLOG_NOTICE);

    if (optionsTable == NULL) {
	/* Read rpm configuration (if not already read). */
	rpmcliConfigured();
	return NULL;
    }

    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);
    {
	char *poptfile = rpmGenPath(rpmConfigDir(), LIBRPMALIAS_FILENAME, NULL);
	(void) poptReadConfigFile(optCon, poptfile);
	free(poptfile);
    }
    (void) poptReadDefaultConfig(optCon, 1);
    poptSetExecPath(optCon, rpmConfigDir(), 1);

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);
	switch (rc) {
	default:
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		__progname, rc);
	    exit(EXIT_FAILURE);

	    break;
        }
    }

    if (rc < -1) {
	fprintf(stderr, "%s: %s: %s\n", __progname,
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(rc));
	exit(EXIT_FAILURE);
    }

    /* Read rpm configuration (if not already read). */
    rpmcliConfigured();

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    return optCon;
}
