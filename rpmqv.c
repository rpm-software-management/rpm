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

#include "rpmdb.h"
#include "rpmps.h"
#include "rpmts.h"

#ifdef	IAM_RPMBT
#include "build.h"
#define GETOPT_REBUILD		1003
#define GETOPT_RECOMPILE	1004
#endif

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
#include "signature.h"
#endif

#include "debug.h"

enum modes {

    MODE_QUERY		= (1 <<  0),
    MODE_VERIFY		= (1 <<  3),
    MODE_QUERYTAGS	= (1 <<  9),
#define	MODES_QV (MODE_QUERY | MODE_VERIFY)

    MODE_INSTALL	= (1 <<  1),
    MODE_ERASE		= (1 <<  2),
#define	MODES_IE (MODE_INSTALL | MODE_ERASE)

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
#define	MODES_FOR_ROOT		(MODES_BT | MODES_IE | MODES_QV | MODES_DB | MODES_K)

/* the structure describing the options we take and the defaults */
/*@unchecked@*/
static struct poptOption optionsTable[] = {

 /* XXX colliding options */
#if defined(IAM_RPMQV) || defined(IAM_RPMEIU)
 {  NULL, 'i', POPT_ARGFLAG_DOC_HIDDEN, 0, 'i',			NULL, NULL},
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

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
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
	/*@globals __assert_program_name, fileSystem @*/
	/*@modifies fileSystem @*/
{
    fprintf(stderr, _("%s: %s\n"), __progname, desc);
    exit(EXIT_FAILURE);
}

static void printVersion(FILE * fp)
	/*@globals rpmEVR, fileSystem @*/
	/*@modifies fileSystem @*/
{
    fprintf(fp, _("RPM version %s\n"), rpmEVR);
}

static void printBanner(FILE * fp)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    fprintf(fp, _("Copyright (C) 1998-2002 - Red Hat, Inc.\n"));
    fprintf(fp, _("This program may be freely redistributed under the terms of the GNU GPL\n"));
}

static void printUsage(poptContext con, FILE * fp, int flags)
	/*@globals __assert_program_name, rpmEVR, fileSystem @*/
	/*@modifies fileSystem @*/
{
    printVersion(fp);
    printBanner(fp);
    fprintf(fp, "\n");

    if (rpmIsVerbose())
	poptPrintHelp(con, fp, flags);
    else
	poptPrintUsage(con, fp, flags);
}

/*@-bounds@*/ /* LCL: segfault */
/*@-mods@*/ /* FIX: shrug */
#if !defined(__GLIBC__) && !defined(__LCLINT__)
int main(int argc, const char ** argv, /*@unused@*/ char ** envp)
#else
int main(int argc, const char ** argv)
#endif
	/*@globals __assert_program_name, rpmEVR, RPMVERSION,
		rpmGlobalMacroContext, rpmCLIMacroContext,
		fileSystem, internalState@*/
	/*@modifies __assert_program_name,
		fileSystem, internalState@*/
{
    rpmts ts = NULL;
    enum modes bigMode = MODE_UNKNOWN;

#ifdef	IAM_RPMQV
    QVA_t qva = &rpmQVKArgs;
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
   QVA_t ka = &rpmQVKArgs;
#endif

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
    char * passPhrase = "";
#endif

    int arg;

    const char * optArg;
    pid_t pipeChild = 0;
    poptContext optCon;
    int ec = 0;
    int status;
    int p[2];
#ifdef	IAM_RPMEIU
    int i;
#endif
	
#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-noeffect@*/
    mtrace();	/* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif
    setprogname(argv[0]);	/* Retrofit glibc __progname */

#if !defined(__GLIBC__) && !defined(__LCLINT__)
    environ = envp;
#endif  

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

    /* XXX Eliminate query linkage loop */
    /*@-type@*/	/* FIX: casts? */
    parseSpecVec = parseSpec;
    freeSpecVec = freeSpec;
    /*@=type@*/

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

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	switch (arg) {
	    
/* XXX options used in multiple rpm modes */

#if defined(IAM_RPMQV) || defined(IAM_RPMEIU)
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
	    /*@switchbreak@*/ break;
#endif	/* IAM_RPMQV || IAM_RPMEIU */

	default:
	    fprintf(stderr, _("Internal error in argument processing (%d) :-(\n"), arg);
	    exit(EXIT_FAILURE);
	}
    }

    if (arg < -1) {
	fprintf(stderr, "%s: %s\n", 
		poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
		poptStrerror(arg));
	exit(EXIT_FAILURE);
    }

    rpmcliConfigured();

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
	switch (ka->qva_mode) {
	case RPMSIGN_NONE:
	    ka->sign = 0;
	    break;
	case RPMSIGN_IMPORT_PUBKEY:
	case RPMSIGN_CHK_SIGNATURE:
	    bigMode = MODE_CHECKSIG;
	    ka->sign = 0;
	    break;
	case RPMSIGN_ADD_SIGNATURE:
	case RPMSIGN_NEW_SIGNATURE:
	    bigMode = MODE_RESIGN;
	    ka->sign = 1;
	    break;
	}
  }
#endif	/* IAM_RPMK */

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

    if (rpmcliRootDir && rpmcliRootDir[1] && (bigMode & ~MODES_FOR_ROOT))
	argerror(_("--root (-r) may only be specified during "
		 "installation, erasure, querying, and "
		 "database rebuilds"));

    if (rpmcliRootDir) {
	switch (urlIsURL(rpmcliRootDir)) {
	default:
	    if (bigMode & MODES_FOR_ROOT)
		break;
	    /*@fallthrough@*/
	case URL_IS_UNKNOWN:
	    if (rpmcliRootDir[0] != '/')
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
    )
    /*@-branchstate@*/
    {
        if (bigMode == MODE_REBUILD || bigMode == MODE_BUILD ||
	    bigMode == MODE_RESIGN || bigMode == MODE_TARBUILD)
	{
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
    /*@=branchstate@*/
#endif	/* IAM_RPMBT || IAM_RPMK */

    if (rpmcliPipeOutput) {
	(void) pipe(p);

	if (!(pipeChild = fork())) {
	    (void) close(p[1]);
	    (void) dup2(p[0], STDIN_FILENO);
	    (void) close(p[0]);
	    (void) execl("/bin/sh", "/bin/sh", "-c", rpmcliPipeOutput, NULL);
	    fprintf(stderr, _("exec failed\n"));
	}

	(void) close(p[0]);
	(void) dup2(p[1], STDOUT_FILENO);
	(void) close(p[1]);
    }
	
    ts = rpmtsCreate();
    (void) rpmtsSetRootDir(ts, rpmcliRootDir);
    switch (bigMode) {
#ifdef	IAM_RPMDB
    case MODE_INITDB:
	(void) rpmtsInitDB(ts, 0644);
	break;

    case MODE_REBUILDDB:
    {   int vsflags = rpmExpandNumeric("%{_vsflags_rebuilddb}");
	(void)rpmtsSetVerifySigFlags(ts, (vsflags & ~_RPMTS_VSF_VERIFY_LEGACY));
	ec = rpmtsRebuildDB(ts);
    }	break;
    case MODE_VERIFYDB:
	ec = rpmtsVerifyDB(ts);
	break;
#endif	/* IAM_RPMDB */

#ifdef	IAM_RPMBT
    case MODE_REBUILD:
    case MODE_RECOMPILE:
    {	const char * pkg;

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

	    ba->cookie = NULL;
	    ec = rpmInstallSource(ts, pkg, &specFile, &ba->cookie);
	    if (ec == 0) {
		ba->rootdir = rpmcliRootDir;
		ba->passPhrase = passPhrase;
		ec = build(ts, specFile, ba, rpmcliRcfile);
	    }
	    ba->cookie = _free(ba->cookie);
	    specFile = _free(specFile);

	    if (ec)
		/*@loopbreak@*/ break;
	}

    }	break;

    case MODE_BUILD:
    case MODE_TARBUILD:
    {	const char * pkg;
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
		/*@innerbreak@*/ break;
	    /*@fallthrough@*/
	case 'c':
	    ba->buildAmount |= RPMBUILD_BUILD;
	    if ((ba->buildChar == 'c') && ba->shortCircuit)
		/*@innerbreak@*/ break;
	    /*@fallthrough@*/
	case 'p':
	    ba->buildAmount |= RPMBUILD_PREP;
	    /*@innerbreak@*/ break;
	    
	case 'l':
	    ba->buildAmount |= RPMBUILD_FILECHECK;
	    /*@innerbreak@*/ break;
	case 's':
	    ba->buildAmount |= RPMBUILD_PACKAGESOURCE;
	    /*@innerbreak@*/ break;
	}

	if (!poptPeekArg(optCon)) {
	    if (bigMode == MODE_BUILD)
		argerror(_("no spec files given for build"));
	    else
		argerror(_("no tar files given for build"));
	}

	while ((pkg = poptGetArg(optCon))) {
	    ba->rootdir = rpmcliRootDir;
	    ba->passPhrase = passPhrase;
	    ba->cookie = NULL;
	    ec = build(ts, pkg, ba, rpmcliRcfile);
	    if (ec)
		/*@loopbreak@*/ break;
	    rpmFreeMacros(NULL);
	    (void) rpmReadConfigFiles(rpmcliRcfile, NULL);
	}
    }	break;
#endif	/* IAM_RPMBT */

#ifdef	IAM_RPMEIU
    case MODE_ERASE:
	if (ia->noDeps) ia->eraseInterfaceFlags |= UNINSTALL_NODEPS;

	if (!poptPeekArg(optCon)) {
	    if (ia->rbtid == 0)
		argerror(_("no packages given for erase"));
ia->transFlags |= RPMTRANS_FLAG_NOMD5;
ia->probFilter |= RPMPROB_FILTER_OLDPACKAGE;
	    ec += rpmRollback(ts, ia, NULL);
	} else {
	    ec += rpmErase(ts, ia, (const char **) poptGetArgs(optCon));
	}
	break;

    case MODE_INSTALL:

	/* RPMTRANS_FLAG_KEEPOBSOLETE */

	if (!ia->incldocs) {
	    if (ia->transFlags & RPMTRANS_FLAG_NODOCS)
		;
	    else if (rpmExpandNumeric("%{_excludedocs}"))
		ia->transFlags |= RPMTRANS_FLAG_NODOCS;
	}

	if (ia->noDeps) ia->installInterfaceFlags |= INSTALL_NODEPS;

	/* we've already ensured !(!ia->prefix && !ia->relocations) */
	/*@-branchstate@*/
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
	/*@=branchstate@*/

	if (!poptPeekArg(optCon)) {
	    if (ia->rbtid == 0)
		argerror(_("no packages given for install"));
ia->transFlags |= RPMTRANS_FLAG_NOMD5;
ia->probFilter |= RPMPROB_FILTER_OLDPACKAGE;
/*@i@*/	    ec += rpmRollback(ts, ia, NULL);
	} else {
	    /*@-compmempass@*/ /* FIX: ia->relocations[0].newPath undefined */
	    ec += rpmInstall(ts, ia, (const char **)poptGetArgs(optCon));
	    /*@=compmempass@*/
	}
	break;

#endif	/* IAM_RPMEIU */

#ifdef	IAM_RPMQV
    case MODE_QUERY:
	if (qva->qva_source != RPMQV_ALL && !poptPeekArg(optCon))
	    argerror(_("no arguments given for query"));
	ec = rpmcliQuery(ts, qva, (const char **) poptGetArgs(optCon));
	/* XXX don't overflow single byte exit status */
	if (ec > 255) ec = 255;
	break;

    case MODE_VERIFY:
    {	rpmVerifyFlags verifyFlags = VERIFY_ALL;

	verifyFlags &= ~qva->qva_flags;
	qva->qva_flags = (rpmQueryFlags) verifyFlags;

	if (qva->qva_source != RPMQV_ALL && !poptPeekArg(optCon))
	    argerror(_("no arguments given for verify"));
	ec = rpmcliVerify(ts, qva, (const char **) poptGetArgs(optCon));
	/* XXX don't overflow single byte exit status */
	if (ec > 255) ec = 255;
    }	break;

    case MODE_QUERYTAGS:
	if (argc != 2)
	    argerror(_("unexpected arguments to --querytags "));

	rpmDisplayQueryTags(stdout);
	break;
#endif	/* IAM_RPMQV */

#ifdef IAM_RPMK
    case MODE_CHECKSIG:
    {	rpmVerifyFlags verifyFlags =
		(VERIFY_MD5|VERIFY_DIGEST|VERIFY_SIGNATURE);

	verifyFlags &= ~ka->qva_flags;
	ka->qva_flags = (rpmQueryFlags) verifyFlags;
    }   /*@fallthrough@*/
    case MODE_RESIGN:
	if (!poptPeekArg(optCon))
	    argerror(_("no arguments given"));
	ka->passPhrase = passPhrase;
	ec = rpmcliSign(ts, ka, (const char **)poptGetArgs(optCon));
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
#endif
    case MODE_UNKNOWN:
	if (poptPeekArg(optCon) != NULL || argc <= 1 || rpmIsVerbose())
	    printUsage(optCon, stdout, 0);
	break;
    }

#if defined(IAM_RPMBT) || defined(IAM_RPMK)
exit:
#endif	/* IAM_RPMBT || IAM_RPMK */

    ts = rpmtsFree(ts);

    optCon = poptFreeContext(optCon);
    rpmFreeMacros(NULL);
/*@i@*/	rpmFreeMacros(rpmCLIMacroContext);
    rpmFreeRpmrc();

    if (pipeChild) {
	(void) fclose(stdout);
	(void) waitpid(pipeChild, &status, 0);
    }

    /* keeps memory leak checkers quiet */
    freeNames();
    freeFilesystems();
/*@i@*/	urlFreeCache();
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
    if (ia->relocations != NULL)
    for (i = 0; i < ia->numRelocations; i++)
	ia->relocations[i].oldPath = _free(ia->relocations[i].oldPath);
    ia->relocations = _free(ia->relocations);
#endif

#if HAVE_MCHECK_H && HAVE_MTRACE
    /*@-noeffect@*/
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
    /*@=noeffect@*/
#endif
    /*@-globstate@*/
    return ec;
    /*@=globstate@*/
}
/*@=mods@*/
/*@=bounds@*/
