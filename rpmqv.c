#include "system.h"

#define	_AUTOHELP

#if defined(IAM_RPM) || defined(__LCLINT__)
#define	IAM_RPMBT
#define	IAM_RPMDB
#define	IAM_RPMEIU
#define	IAM_RPMQV
#define	IAM_RPMK
#endif

#include <rpmcli.h>
#include <rpmbuild.h>

#define	POPT_NODEPS		1025
#define	POPT_FORCE		1026
#define	POPT_NOMD5		1027
#define	POPT_NOSCRIPTS		1028

#ifdef	IAM_RPMBT
#include "build.h"
#define GETOPT_REBUILD		1003
#define GETOPT_RECOMPILE	1004
#endif

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
#include "signature.h"
#endif

#include "debug.h"

#define GETOPT_DBPATH		1010
#define GETOPT_SHOWRC		1018
#define	GETOPT_DEFINEMACRO	1020
#define	GETOPT_EVALMACRO	1021
#ifdef	NOTYET
#define	GETOPT_RCFILE		1022
#endif

enum modes {

    MODE_QUERY		= (1 <<  0),
    MODE_VERIFY		= (1 <<  3),
    MODE_QUERYTAGS	= (1 <<  9),
#define	MODES_QV (MODE_QUERY | MODE_VERIFY)

    MODE_INSTALL	= (1 <<  1),
    MODE_ERASE		= (1 <<  2),
    MODE_ROLLBACK	= (1 << 14),
#define	MODES_IE (MODE_INSTALL | MODE_ERASE | MODE_ROLLBACK)

    MODE_BUILD		= (1 <<  4),
    MODE_REBUILD	= (1 <<  5),
    MODE_RECOMPILE	= (1 <<  8),
    MODE_TARBUILD	= (1 << 11),
#define	MODES_BT (MODE_BUILD | MODE_TARBUILD | MODE_REBUILD | MODE_RECOMPILE)

    MODE_CHECKSIG	= (1 <<  6),
    MODE_RESIGN		= (1 <<  7),
#define	MODES_K	 (MODE_CHECKSIG | MODE_RESIGN)

    MODE_INITDB		= (1 << 10),
    MODE_REBUILDDB	= (1 << 12),
    MODE_VERIFYDB	= (1 << 13),
#define	MODES_DB (MODE_INITDB | MODE_REBUILDDB | MODE_VERIFYDB)


    MODE_UNKNOWN	= 0
};

#define	MODES_FOR_DBPATH	(MODES_BT | MODES_IE | MODES_QV | MODES_DB)
#define	MODES_FOR_NODEPS	(MODES_BT | MODES_IE | MODE_VERIFY)
#define	MODES_FOR_TEST		(MODES_BT | MODES_IE)
#define	MODES_FOR_ROOT		(MODES_BT | MODES_IE | MODES_QV | MODES_DB)

/*@-exportheadervar@*/
extern int _ftp_debug;
extern int noLibio;
extern int _rpmio_debug;
extern int _url_debug;

/*@-varuse@*/
/*@observer@*/ extern const char * rpmNAME;
/*@=varuse@*/
/*@observer@*/ extern const char * rpmEVR;
/*@-varuse@*/
extern int rpmFLAGS;
/*@=varuse@*/

extern struct MacroContext_s rpmCLIMacroContext;
/*@=exportheadervar@*/

/* options for all executables */

static int help = 0;
static int noUsageMsg = 0;
/*@observer@*/ /*@null@*/ static const char * pipeOutput = NULL;
static int quiet = 0;
/*@observer@*/ /*@null@*/ static const char * rcfile = NULL;
/*@observer@*/ /*@null@*/ static char * rootdir = "/";
static int showrc = 0;
static int showVersion = 0;

static struct poptOption rpmAllPoptTable[] = {
 { "version", '\0', 0, &showVersion, 0,
	N_("print the version of rpm being used"),
	NULL },
 { "quiet", '\0', 0, &quiet, 0,
	N_("provide less detailed output"), NULL},
 { "verbose", 'v', 0, 0, 'v',
	N_("provide more detailed output"), NULL},
 { "define", '\0', POPT_ARG_STRING, 0, GETOPT_DEFINEMACRO,
	N_("define macro <name> with value <body>"),
	N_("'<name> <body>'") },
 { "eval", '\0', POPT_ARG_STRING, 0, GETOPT_EVALMACRO,
	N_("print macro expansion of <expr>+"),
	N_("<expr>+") },
 { "pipe", '\0', POPT_ARG_STRING|POPT_ARGFLAG_DOC_HIDDEN, &pipeOutput, 0,
	N_("send stdout to <cmd>"),
	N_("<cmd>") },
 { "root", 'r', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &rootdir, 0,
	N_("use <dir> as the top level directory"),
	N_("<dir>") },
 { "macros", '\0', POPT_ARG_STRING, &macrofiles, 0,
	N_("read <file:...> instead of default macro file(s)"),
	N_("<file:...>") },
#if !defined(GETOPT_RCFILE)
 { "rcfile", '\0', POPT_ARG_STRING, &rcfile, 0,
	N_("read <file:...> instead of default rpmrc file(s)"),
	N_("<file:...>") },
#else
 { "rcfile", '\0', 0, 0, GETOPT_RCFILE,	
	N_("read <file:...> instead of default rpmrc file(s)"),
	N_("<file:...>") },
#endif
 { "showrc", '\0', 0, &showrc, GETOPT_SHOWRC,
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

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {

 /* XXX colliding options */
#if defined(IAM_RPMQV) || defined(IAM_RPMEIU) || defined(IAM_RPMBT)
 {  NULL, 'i', POPT_ARGFLAG_DOC_HIDDEN, 0, 'i',			NULL, NULL},
 {  "nodeps", 0, POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_NODEPS,	NULL, NULL},
 {  "noscripts", 0, POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_NOSCRIPTS,	NULL, NULL},
 {  "nomd5", 0, POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_NOMD5,		NULL, NULL},
 {  "force", 0, POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_FORCE,		NULL, NULL},
#endif

#ifdef	IAM_RPMQV
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQueryPoptTable, 0,
	N_("Query options (with -q or --query):"),
	NULL },
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmVerifyPoptTable, 0,
	N_("Verify options (with -V or --verify):"),
	NULL },
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMK
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmSignPoptTable, 0,
	N_("Signature options:"),
	NULL },
#endif	/* IAM_RPMK */

#ifdef	IAM_RPMDB
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmDatabasePoptTable, 0,
	N_("Database options:"),
	NULL },
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMBT
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmBuildPoptTable, 0,
	N_("Build options with [ <specfile> | <tarball> | <source package> ]:"),
	NULL },
#endif	/* IAM_RPMBT */

#ifdef	IAM_RPMEIU
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
	N_("Install/Upgrade/Erase options:"),
	NULL },
#endif	/* IAM_RPMEIU */

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmAllPoptTable, 0,
	N_("Common options for all rpm modes:"),
	NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

#ifdef __MINT__
/* MiNT cannot dynamically increase the stack.  */
long _stksize = 64 * 1024L;
#endif

/*@exits@*/ static void argerror(const char * desc)
	/*@modifies fileSystem @*/
{
    fprintf(stderr, _("%s: %s\n"), __progname, desc);
    exit(EXIT_FAILURE);
}

static void printVersion(void)
	/*@modifies fileSystem @*/
{
    fprintf(stdout, _("RPM version %s\n"), rpmEVR);
}

static void printBanner(void)
	/*@modifies fileSystem @*/
{
    (void) puts(_("Copyright (C) 1998-2000 - Red Hat, Inc."));
    (void) puts(_("This program may be freely redistributed under the terms of the GNU GPL"));
}

static void printUsage(void)
	/*@modifies fileSystem @*/
{
    FILE * fp = stdout;
    printVersion();
    printBanner();
    (void) puts("");

    fprintf(fp, _("Usage: %s {--help}\n"), __progname);
    fprintf(fp,   "       %s {--version}\n" , __progname);

#ifdef	IAM_RPMEIU
#ifdef	DYING
--dbpath	all
--ftpproxy etc	all
--force		alias for --replacepkgs --replacefiles
--includedocs	handle as option in table
		--erase forbids many options
#endif	/* DYING */
#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMQV
#ifdef	DYING	/* XXX popt glue needing --help doco. */
--dbpath	all
--ftpproxy etc	all
-i,--info	Q
-R,--requires	Q
-P,--provides	Q
--scripts	Q
--triggeredby	Q
--changelog	Q
--triggers	Q
--querytags	!V
--setperms	V
--setugids	V
#endif	/* DYING */
#endif	/* IAM_RPMQV */

}

int main(int argc, const char ** argv)
{
    enum modes bigMode = MODE_UNKNOWN;

#ifdef	IAM_RPMQV
    QVA_t qva = &rpmQVArgs;
#endif

#ifdef	IAM_RPMBT
    BTA_t ba = &rpmBTArgs;
#endif

#ifdef	IAM_RPMEIU
   struct rpmInstallArguments_s * ia = &rpmIArgs;
#endif

#if defined(IAM_RPMDB)
   struct rpmDatabaseArguments_s * da = &rpmDBArgs;
#endif

#if defined(IAM_RPMK)
   struct rpmSignArguments_s * ka = &rpmKArgs;
#endif

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
    char * passPhrase = "";
#endif

    int arg;
    int gotDbpath = 0;

    const char * optArg;
    pid_t pipeChild = 0;
    poptContext optCon;
    int ec = 0;
    int status;
    int p[2];
	
#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();	/* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);	/* Retrofit glibc __progname */

    /* XXX glibc churn sanity */
    if (__progname == NULL) {
	if ((__progname = strrchr(argv[0], '/')) != NULL) __progname++;
	else __progname = argv[0];
    }

    /* Set the major mode based on argv[0] */
    /*@-nullpass@*/
#ifdef	IAM_RPMBT
    if (!strcmp(__progname, "rpmb"))	bigMode = MODE_BUILD;
    if (!strcmp(__progname, "rpmt"))	bigMode = MODE_TARBUILD;
    if (!strcmp(__progname, "rpmbuild"))	bigMode = MODE_BUILD;
#endif
#ifdef	IAM_RPMQV
    if (!strcmp(__progname, "rpmq"))	bigMode = MODE_QUERY;
    if (!strcmp(__progname, "rpmv"))	bigMode = MODE_VERIFY;
    if (!strcmp(__progname, "rpmquery"))	bigMode = MODE_QUERY;
    if (!strcmp(__progname, "rpmverify"))	bigMode = MODE_VERIFY;
#endif
#ifdef	RPMEIU
    if (!strcmp(__progname, "rpme"))	bigMode = MODE_ERASE;
    if (!strcmp(__progname, "rpmi"))	bigMode = MODE_INSTALL;
    if (!strcmp(__progname, "rpmu"))	bigMode = MODE_INSTALL;
#endif
    /*@=nullpass@*/

    /* set the defaults for the various command line options */
    _ftp_debug = 0;

#if HAVE_LIBIO_H && defined(_G_IO_IO_FILE_VERSION)
    noLibio = 0;
#else
    noLibio = 1;
#endif
    _rpmio_debug = 0;
    _url_debug = 0;

    /* XXX Eliminate query linkage loop */
    specedit = 0;
    parseSpecVec = parseSpec;
    freeSpecVec = freeSpec;

    /* set up the correct locale */
    (void) setlocale(LC_ALL, "" );

#ifdef	__LCLINT__
#define	LOCALEDIR	"/usr/share/locale"
#endif
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    rpmSetVerbosity(RPMMESS_NORMAL);	/* XXX silly use by showrc */

    /* Make a first pass through the arguments, looking for --rcfile */
    /* We need to handle that before dealing with the rest of the arguments. */
    /*@-nullpass -temptrans@*/
    optCon = poptGetContext(__progname, argc, argv, optionsTable, 0);
    /*@=nullpass =temptrans@*/
    (void) poptReadConfigFile(optCon, LIBRPMALIAS_FILENAME);
    (void) poptReadDefaultConfig(optCon, 1);
    poptSetExecPath(optCon, RPMCONFIGDIR, 1);

    /* reading rcfile early makes it easy to override */
    /* XXX only --rcfile (and --showrc) need this pre-parse */

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	switch(arg) {
	case 'v':
	    rpmIncreaseVerbosity();	/* XXX silly use by showrc */
	    break;
        default:
	    break;
      }
    }

    if (rpmReadConfigFiles(rcfile, NULL))  
	exit(EXIT_FAILURE);

    if (showrc) {
	(void) rpmShowRC(stdout);
	exit(EXIT_SUCCESS);
    }

    rpmSetVerbosity(RPMMESS_NORMAL);	/* XXX silly use by showrc */

    poptResetContext(optCon);

#ifdef	IAM_RPMQV
    qva->qva_queryFormat = _free(qva->qva_queryFormat);
    memset(qva, 0, sizeof(*qva));
    qva->qva_source = RPMQV_PACKAGE;
    qva->qva_fflags = RPMFILE_ALL;
    qva->qva_mode = ' ';
    qva->qva_char = ' ';
#endif

#ifdef	IAM_RPMBT
    ba->buildRootOverride = _free(ba->buildRootOverride);
    ba->targets = _free(ba->targets);
    memset(ba, 0, sizeof(*ba));
    ba->buildMode = ' ';
    ba->buildChar = ' ';
#endif

#ifdef	IAM_RPMDB
    memset(da, 0, sizeof(*da));
#endif

#ifdef	IAM_RPMK
    memset(ka, 0, sizeof(*ka));
    ka->addSign = RESIGN_NONE;
    ka->checksigFlags = CHECKSIG_ALL;
#endif

#ifdef	IAM_RPMEIU
    ia->relocations = _free(ia->relocations);
    memset(ia, 0, sizeof(*ia));
    ia->transFlags = RPMTRANS_FLAG_NONE;
    ia->probFilter = RPMPROB_FILTER_NONE;
    ia->installInterfaceFlags = INSTALL_NONE;
    ia->eraseInterfaceFlags = UNINSTALL_NONE;
#endif

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	switch (arg) {
	    
	case 'v':
	    rpmIncreaseVerbosity();
	    break;

/* XXX options used in multiple rpm modes */
#if defined(IAM_RPMQV) || defined(IAM_RPMK)
	case POPT_NOMD5:
#ifdef	IAM_RPMQV
	    if (bigMode == MODE_VERIFY || qva->qva_mode == 'V')
		qva->qva_flags |= VERIFY_MD5;
	    else
#endif
#ifdef	IAM_RPMK
	    if (bigMode & MODES_K)
		ka->checksigFlags &= ~CHECKSIG_MD5;
	    else
#endif
		/*@-ifempty@*/ ;
	    break;
#endif	/* IAM_RPMQV || IAM_RPMK */

#if defined(IAM_RPMQV) || defined(IAM_RPMEIU) || defined(IAM_RPMBT)
	case POPT_NODEPS:
#ifdef	IAM_RPMQV
	    if (bigMode == MODE_VERIFY || qva->qva_mode == 'V')
		qva->qva_flags |= VERIFY_DEPS;
	    else
#endif
#ifdef	IAM_RPMEIU
	    if ((bigMode & MODES_IE) ||
		(ia->installInterfaceFlags &
	    (INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL|INSTALL_ERASE)))
		ia->noDeps = 1;
	    else
#endif
#ifdef	IAM_RPMBT
	    if ((bigMode & MODES_BT) || ba->buildMode != ' ')
		ba->noDeps = 1;
	    else
#endif
		/*@-ifempty@*/ ;
	    break;

	case POPT_FORCE:
#ifdef	IAM_RPMEIU
	    if ((bigMode & MODES_IE) ||
		(ia->installInterfaceFlags &
	    (INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL|INSTALL_ERASE)))
		ia->probFilter |=
			( RPMPROB_FILTER_REPLACEPKG
			| RPMPROB_FILTER_REPLACEOLDFILES
			| RPMPROB_FILTER_REPLACENEWFILES
			| RPMPROB_FILTER_OLDPACKAGE);
	    else
#endif
#ifdef	IAM_RPMBT
	    if ((bigMode & MODES_BT) || ba->buildMode != ' ')
		ba->force = 1;
	    else
#endif
		/*@-ifempty@*/ ;
	    break;

	case 'i':
#ifdef	IAM_RPMQV
	    if (bigMode == MODE_QUERY || qva->qva_mode == 'q') {
		/*@-nullassign -readonlytrans@*/
		const char * infoCommand[] = { "--info", NULL };
		/*@=nullassign =readonlytrans@*/
		(void) poptStuffArgs(optCon, infoCommand);
	    } else
#endif
#ifdef	IAM_RPMEIU
	    if (bigMode == MODE_INSTALL ||
		(ia->installInterfaceFlags &
		    (INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL)))
		/*@-ifempty@*/ ;
	    else if (bigMode == MODE_UNKNOWN) {
		/*@-nullassign -readonlytrans@*/
		const char * installCommand[] = { "--install", NULL };
		/*@=nullassign =readonlytrans@*/
		(void) poptStuffArgs(optCon, installCommand);
	    } else
#endif
		/*@-ifempty@*/ ;
	    break;

	case POPT_NOSCRIPTS:
#ifdef	IAM_RPMQV
	    if (bigMode == MODE_VERIFY || qva->qva_mode == 'V')
		qva->qva_flags |= VERIFY_SCRIPT;
	    else
#endif
#ifdef	IAM_RPMEIU
	    if ((bigMode & MODES_IE) ||
		(ia->installInterfaceFlags &
	    (INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL|INSTALL_ERASE)))
		ia->transFlags |= (_noTransScripts | _noTransTriggers);
	    else
#endif
		/*@-ifempty@*/ ;
	    break;

#endif	/* IAM_RPMQV || IAM_RPMEIU || IAM_RPMBT */

	case GETOPT_DEFINEMACRO:
	    if (optArg) {
		(void) rpmDefineMacro(NULL, optArg, RMIL_CMDLINE);
		(void) rpmDefineMacro(&rpmCLIMacroContext, optArg,RMIL_CMDLINE);
	    }
	    noUsageMsg = 1;
	    break;

	case GETOPT_EVALMACRO:
	    if (optArg) {
		const char *val = rpmExpand(optArg, NULL);
		fprintf(stdout, "%s\n", val);
		val = _free(val);
	    }
	    noUsageMsg = 1;
	    break;

#if defined(GETOPT_RCFILE)
	case GETOPT_RCFILE:
	    fprintf(stderr, _("The --rcfile option has been eliminated.\n"));
	    fprintf(stderr, _("Use \"--macros <file:...>\" instead.\n"));
	    exit(EXIT_FAILURE);
	    /*@notreached@*/ break;
#endif

	default:
	    fprintf(stderr, _("Internal error in argument processing (%d) :-(\n"), arg);
	    exit(EXIT_FAILURE);
	}
    }

    if (quiet)
	rpmSetVerbosity(RPMMESS_QUIET);

    if (showVersion) printVersion();

    if (arg < -1) {
	fprintf(stderr, "%s: %s\n", 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(arg));
	exit(EXIT_FAILURE);
    }

#ifdef	IAM_RPMBT
    switch (ba->buildMode) {
    case 'b':	bigMode = MODE_BUILD;		break;
    case 't':	bigMode = MODE_TARBUILD;	break;
    case 'B':	bigMode = MODE_REBUILD;		break;
    case 'C':	bigMode = MODE_RECOMPILE;	break;
    }

    if ((ba->buildAmount & RPMBUILD_RMSOURCE) && bigMode == MODE_UNKNOWN)
	bigMode = MODE_BUILD;

    if ((ba->buildAmount & RPMBUILD_RMSPEC) && bigMode == MODE_UNKNOWN)
	bigMode = MODE_BUILD;

    if (ba->buildRootOverride && bigMode != MODE_BUILD &&
	bigMode != MODE_REBUILD && bigMode != MODE_TARBUILD) {
	argerror("--buildroot may only be used during package builds");
    }
#endif	/* IAM_RPMBT */
    
#ifdef	IAM_RPMDB
  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_DB)) {
    if (da->init) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_INITDB;
    } else
    if (da->rebuild) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_REBUILDDB;
    } else
    if (da->verify) {
	if (bigMode != MODE_UNKNOWN) 
	    argerror(_("only one major mode may be specified"));
	else
	    bigMode = MODE_VERIFYDB;
    }
  }
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMQV
  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_QV)) {
    switch (qva->qva_mode) {
    case 'q':	bigMode = MODE_QUERY;		break;
    case 'V':	bigMode = MODE_VERIFY;		break;
    case 'Q':	bigMode = MODE_QUERYTAGS;	break;
    }

    if (qva->qva_sourceCount) {
	if (qva->qva_sourceCount > 2)
	    argerror(_("one type of query/verify may be performed at a "
			"time"));
    }
    if (qva->qva_flags && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query flags"));

    if (qva->qva_queryFormat && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query format"));

    if (qva->qva_source != RPMQV_PACKAGE && (bigMode & ~MODES_QV)) 
	argerror(_("unexpected query source"));
  }
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMEIU
  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_IE))
    {	int iflags = (ia->installInterfaceFlags &
		(INSTALL_UPGRADE|INSTALL_FRESHEN|INSTALL_INSTALL));
	int eflags = (ia->installInterfaceFlags & INSTALL_ERASE);

	if (iflags & eflags)
	    argerror(_("only one major mode may be specified"));
	else if (iflags)
	    bigMode = MODE_INSTALL;
	else if (eflags)
	    bigMode = MODE_ERASE;
    }
#endif	/* IAM_RPMQV */

#ifdef	IAM_RPMK
  if (bigMode == MODE_UNKNOWN || (bigMode & MODES_K)) {
	switch (ka->addSign) {
	case RESIGN_NONE:
	    break;
	case RESIGN_CHK_SIGNATURE:
	    bigMode = MODE_CHECKSIG;
	    break;
	case RESIGN_ADD_SIGNATURE:
	case RESIGN_NEW_SIGNATURE:
	    bigMode = MODE_RESIGN;
	    break;

	}
  }
#endif	/* IAM_RPMK */

    /* XXX TODO: never happens. */
    if (gotDbpath && (bigMode & ~MODES_FOR_DBPATH))
	argerror(_("--dbpath given for operation that does not use a "
			"database"));

#if defined(IAM_RPMEIU)
    if (!( bigMode == MODE_INSTALL ) &&
(ia->probFilter & (RPMPROB_FILTER_REPLACEPKG | RPMPROB_FILTER_REPLACEOLDFILES | RPMPROB_FILTER_REPLACENEWFILES | RPMPROB_FILTER_OLDPACKAGE)))
	argerror(_("only installation, upgrading, rmsource and rmspec may be forced"));
    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_FORCERELOCATE))
	argerror(_("files may only be relocated during package installation"));

    if (ia->relocations && ia->prefix)
	argerror(_("only one of --prefix or --relocate may be used"));

    if (bigMode != MODE_INSTALL && ia->relocations)
	argerror(_("--relocate and --excludepath may only be used when installing new packages"));

    if (bigMode != MODE_INSTALL && ia->prefix)
	argerror(_("--prefix may only be used when installing new packages"));

    if (ia->prefix && ia->prefix[0] != '/') 
	argerror(_("arguments to --prefix must begin with a /"));

    if (bigMode != MODE_INSTALL && (ia->installInterfaceFlags & INSTALL_HASH))
	argerror(_("--hash (-h) may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && (ia->installInterfaceFlags & INSTALL_PERCENT))
	argerror(_("--percent may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL &&
 (ia->probFilter & (RPMPROB_FILTER_REPLACEOLDFILES|RPMPROB_FILTER_REPLACENEWFILES)))
	argerror(_("--replacefiles may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_REPLACEPKG))
	argerror(_("--replacepkgs may only be specified during package "
			"installation"));

    if (bigMode != MODE_INSTALL && (ia->transFlags & RPMTRANS_FLAG_NODOCS))
	argerror(_("--excludedocs may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && ia->incldocs)
	argerror(_("--includedocs may only be specified during package "
		   "installation"));

    if (ia->incldocs && (ia->transFlags & RPMTRANS_FLAG_NODOCS))
	argerror(_("only one of --excludedocs and --includedocs may be "
		 "specified"));
  
    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_IGNOREARCH))
	argerror(_("--ignorearch may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL && (ia->probFilter & RPMPROB_FILTER_IGNOREOS))
	argerror(_("--ignoreos may only be specified during package "
		   "installation"));

    if (bigMode != MODE_INSTALL &&
	(ia->probFilter & (RPMPROB_FILTER_DISKSPACE|RPMPROB_FILTER_DISKNODES)))
	argerror(_("--ignoresize may only be specified during package "
		   "installation"));

    if ((ia->eraseInterfaceFlags & UNINSTALL_ALLMATCHES) && bigMode != MODE_ERASE)
	argerror(_("--allmatches may only be specified during package "
		   "erasure"));

    if ((ia->transFlags & RPMTRANS_FLAG_ALLFILES) && bigMode != MODE_INSTALL)
	argerror(_("--allfiles may only be specified during package "
		   "installation"));

    if ((ia->transFlags & RPMTRANS_FLAG_JUSTDB) &&
	bigMode != MODE_INSTALL && bigMode != MODE_ERASE)
	argerror(_("--justdb may only be specified during package "
		   "installation and erasure"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE &&
	(ia->transFlags & (RPMTRANS_FLAG_NOSCRIPTS | _noTransScripts | _noTransTriggers)))
	argerror(_("script disabling options may only be specified during "
		   "package installation and erasure"));

    if (bigMode != MODE_INSTALL && bigMode != MODE_ERASE &&
	(ia->transFlags & (RPMTRANS_FLAG_NOTRIGGERS | _noTransTriggers)))
	argerror(_("trigger disabling options may only be specified during "
		   "package installation and erasure"));

    if (ia->noDeps & (bigMode & ~MODES_FOR_NODEPS))
	argerror(_("--nodeps may only be specified during package "
		   "building, rebuilding, recompilation, installation,"
		   "erasure, and verification"));

    if ((ia->transFlags & RPMTRANS_FLAG_TEST) && (bigMode & ~MODES_FOR_TEST))
	argerror(_("--test may only be specified during package installation, "
		 "erasure, and building"));
#endif	/* IAM_RPMEIU */

    if (rootdir && rootdir[1] && (bigMode & ~MODES_FOR_ROOT))
	argerror(_("--root (-r) may only be specified during "
		 "installation, erasure, querying, and "
		 "database rebuilds"));

    if (rootdir) {
	switch (urlIsURL(rootdir)) {
	default:
	    if (bigMode & MODES_FOR_ROOT)
		break;
	    /*@fallthrough@*/
	case URL_IS_UNKNOWN:
	    if (rootdir[0] != '/')
		argerror(_("arguments to --root (-r) must begin with a /"));
	    break;
	}
    }

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
    if (0
#if defined(IAM_RPMBT)
    || ba->sign 
#endif
#if defined(IAM_RPMK)
    || ka->sign
#endif
    ) {
        if (bigMode == MODE_REBUILD || bigMode == MODE_BUILD ||
	    bigMode == MODE_RESIGN || bigMode == MODE_TARBUILD) {
	    const char ** av;
	    struct stat sb;
	    int errors = 0;

	    if ((av = poptGetArgs(optCon)) == NULL) {
		fprintf(stderr, _("no files to sign\n"));
		errors++;
	    } else
	    while (*av) {
		if (stat(*av, &sb)) {
		    fprintf(stderr, _("cannot access file %s\n"), *av);
		    errors++;
		}
		av++;
	    }

	    if (errors) {
		ec = errors;
		goto exit;
	    }

            if (poptPeekArg(optCon)) {
		int sigTag;
		switch (sigTag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) {
		  case 0:
		    break;
		  case RPMSIGTAG_PGP:
		    if ((sigTag == RPMSIGTAG_PGP || sigTag == RPMSIGTAG_PGP5) &&
		        !rpmDetectPGPVersion(NULL)) {
		        fprintf(stderr, _("pgp not found: "));
			ec = EXIT_FAILURE;
			goto exit;
		    }	/*@fallthrough@*/
		  case RPMSIGTAG_GPG:
		    passPhrase = rpmGetPassPhrase(_("Enter pass phrase: "), sigTag);
		    if (passPhrase == NULL) {
			fprintf(stderr, _("Pass phrase check failed\n"));
			ec = EXIT_FAILURE;
			goto exit;
		    }
		    fprintf(stderr, _("Pass phrase is good.\n"));
		    passPhrase = xstrdup(passPhrase);
		    break;
		  default:
		    fprintf(stderr,
		            _("Invalid %%_signature spec in macro file.\n"));
		    ec = EXIT_FAILURE;
		    goto exit;
		    /*@notreached@*/ break;
		}
	    }
	} else {
	    argerror(_("--sign may only be used during package building"));
	}
    } else {
    	/* Make rpmLookupSignatureType() return 0 ("none") from now on */
        (void) rpmLookupSignatureType(RPMLOOKUPSIG_DISABLE);
    }
#endif	/* IAM_RPMBT || IAM_RPMK */

    if (pipeOutput) {
	(void) pipe(p);

	if (!(pipeChild = fork())) {
	    (void) close(p[1]);
	    (void) dup2(p[0], STDIN_FILENO);
	    (void) close(p[0]);
	    (void) execl("/bin/sh", "/bin/sh", "-c", pipeOutput, NULL);
	    fprintf(stderr, _("exec failed\n"));
	}

	(void) close(p[0]);
	(void) dup2(p[1], STDOUT_FILENO);
	(void) close(p[1]);
    }
	
    switch (bigMode) {
#ifdef	IAM_RPMDB
    case MODE_INITDB:
	(void) rpmdbInit(rootdir, 0644);
	break;

    case MODE_REBUILDDB:
	ec = rpmdbRebuild(rootdir);
	break;
    case MODE_VERIFYDB:
	ec = rpmdbVerify(rootdir);
	break;
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMBT
    case MODE_REBUILD:
    case MODE_RECOMPILE:
      { const char * pkg;
        while (!rpmIsVerbose())
	    rpmIncreaseVerbosity();

	if (!poptPeekArg(optCon))
	    argerror(_("no packages files given for rebuild"));

	ba->buildAmount = RPMBUILD_PREP | RPMBUILD_BUILD | RPMBUILD_INSTALL;
	if (bigMode == MODE_REBUILD) {
	    ba->buildAmount |= RPMBUILD_PACKAGEBINARY;
	    ba->buildAmount |= RPMBUILD_RMSOURCE;
	    ba->buildAmount |= RPMBUILD_RMSPEC;
	    ba->buildAmount |= RPMBUILD_CLEAN;
	    ba->buildAmount |= RPMBUILD_RMBUILD;
	}

	while ((pkg = poptGetArg(optCon))) {
	    const char * specFile = NULL;
	    char * cookie = NULL;

	    ec = rpmInstallSource("", pkg, &specFile, &cookie);
	    if (ec)
		/*@loopbreak@*/ break;

	    ba->rootdir = rootdir;
	    ec = build(specFile, ba, passPhrase, cookie, rcfile);
	    free(cookie);
	    cookie = NULL;
	    free((void *)specFile);
	    specFile = NULL;

	    if (ec)
		/*@loopbreak@*/ break;
	}
      }	break;

      case MODE_BUILD:
      case MODE_TARBUILD:
      { const char * pkg;
        while (!rpmIsVerbose())
	    rpmIncreaseVerbosity();
       
	switch (ba->buildChar) {
	case 'a':
	    ba->buildAmount |= RPMBUILD_PACKAGESOURCE;
	    /*@fallthrough@*/
	case 'b':
	    ba->buildAmount |= RPMBUILD_PACKAGEBINARY;
	    ba->buildAmount |= RPMBUILD_CLEAN;
	    /*@fallthrough@*/
	case 'i':
	    ba->buildAmount |= RPMBUILD_INSTALL;
	    if ((ba->buildChar == 'i') && ba->shortCircuit)
		break;
	    /*@fallthrough@*/
	case 'c':
	    ba->buildAmount |= RPMBUILD_BUILD;
	    if ((ba->buildChar == 'c') && ba->shortCircuit)
		break;
	    /*@fallthrough@*/
	case 'p':
	    ba->buildAmount |= RPMBUILD_PREP;
	    break;
	    
	case 'l':
	    ba->buildAmount |= RPMBUILD_FILECHECK;
	    break;
	case 's':
	    ba->buildAmount |= RPMBUILD_PACKAGESOURCE;
	    break;
	}

	if (!poptPeekArg(optCon)) {
	    if (bigMode == MODE_BUILD)
		argerror(_("no spec files given for build"));
	    else
		argerror(_("no tar files given for build"));
	}

	while ((pkg = poptGetArg(optCon))) {
	    ba->rootdir = rootdir;
	    ec = build(pkg, ba, passPhrase, NULL, rcfile);
	    if (ec)
		/*@loopbreak@*/ break;
	    rpmFreeMacros(NULL);
	    (void) rpmReadConfigFiles(rcfile, NULL);
	}
      }	break;
#endif	/* IAM_RPMBT */

#ifdef	IAM_RPMEIU
    case MODE_ERASE:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for uninstall"));

	if (ia->noDeps) ia->eraseInterfaceFlags |= UNINSTALL_NODEPS;

	ec = rpmErase(rootdir, (const char **)poptGetArgs(optCon), 
			 ia->transFlags, ia->eraseInterfaceFlags);
	break;

    case MODE_INSTALL:

	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for install"));

	/* RPMTRANS_FLAG_BUILD_PROBS */
	/* RPMTRANS_FLAG_KEEPOBSOLETE */

	if (!ia->incldocs) {
	    if (ia->transFlags & RPMTRANS_FLAG_NODOCS)
		;
	    else if (rpmExpandNumeric("%{_excludedocs}"))
		ia->transFlags |= RPMTRANS_FLAG_NODOCS;
	}

	if (ia->noDeps) ia->installInterfaceFlags |= INSTALL_NODEPS;

	/* we've already ensured !(!ia->prefix && !ia->relocations) */
	if (ia->prefix) {
	    ia->relocations = xmalloc(2 * sizeof(*ia->relocations));
	    ia->relocations[0].oldPath = NULL;   /* special case magic */
	    ia->relocations[0].newPath = ia->prefix;
	    ia->relocations[1].oldPath = NULL;
	    ia->relocations[1].newPath = NULL;
	} else if (ia->relocations) {
	    ia->relocations = xrealloc(ia->relocations, 
			sizeof(*ia->relocations) * (ia->numRelocations + 1));
	    ia->relocations[ia->numRelocations].oldPath = NULL;
	    ia->relocations[ia->numRelocations].newPath = NULL;
	}

	/*@-compdef@*/ /* FIX: ia->relocations[0].newPath undefined */
	ec += rpmInstall(rootdir, (const char **)poptGetArgs(optCon), 
			ia->transFlags, ia->installInterfaceFlags, ia->probFilter,
			ia->relocations);
	/*@=compdef@*/
	break;

    case MODE_ROLLBACK:
	ia->rootdir = rootdir;
	ec += rpmRollback(ia, (const char **)poptGetArgs(optCon));
	break;

#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMQV
    case MODE_QUERY:
      { const char * pkg;

	qva->qva_prefix = rootdir;
	if (qva->qva_source == RPMQV_ALL) {
#ifdef	DYING
	    if (poptPeekArg(optCon))
		argerror(_("extra arguments given for query of all packages"));
#else
	    const char ** av = poptGetArgs(optCon);
#endif
	    /*@-nullpass@*/	/* FIX: av can be NULL */
	    ec = rpmQuery(qva, RPMQV_ALL, (const char *) av);
	    /*@=nullpass@*/
	} else {
	    if (!poptPeekArg(optCon))
		argerror(_("no arguments given for query"));
	    while ((pkg = poptGetArg(optCon)))
		ec += rpmQuery(qva, qva->qva_source, pkg);
	}
      }	break;

    case MODE_VERIFY:
      { const char * pkg;
	rpmVerifyFlags verifyFlags = VERIFY_ALL;

	verifyFlags &= ~qva->qva_flags;
	qva->qva_flags = (rpmQueryFlags) verifyFlags;
	qva->qva_prefix = rootdir;

	if (qva->qva_source == RPMQV_ALL) {
	    if (poptPeekArg(optCon))
		argerror(_("extra arguments given for verify of all packages"));
	    ec = rpmVerify(qva, RPMQV_ALL, NULL);
	} else {
	    if (!poptPeekArg(optCon))
		argerror(_("no arguments given for verify"));
	    while ((pkg = poptGetArg(optCon)))
		ec += rpmVerify(qva, qva->qva_source, pkg);
	}
      }	break;

    case MODE_QUERYTAGS:
	if (argc != 2)
	    argerror(_("unexpected arguments to --querytags "));

	rpmDisplayQueryTags(stdout);
	break;
#endif	/* IAM_RPMQV */

#ifdef IAM_RPMK
    case MODE_CHECKSIG:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for signature check"));
	ec = rpmCheckSig(ka->checksigFlags,
			(const char **)poptGetArgs(optCon));
	/* XXX don't overflow single byte exit status */
	if (ec > 255) ec = 255;
	break;

    case MODE_RESIGN:
	if (!poptPeekArg(optCon))
	    argerror(_("no packages given for signing"));
	ec = rpmReSign(ka->addSign, passPhrase,
			(const char **)poptGetArgs(optCon));
	/* XXX don't overflow single byte exit status */
	if (ec > 255) ec = 255;
	break;
#endif	/* IAM_RPMK */
	
#if !defined(IAM_RPMQV)
    case MODE_QUERY:
    case MODE_VERIFY:
    case MODE_QUERYTAGS:
#endif
#if !defined(IAM_RPMK)
    case MODE_CHECKSIG:
    case MODE_RESIGN:
#endif
#if !defined(IAM_RPMDB)
    case MODE_INITDB:
    case MODE_REBUILDDB:
    case MODE_VERIFYDB:
#endif
#if !defined(IAM_RPMBT)
    case MODE_BUILD:
    case MODE_REBUILD:
    case MODE_RECOMPILE:
    case MODE_TARBUILD:
#endif
#if !defined(IAM_RPMEIU)
    case MODE_INSTALL:
    case MODE_ERASE:
    case MODE_ROLLBACK:
#endif
    case MODE_UNKNOWN:
	if (!showVersion && !help && !noUsageMsg) printUsage();
	break;
    }

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
exit:
#endif	/* IAM_RPMBT || IAM_RPMK */
    optCon = poptFreeContext(optCon);
    rpmFreeMacros(NULL);
    rpmFreeMacros(&rpmCLIMacroContext);
    rpmFreeRpmrc();

    if (pipeChild) {
	(void) fclose(stdout);
	(void) waitpid(pipeChild, &status, 0);
    }

    /* keeps memory leak checkers quiet */
    freeNames();
    freeFilesystems();
    urlFreeCache();
    rpmlogClose();
    dbiTags = _free(dbiTags);

#ifdef	IAM_RPMQV
    qva->qva_queryFormat = _free(qva->qva_queryFormat);
#endif

#ifdef	IAM_RPMBT
    ba->buildRootOverride = _free(ba->buildRootOverride);
    ba->targets = _free(ba->targets);
#endif

#ifdef	IAM_RPMEIU
    ia->relocations = _free(ia->relocations);
#endif

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    /*@-globstate@*/
    return ec;
    /*@=globstate@*/
}
