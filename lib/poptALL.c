/*@-boundsread@*/
/** \ingroup rpmcli
 * \file lib/poptALL.c
 *  Popt tables for all rpm modes.
 */

#include "system.h"

#include <rpmcli.h>

#include "debug.h"

#ifdef  NOTYET
#define POPT_RCFILE		-1022
#endif
#define POPT_SHOWVERSION	-999
#define POPT_SHOWRC		-998

/*@unchecked@*/
static int _debug = 0;

/*@unchecked@*/
/*@observer@*/ /*@null@*/
static const char * pipeOutput = NULL;
/*@unchecked@*/
/*@observer@*/ /*@null@*/
static const char * rcfile = NULL;
/*@unchecked@*/
static const char * rootdir = "/";

/*@-exportheadervar@*/
/*@unchecked@*/
extern int _ftp_debug;
/*@unchecked@*/
extern int noLibio;
/*@unchecked@*/
extern int _rpmio_debug;
/*@=exportheadervar@*/

/**
 * Display rpm version.
 */
static void printVersion(void)
	/*@globals rpmEVR, fileSystem @*/
	/*@modifies fileSystem @*/
{
    fprintf(stdout, _("RPM version %s\n"), rpmEVR);
}

/**
 * Make sure that config files have been read.
 * @warning Options like --rcfile and --verbose must precede callers option.
 */
/*@mayexit@*/
static void rpmcliConfigured(void)
	/*@globals internalState @*/
	/*@modifies internalState @*/
{
    static int initted = -1;

    if (initted < 0)
	initted = rpmReadConfigFiles(rcfile, NULL);
    if (initted)
	exit(EXIT_FAILURE);
}

/**
 */
/*@-bounds@*/
static void rpmcliAllArgCallback( /*@unused@*/ poptContext con,
                /*@unused@*/ enum poptCallbackReason reason,
                const struct poptOption * opt, const char * arg,
                /*@unused@*/ const void * data)
        /*@*/
{
#if 0
/*@observer@*/
static const char *cbreasonstr[] = { "PRE", "POST", "OPTION", "?WTFO?" };
fprintf(stderr, "*** rpmcliALL: -%c,--%s %s %s opt %p arg %p val %d\n", opt->shortName, opt->longName, arg, cbreasonstr[reason&0x3], opt, opt->arg, opt->val);
#endif

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
    case 'D':
	(void) rpmDefineMacro(NULL, arg, RMIL_CMDLINE);
	rpmcliConfigured();
	(void) rpmDefineMacro(rpmCLIMacroContext, arg, RMIL_CMDLINE);
	break;
    case 'E':
	rpmcliConfigured();
	{   const char *val = rpmExpand(arg, NULL);
	    fprintf(stdout, "%s\n", val);
	    val = _free(val);
	}
	break;
    case POPT_SHOWVERSION:
	printVersion();
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
    case POPT_SHOWRC:
	rpmcliConfigured();
	(void) rpmShowRC(stdout);
	exit(EXIT_SUCCESS);
	/*@notreached@*/ break;
#if defined(POPT_RCFILE)
    case POPT_RCFILE:		/* XXX FIXME: noop for now */
	break;
#endif
    }
    /*@=branchstate@*/
}

/*@unchecked@*/
struct poptOption rpmcliAllPoptTable[] = {
/*@-type@*/ /* FIX: cast? */
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA,
        rpmcliAllArgCallback, 0, NULL, NULL },
/*@=type@*/

 { "debug", 'd', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_debug, -1,
        NULL, NULL },
 { "quiet", '\0', POPT_ARGFLAG_DOC_HIDDEN, NULL, 'q',
	N_("provide less detailed output"), NULL},
 { "verbose", 'v', 0, 0, 'v',
	N_("provide more detailed output"), NULL},
 { "version", '\0', 0, 0, POPT_SHOWVERSION,
	N_("print the version of rpm being used"), NULL },

 { "define", 'D', POPT_ARG_STRING, 0, 'D',
	N_("define MACRO with value EXPR"),
	N_("'MACRO EXPR'") },
 { "eval", 'E', POPT_ARG_STRING, 0, 'E',
	N_("print macro expansion of EXPR"),
	N_("'EXPR'") },
 { "pipe", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &pipeOutput, 0,
	N_("send stdout to <cmd>"),
	N_("<cmd>") },
 { "root", 'r', POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT | POPT_ARGFLAG_DOC_HIDDEN, &rootdir, 0,
	N_("use <dir> as the top level directory"),
	N_("<dir>") },
 { "macros", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &macrofiles, 0,
	N_("read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },
#if !defined(POPT_RCFILE)
 { "rcfile", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &rcfile, 0,
	N_("read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },
#else
 { "rcfile", '\0', 0, 0, POPT_RCFILE|POPT_ARGFLAG_DOC_HIDDEN,	
	N_("read <FILE:...> instead of default file(s)"),
	N_("<FILE:...>") },
#endif
 { "showrc", '\0', 0, POPT_SHOWRC|POPT_ARGFLAG_DOC_HIDDEN, 0,
	N_("display final rpmrc and macro configuration"),
	NULL },

#if HAVE_LIBIO_H && defined(_G_IO_IO_FILE_VERSION)
 { "nolibio", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noLibio, 1,
	N_("disable use of libio(3) API"), NULL},
#endif

 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},

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

    (void)setlocale(LC_ALL, "" );

#ifdef  __LCLINT__
#define LOCALEDIR       "/usr/share/locale"
#endif
    (void)bindtextdomain(PACKAGE, LOCALEDIR);
    (void)textdomain(PACKAGE);

    rpmSetVerbosity(RPMMESS_NORMAL);

    if (optionsTable == NULL) {
	/* Read rpm configuration (if not already read). */
	rpmcliConfigured();
	return NULL;
    }

    optCon = poptGetContext(__progname, argc, (const char **)argv, optionsTable, 0);
    (void) poptReadConfigFile(optCon, LIBRPMALIAS_FILENAME);
    poptReadDefaultConfig(optCon, 1);
    poptSetExecPath(optCon, RPMCONFIGDIR, 1);

    /* Process all options, whine if unknown. */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);
	switch (rc) {
	default:
	    fprintf(stderr, _("%s: option table misconfigured (%d)\n"),
		__progname, rc);
	    exit(EXIT_FAILURE);

	    /*@notreached@*/ break;
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

/*@=boundsread@*/
