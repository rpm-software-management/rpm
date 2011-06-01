/** \ingroup rpmcli
 * \file lib/poptALL.c
 *  Popt tables for all rpm modes.
 */

#include "system.h"
const char *__progname;

#if HAVE_MCHECK_H
#include <mcheck.h>
#endif

#include <rpm/rpmcli.h>
#include <rpm/rpmlib.h>		/* rpmEVR, rpmReadConfigFiles etc */
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmfileutil.h>

#include "debug.h"

#define POPT_SHOWVERSION	-999
#define POPT_SHOWRC		-998
#define POPT_QUERYTAGS		-997
#define POPT_PREDEFINE		-996
#define POPT_DBPATH		-995

static int _debug = 0;

extern int _rpmds_nopromote;

extern int _fsm_debug;

extern int _print_pkts;

extern int _psm_debug;

/* XXX avoid -lrpmbuild linkage. */
       int _rpmfc_debug;

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
    case POPT_DBPATH:
	rpmcliConfigured();
	addMacro(NULL, "_dbpath", NULL, arg, RMIL_CMDLINE);
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

struct poptOption rpmcliAllPoptTable[] = {
/* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmcliAllArgCallback, 0, NULL, NULL },

 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_debug, -1,
        NULL, NULL },

 { "predefine", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_PREDEFINE,
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
 { "rcfile", '\0', POPT_ARG_STRING, &rpmcliRcfile, 0,
	N_("read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },
 { "root", 'r', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT, &rpmcliRootDir, 0,
	N_("use ROOT as top level directory"),
	N_("ROOT") },
 { "dbpath", '\0', POPT_ARG_STRING, 0, POPT_DBPATH,
	N_("use database in DIRECTORY"),
	N_("DIRECTORY") },

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
 { "prtpkts", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_print_pkts, -1,
	NULL, NULL},
 { "rpmfcdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmfc_debug, -1,
	NULL, NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "stats", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmts_stats, -1,
	NULL, NULL},

   POPT_TABLEEND
};

poptContext
rpmcliFini(poptContext optCon)
{
    poptFreeContext(optCon);
    rpmFreeMacros(NULL);
    rpmFreeMacros(rpmCLIMacroContext);
    rpmFreeRpmrc();
    rpmlogClose();
    rpmcliInitialized = -1;

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
    const char *ctx, *execPath;

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

    /* XXX hack to get popt working from build tree wrt lt-foo names */
    ctx = rstreqn(__progname, "lt-", 3) ? __progname + 3 : __progname;

    optCon = poptGetContext(ctx, argc, (const char **)argv, optionsTable, 0);
    {
	char *poptfile = rpmGenPath(rpmConfigDir(), LIBRPMALIAS_FILENAME, NULL);
	(void) poptReadConfigFile(optCon, poptfile);
	free(poptfile);
    }
    (void) poptReadDefaultConfig(optCon, 1);

    if ((execPath = getenv("RPM_POPTEXEC_PATH")) == NULL)
	execPath = LIBRPMALIAS_EXECPATH;
    poptSetExecPath(optCon, execPath, 1);

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
