#include "system.h"
const char *__progname;

#define	_AUTOHELP

#include <sys/wait.h>
#if HAVE_MCHECK_H
#include <mcheck.h>
#endif

#include <rpm/rpmcli.h>
#include <rpm/rpmlib.h>			/* RPMSIGTAG, rpmReadPackageFile .. */
#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmps.h>
#include <rpm/rpmts.h>

#ifdef	IAM_RPMBT
#include "build.h"
#define GETOPT_REBUILD		1003
#define GETOPT_RECOMPILE	1004
#endif

#if defined(IAM_RPMBT)
#include "lib/signature.h"
#endif

#include "debug.h"

enum modes {
    MODE_BUILD		= (1 <<  4),
    MODE_REBUILD	= (1 <<  5),
    MODE_RECOMPILE	= (1 <<  8),
    MODE_TARBUILD	= (1 << 11),
#define	MODES_BT (MODE_BUILD | MODE_TARBUILD | MODE_REBUILD | MODE_RECOMPILE)

    MODE_UNKNOWN	= 0
};

#define	MODES_FOR_DBPATH	(MODES_BT)
#define	MODES_FOR_NODEPS	(MODES_BT)
#define	MODES_FOR_TEST		(MODES_BT)
#define	MODES_FOR_ROOT		(MODES_BT)

static int quiet;

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {

#ifdef	IAM_RPMBT
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmBuildPoptTable, 0,
	N_("Build options with [ <specfile> | <tarball> | <source package> ]:"),
	NULL },
#endif	/* IAM_RPMBT */

 { "quiet", '\0', 0, &quiet, 0,			NULL, NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

#ifdef __MINT__
/* MiNT cannot dynamically increase the stack.  */
long _stksize = 64 * 1024L;
#endif

RPM_GNUC_NORETURN
static void argerror(const char * desc)
{
    fprintf(stderr, _("%s: %s\n"), __progname, desc);
    exit(EXIT_FAILURE);
}

static void printVersion(FILE * fp)
{
    fprintf(fp, _("RPM version %s\n"), rpmEVR);
}

static void printBanner(FILE * fp)
{
    fprintf(fp, _("Copyright (C) 1998-2002 - Red Hat, Inc.\n"));
    fprintf(fp, _("This program may be freely redistributed under the terms of the GNU GPL\n"));
}

static void printUsage(poptContext con, FILE * fp, int flags)
{
    printVersion(fp);
    printBanner(fp);
    fprintf(fp, "\n");

    if (rpmIsVerbose())
	poptPrintHelp(con, fp, flags);
    else
	poptPrintUsage(con, fp, flags);
}

int main(int argc, char *argv[])
{
    rpmts ts = NULL;
    enum modes bigMode = MODE_UNKNOWN;

#ifdef	IAM_RPMBT
    BTA_t ba = &rpmBTArgs;
#endif

#if defined(IAM_RPMBT)
    char * passPhrase = "";
#endif

    int arg;

    const char *optArg, *poptCtx;
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
#ifdef	IAM_RPMBT
    if (rstreq(__progname, "rpmbuild"))	bigMode = MODE_BUILD;
#endif

#if defined(ENABLE_NLS)
    /* set up the correct locale */
    (void) setlocale(LC_ALL, "" );

    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    rpmSetVerbosity(RPMLOG_NOTICE);	/* XXX silly use by showrc */

    /* Only build has it's own set of aliases, everything else uses rpm */
#ifdef	IAM_RPMBT
    poptCtx = "rpmbuild";
#endif

    /* Make a first pass through the arguments, looking for --rcfile */
    /* We need to handle that before dealing with the rest of the arguments. */
    /* XXX popt argv definition should be fixed instead of casting... */
    optCon = poptGetContext(poptCtx, argc, (const char **)argv, optionsTable, 0);
    {
	char *poptfile = rpmGenPath(rpmConfigDir(), LIBRPMALIAS_FILENAME, NULL);
	(void) poptReadConfigFile(optCon, poptfile);
	free(poptfile);
    }
    (void) poptReadDefaultConfig(optCon, 1);
    poptSetExecPath(optCon, rpmConfigDir(), 1);

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);

	switch (arg) {
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
    
    if (rpmcliRootDir && rpmcliRootDir[1] && (bigMode & ~MODES_FOR_ROOT))
	argerror(_("--root (-r) may only be specified during "
		 "installation, erasure, querying, and "
		 "database rebuilds"));

    if (rpmcliRootDir) {
	switch (urlIsURL(rpmcliRootDir)) {
	default:
	    if (bigMode & MODES_FOR_ROOT)
		break;
	case URL_IS_UNKNOWN:
	    if (rpmcliRootDir[0] != '/')
		argerror(_("arguments to --root (-r) must begin with a /"));
	    break;
	}
    }

    if (quiet)
	rpmSetVerbosity(RPMLOG_WARNING);

#if defined(IAM_RPMBT)
    if (ba->sign) {
        if (bigMode == MODE_REBUILD || bigMode == MODE_BUILD ||
	    bigMode == MODE_TARBUILD)
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
		int sigTag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY);
		switch (sigTag) {
		  case 0:
		    break;
		  case RPMSIGTAG_PGP:
		  case RPMSIGTAG_GPG:
		  case RPMSIGTAG_DSA:
		  case RPMSIGTAG_RSA:
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
		    break;
		}
	    }
	} else {
	    argerror(_("--sign may only be used during package building"));
	}
    } else {
    	/* Make rpmLookupSignatureType() return 0 ("none") from now on */
        (void) rpmLookupSignatureType(RPMLOOKUPSIG_DISABLE);
    }
#endif	/* IAM_RPMBT */

    if (rpmcliPipeOutput) {
	if (pipe(p) < 0) {
	    fprintf(stderr, _("creating a pipe for --pipe failed: %m\n"));
	    goto exit;
	}

	if (!(pipeChild = fork())) {
	    (void) signal(SIGPIPE, SIG_DFL);
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
#ifdef	IAM_RPMBT
    case MODE_REBUILD:
    case MODE_RECOMPILE:
    {	const char * pkg;

        while (!rpmIsVerbose())
	    rpmIncreaseVerbosity();

	if (!poptPeekArg(optCon))
	    argerror(_("no packages files given for rebuild"));

	ba->buildAmount =
	    RPMBUILD_PREP | RPMBUILD_BUILD | RPMBUILD_INSTALL | RPMBUILD_CHECK;
	if (bigMode == MODE_REBUILD) {
	    ba->buildAmount |= RPMBUILD_PACKAGEBINARY;
	    ba->buildAmount |= RPMBUILD_RMSOURCE;
	    ba->buildAmount |= RPMBUILD_RMSPEC;
	    ba->buildAmount |= RPMBUILD_CLEAN;
	    ba->buildAmount |= RPMBUILD_RMBUILD;
	}

	while ((pkg = poptGetArg(optCon))) {
	    char * specFile = NULL;

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
		break;
	}

    }	break;

    case MODE_BUILD:
    case MODE_TARBUILD:
    {	const char * pkg;
        if (!quiet) while (!rpmIsVerbose())
	    rpmIncreaseVerbosity();
       
	switch (ba->buildChar) {
	case 'a':
	    ba->buildAmount |= RPMBUILD_PACKAGESOURCE;
	case 'b':
	    ba->buildAmount |= RPMBUILD_PACKAGEBINARY;
	    ba->buildAmount |= RPMBUILD_CLEAN;
	    if ((ba->buildChar == 'b') && ba->shortCircuit)
		break;
	case 'i':
	    ba->buildAmount |= RPMBUILD_INSTALL;
	    ba->buildAmount |= RPMBUILD_CHECK;
	    if ((ba->buildChar == 'i') && ba->shortCircuit)
		break;
	case 'c':
	    ba->buildAmount |= RPMBUILD_BUILD;
	    if ((ba->buildChar == 'c') && ba->shortCircuit)
		break;
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
	    ba->rootdir = rpmcliRootDir;
	    ba->passPhrase = passPhrase;
	    ba->cookie = NULL;
	    ec = build(ts, pkg, ba, rpmcliRcfile);
	    if (ec)
		break;
	    rpmFreeMacros(NULL);
	    (void) rpmReadConfigFiles(rpmcliRcfile, NULL);
	}
    }	break;
#endif	/* IAM_RPMBT */

    case MODE_UNKNOWN:
	if (poptPeekArg(optCon) != NULL || argc <= 1 || rpmIsVerbose()) {
	    printUsage(optCon, stderr, 0);
	    ec = argc;
	}
	break;
    }

exit:

    ts = rpmtsFree(ts);

    optCon = poptFreeContext(optCon);
    rpmFreeMacros(NULL);
	rpmFreeMacros(rpmCLIMacroContext);
    rpmFreeRpmrc();

    if (pipeChild) {
	(void) fclose(stdout);
	(void) waitpid(pipeChild, &status, 0);
    }

    /* keeps memory leak checkers quiet */
    rpmlogClose();

#ifdef	IAM_RPMBT
    freeNames();
    ba->buildRootOverride = _free(ba->buildRootOverride);
    ba->targets = _free(ba->targets);
#endif

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    /* XXX Avoid exit status overflow. Status 255 is special to xargs(1) */
    if (ec > 254) ec = 254;

    return ec;
}
