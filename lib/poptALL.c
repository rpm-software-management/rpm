/** \ingroup rpmcli
 * \file lib/poptALL.c
 *  Popt tables for all rpm modes.
 */

#include "system.h"

#include <rpmcli.h>

#include "debug.h"

#define POPT_SHOWVERSION	-999
#define POPT_SHOWRC		-998
#define POPT_QUERYTAGS		-997
#define POPT_PREDEFINE		-996
#ifdef  NOTYET
#define POPT_RCFILE		-995
#endif

/*@unchecked@*/
static int _debug = 0;

/*@-exportheadervar@*/
/*@unchecked@*/
extern int _rpmds_nopromote;

/*@unchecked@*/
extern int _fps_debug;

/*@unchecked@*/
extern int _fsm_debug;

/*@unchecked@*/
extern int _hdr_debug;

/*@unchecked@*/
extern int _psm_debug;

/*@unchecked@*/
extern int _rpmal_debug;

/*@unchecked@*/
extern int _rpmdb_debug;

/*@unchecked@*/
extern int _rpmds_debug;

/* XXX avoid -lrpmbuild linkage. */
/*@unchecked@*/
       int _rpmfc_debug;

/*@unchecked@*/
extern int _rpmfi_debug;

/*@unchecked@*/
extern int _rpmps_debug;

/*@unchecked@*/
extern int _rpmsq_debug;

/*@unchecked@*/
extern int _rpmte_debug;

/*@unchecked@*/
extern int _rpmts_debug;

/*@unchecked@*/
extern int noLibio;
/*@=exportheadervar@*/

/*@unchecked@*/
const char * rpmcliPipeOutput = NULL;

/*@unchecked@*/
const char * rpmcliRcfile = NULL;

/*@unchecked@*/
const char * rpmcliRootDir = "/";

/*@unchecked@*/
rpmQueryFlags rpmcliQueryFlags;

/*@-exportheadervar@*/
/*@unchecked@*/
extern int _ftp_debug;
/*@unchecked@*/
extern int noLibio;
/*@unchecked@*/
extern int _rpmio_debug;
/*@=exportheadervar@*/

/*@unchecked@*/
static int rpmcliInitialized = -1;

/**
 * Display rpm version.
 */
static void printVersion(FILE * fp)
	/*@globals rpmEVR, fileSystem @*/
	/*@modifies *fp, fileSystem @*/
{
    fprintf(fp, _("RPM version %s\n"), rpmEVR);
}

/**
 * Make sure that config files have been read.
 * @warning Options like --rcfile and --verbose must precede callers option.
 */
/*@mayexit@*/
void rpmcliConfigured(void)
	/*@globals rpmcliInitialized, rpmCLIMacroContext, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies rpmcliInitialized, rpmCLIMacroContext, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{

    if (rpmcliInitialized < 0)
	rpmcliInitialized = rpmReadConfigFiles(rpmcliRcfile, NULL);
    if (rpmcliInitialized)
	exit(EXIT_FAILURE);
}

/**
 */
/*@-bounds@*/
static void rpmcliAllArgCallback( /*@unused@*/ poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ const void * data)
	/*@globals rpmcliQueryFlags, rpmCLIMacroContext, rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies rpmcliQueryFlags, rpmCLIMacroContext, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{

    /* XXX avoid accidental collisions with POPT_BIT_SET for flags */
    /*@-branchstate@*/
    if (opt->arg == NULL)
    switch (opt->val) {
    case 'q':
	rpmSetVerbosity(RPMMESS_QUIET);
	break;
    case 'v':
	rpmIncreaseVerbosity();
	break;
    case POPT_PREDEFINE:
	(void) rpmDefineMacro(NULL, arg, RMIL_CMDLINE);
	break;
    case 'D':
	/* XXX Predefine macro if not initialized yet. */
	if (rpmcliInitialized < 0)
	    (void) rpmDefineMacro(NULL, arg, RMIL_CMDLINE);
	rpmcliConfigured();
/*@-type@*/
	(void) rpmDefineMacro(NULL, arg, RMIL_CMDLINE);
	(void) rpmDefineMacro(rpmCLIMacroContext, arg, RMIL_CMDLINE);
/*@=type@*/
	break;
    case 'E':
	rpmcliConfigured();
	{   const char *val = rpmExpand(arg, NULL);
	    fprintf(stdout, "%s\n", val);
	    val = _free(val);
	}
	break;
    case POPT_SHOWVERSION:
	printVersion(stdout);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
    case POPT_SHOWRC:
	rpmcliConfigured();
	(void) rpmShowRC(stdout);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
    case POPT_QUERYTAGS:
	rpmDisplayQueryTags(stdout);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
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
    /*@=branchstate@*/
}

/*@-bitwisesigned -compmempass @*/
/*@unchecked@*/
struct poptOption rpmcliAllPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
        rpmcliAllArgCallback, 0, NULL, NULL },
/*@=type@*/

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
#if HAVE_LIBIO_H && defined(_G_IO_IO_FILE_VERSION)
 { "nolibio", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noLibio, 1,
	N_("disable use of libio(3) API"), NULL},
#endif
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

#if HAVE_LIBIO_H && defined(_G_IO_IO_FILE_VERSION)
 { "nolibio", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noLibio, 1,
       N_("disable use of libio(3) API"), NULL},
#endif

 { "promoteepoch", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmds_nopromote, 0,
	NULL, NULL},

 { "fpsdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fps_debug, -1,
	NULL, NULL},
 { "fsmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_fsm_debug, -1,
	N_("debug payload file state machine"), NULL},
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "hdrdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_hdr_debug, -1,
	NULL, NULL},
#ifdef	DYING
 { "poptdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_popt_debug, -1,
	N_("debug option/argument processing"), NULL},
#endif
 { "psmdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_psm_debug, -1,
	N_("debug package state machine"), NULL},
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
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},

   POPT_TABLEEND
};
/*@=bitwisesigned =compmempass @*/

poptContext
rpmcliFini(poptContext optCon)
{
    optCon = poptFreeContext(optCon);

#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-noeffect@*/
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif

    return NULL;
}

/*@-globstate@*/
poptContext
rpmcliInit(int argc, char *const argv[], struct poptOption * optionsTable)
{
    const char * optArg;
    poptContext optCon;
    int rc;

#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-noeffect@*/
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif
/*@-globs -mods@*/
    setprogname(argv[0]);       /* Retrofit glibc __progname */

    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }
/*@=globs =mods@*/

#if defined(ENABLE_NLS) && !defined(__LCLINT__)
    (void) setlocale(LC_ALL, "" );
    (void) bindtextdomain(PACKAGE, LOCALEDIR);
    (void) textdomain(PACKAGE);
#endif

    rpmSetVerbosity(RPMMESS_NORMAL);

    if (optionsTable == NULL) {
	/* Read rpm configuration (if not already read). */
	rpmcliConfigured();
	return NULL;
    }

/*@-nullpass -temptrans@*/
    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);
/*@=nullpass =temptrans@*/
    (void) poptReadConfigFile(optCon, LIBRPMALIAS_FILENAME);
    (void) poptReadDefaultConfig(optCon, 1);
    poptSetExecPath(optCon, RPMCONFIGDIR, 1);

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);
	switch (rc) {
	default:
/*@-nullpass@*/
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		__progname, rc);
/*@=nullpass@*/
	    exit(EXIT_FAILURE);

	    /*@notreached@*/ /*@switchbreak@*/ break;
        }
    }

    if (rc < -1) {
/*@-nullpass@*/
	fprintf(stderr, "%s: %s: %s\n", __progname,
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		poptStrerror(rc));
/*@=nullpass@*/
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
/*@=globstate@*/
