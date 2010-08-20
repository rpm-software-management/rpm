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
#include "lib/signature.h"
#include "build.h"

#include "debug.h"

enum modes {
    MODE_BUILD		= (1 <<  4),
    MODE_REBUILD	= (1 <<  5),
    MODE_RECOMPILE	= (1 <<  8),
    MODE_TARBUILD	= (1 << 11),
};

static int quiet;

/* the structure describing the options we take and the defaults */
static struct poptOption optionsTable[] = {

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmBuildPoptTable, 0,
	N_("Build options with [ <specfile> | <tarball> | <source package> ]:"),
	NULL },

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
    enum modes bigMode = MODE_BUILD;
    BTA_t ba = &rpmBTArgs;
    char * passPhrase = "";

    int arg;
    const char *optArg;
    const char *poptCtx = "rpmbuild";
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

#if defined(ENABLE_NLS)
    /* set up the correct locale */
    (void) setlocale(LC_ALL, "" );

    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    rpmSetVerbosity(RPMLOG_NOTICE);	/* XXX silly use by showrc */

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

    if (argc <= 1 || poptPeekArg(optCon) == NULL) {
	printUsage(optCon, stderr, 0);
	exit(EXIT_FAILURE);
    }

    rpmcliConfigured();

    switch (ba->buildMode) {
    case 'b':	bigMode = MODE_BUILD;		break;
    case 't':	bigMode = MODE_TARBUILD;	break;
    case 'B':	bigMode = MODE_REBUILD;		break;
    case 'C':	bigMode = MODE_RECOMPILE;	break;
    }

    if (rpmcliRootDir && rpmcliRootDir[0] != '/') {
	argerror(_("arguments to --root (-r) must begin with a /"));
    }

    if (quiet)
	rpmSetVerbosity(RPMLOG_WARNING);

    if (ba->sign) {
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
	    fprintf(stderr, _("Invalid %%_signature spec in macro file.\n"));
	    ec = EXIT_FAILURE;
	    goto exit;
	    break;
	}
    } else {
    	/* Make rpmLookupSignatureType() return 0 ("none") from now on */
        (void) rpmLookupSignatureType(RPMLOOKUPSIG_DISABLE);
    }

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
    case MODE_REBUILD:
    case MODE_RECOMPILE:
    {	const char * pkg;

        while (!rpmIsVerbose())
	    rpmIncreaseVerbosity();

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

    freeNames();
    ba->buildRootOverride = _free(ba->buildRootOverride);
    ba->targets = _free(ba->targets);

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    /* XXX Avoid exit status overflow. Status 255 is special to xargs(1) */
    if (ec > 254) ec = 254;

    return ec;
}
