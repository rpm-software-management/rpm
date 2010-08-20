#include "system.h"
const char *__progname;

#define	_AUTOHELP

#include <errno.h>
#include <libgen.h>
#include <ctype.h>
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
#include "cliutils.h"

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

static int checkSpec(rpmts ts, Header h)
{
    rpmps ps;
    int rc;

    if (!headerIsEntry(h, RPMTAG_REQUIRENAME)
     && !headerIsEntry(h, RPMTAG_CONFLICTNAME))
	return 0;

    rc = rpmtsAddInstallElement(ts, h, NULL, 0, NULL);

    rc = rpmtsCheck(ts);

    ps = rpmtsProblems(ts);
    if (rc == 0 && rpmpsNumProblems(ps) > 0) {
	rpmlog(RPMLOG_ERR, _("Failed build dependencies:\n"));
	rpmpsPrint(NULL, ps);
	rc = 1;
    }
    ps = rpmpsFree(ps);

    /* XXX nuke the added package. */
    rpmtsClean(ts);

    return rc;
}

static int isSpecFile(const char * specfile)
{
    char buf[256];
    const char * s;
    FILE * f;
    int count;
    int checking;

    f = fopen(specfile, "r");
    if (f == NULL || ferror(f)) {
	rpmlog(RPMLOG_ERR, _("Unable to open spec file %s: %s\n"),
		specfile, strerror(errno));
	return 0;
    }
    count = fread(buf, sizeof(buf[0]), sizeof(buf), f);
    (void) fclose(f);

    if (count == 0)
	return 0;

    checking = 1;
    for (s = buf; count--; s++) {
	switch (*s) {
	case '\r':
	case '\n':
	    checking = 1;
	    break;
	case ':':
	    checking = 0;
	    break;
	default:
#if 0
	    if (checking && !(isprint(*s) || isspace(*s))) return 0;
	    break;
#else
	    if (checking && !(isprint(*s) || isspace(*s)) && *(unsigned char *)s < 32) return 0;
	    break;
#endif
	}
    }
    return 1;
}

/* 
 * Try to find a spec from a tarball pointed to by arg. 
 * Return absolute path to spec name on success, otherwise NULL.
 */
static char * getTarSpec(const char *arg)
{
    char *specFile = NULL;
    char *specDir;
    char *specBase;
    char *tmpSpecFile;
    const char **try;
    char tarbuf[BUFSIZ];
    int gotspec = 0, res;
    static const char *tryspec[] = { "Specfile", "\\*.spec", NULL };

    specDir = rpmGetPath("%{_specdir}", NULL);
    tmpSpecFile = rpmGetPath("%{_specdir}/", "rpm-spec.XXXXXX", NULL);

    (void) close(mkstemp(tmpSpecFile));

    for (try = tryspec; *try != NULL; try++) {
	FILE *fp;
	char *cmd;

	cmd = rpmExpand("%{uncompress: ", arg, "} | ",
			"%{__tar} xOvf - --wildcards ", *try,
			" 2>&1 > ", tmpSpecFile, NULL);

	if (!(fp = popen(cmd, "r"))) {
	    rpmlog(RPMLOG_ERR, _("Failed to open tar pipe: %m\n"));
	} else {
	    char *fok;
	    for (;;) {
		fok = fgets(tarbuf, sizeof(tarbuf) - 1, fp);
		/* tar sometimes prints "tar: Record size = 16" messages */
		if (!fok || strncmp(fok, "tar: ", 5) != 0)
		    break;
	    }
	    pclose(fp);
	    gotspec = (fok != NULL) && isSpecFile(tmpSpecFile);
	}

	if (!gotspec) 
	    unlink(tmpSpecFile);
	free(cmd);
    }

    if (!gotspec) {
    	rpmlog(RPMLOG_ERR, _("Failed to read spec file from %s\n"), arg);
	goto exit;
    }

    specBase = basename(tarbuf);
    /* remove trailing \n */
    specBase[strlen(specBase)-1] = '\0';

    rasprintf(&specFile, "%s/%s", specDir, specBase);
    res = rename(tmpSpecFile, specFile);

    if (res) {
    	rpmlog(RPMLOG_ERR, _("Failed to rename %s to %s: %m\n"),
		tmpSpecFile, specFile);
    	free(specFile);
	specFile = NULL;
    } else {
    	/* mkstemp() can give unnecessarily strict permissions, fixup */
	mode_t mask;
	umask(mask = umask(0));
	(void) chmod(specFile, 0666 & ~mask);
    }

exit:
    (void) unlink(tmpSpecFile);
    free(tmpSpecFile);
    free(specDir);
    return specFile;
}

static int buildForTarget(rpmts ts, const char * arg, BTA_t ba)
{
    const char * passPhrase = ba->passPhrase;
    const char * cookie = ba->cookie;
    int buildAmount = ba->buildAmount;
    char * buildRootURL = NULL;
    char * specFile = NULL;
    rpmSpec spec = NULL;
    int rc = 1; /* assume failure */

#ifndef	DYING
    rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);
#endif

    if (ba->buildRootOverride)
	buildRootURL = rpmGenPath(NULL, ba->buildRootOverride, NULL);

    /* Create build tree if necessary */
    const char * buildtree = "%{_topdir}:%{_specdir}:%{_sourcedir}:%{_builddir}:%{_rpmdir}:%{_srcrpmdir}:%{_buildrootdir}";
    const char * rootdir = rpmtsRootDir(ts);
    if (rpmMkdirs(!rstreq(rootdir, "/") ? rootdir : NULL , buildtree)) {
	goto exit;
    }

    if (ba->buildMode == 't') {
    	char *srcdir = NULL, *dir;

	specFile = getTarSpec(arg);
	if (!specFile)
	    goto exit;

	/* Make the directory of the tarball %_sourcedir for this run */
	/* dirname() may modify contents so extra hoops needed. */
	if (*arg != '/') {
	    dir = rpmGetCwd();
	    rstrscat(&dir, "/", arg, NULL);
	} else {
	    dir = xstrdup(arg);
	}
	srcdir = dirname(dir);
	addMacro(NULL, "_sourcedir", NULL, srcdir, RMIL_TARBALL);
	free(dir);
    } else {
	specFile = xstrdup(arg);
    }

    if (*specFile != '/') {
	char *cwd = rpmGetCwd();
	char *s = NULL;
	rasprintf(&s, "%s/%s", cwd, arg);
	free(cwd);
	free(specFile);
	specFile = s;
    }

    struct stat st;
    if (stat(specFile, &st) < 0) {
	rpmlog(RPMLOG_ERR, _("failed to stat %s: %m\n"), specFile);
	goto exit;
    }
    if (! S_ISREG(st.st_mode)) {
	rpmlog(RPMLOG_ERR, _("File %s is not a regular file.\n"), specFile);
	goto exit;
    }

    /* Try to verify that the file is actually a specfile */
    if (!isSpecFile(specFile)) {
	rpmlog(RPMLOG_ERR,
		_("File %s does not appear to be a specfile.\n"), specFile);
	goto exit;
    }
    
    /* Don't parse spec if only its removal is requested */
    if (ba->buildAmount == RPMBUILD_RMSPEC) {
	rc = unlink(specFile);
	goto exit;
    }

    /* Parse the spec file */
#define	_anyarch(_f)	\
(((_f)&(RPMBUILD_PREP|RPMBUILD_BUILD|RPMBUILD_INSTALL|RPMBUILD_PACKAGEBINARY)) == 0)
    if (parseSpec(ts, specFile, ba->rootdir, buildRootURL, 0, passPhrase,
		cookie, _anyarch(buildAmount), ba->force))
    {
	goto exit;
    }
#undef	_anyarch
    if ((spec = rpmtsSetSpec(ts, NULL)) == NULL) {
	goto exit;
    }

    if ( ba->buildAmount&RPMBUILD_RMSOURCE && !(ba->buildAmount&~(RPMBUILD_RMSOURCE|RPMBUILD_RMSPEC)) ) {
	rc = doRmSource(spec);
	if ( rc == RPMRC_OK && ba->buildAmount&RPMBUILD_RMSPEC )
	    rc = unlink(specFile);
	goto exit;
    }

    /* Assemble source header from parsed components */
    initSourceHeader(spec);

    /* Check build prerequisites */
    if (!ba->noDeps && checkSpec(ts, spec->sourceHeader)) {
	goto exit;
    }

    if (buildSpec(ts, spec, buildAmount, ba->noBuild)) {
	goto exit;
    }
    
    if (ba->buildMode == 't')
	(void) unlink(specFile);
    rc = 0;

exit:
    free(specFile);
    freeSpec(spec);
    free(buildRootURL);
    return rc;
}

static int build(rpmts ts, const char * arg, BTA_t ba, const char * rcfile)
{
    char *t, *te;
    int rc = 0;
    char * targets = ba->targets;
#define	buildCleanMask	(RPMBUILD_RMSOURCE|RPMBUILD_RMSPEC)
    int cleanFlags = ba->buildAmount & buildCleanMask;
    rpmVSFlags vsflags, ovsflags;

    vsflags = rpmExpandNumeric("%{_vsflags_build}");
    if (ba->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (ba->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (ba->qva_flags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    ovsflags = rpmtsSetVSFlags(ts, vsflags);

    if (targets == NULL) {
	rc =  buildForTarget(ts, arg, ba);
	goto exit;
    }

    /* parse up the build operators */

    printf(_("Building target platforms: %s\n"), targets);

    ba->buildAmount &= ~buildCleanMask;
    for (t = targets; *t != '\0'; t = te) {
	char *target;
	if ((te = strchr(t, ',')) == NULL)
	    te = t + strlen(t);
	target = xmalloc(te-t+1);
	strncpy(target, t, (te-t));
	target[te-t] = '\0';
	if (*te != '\0')
	    te++;
	else	/* XXX Perform clean-up after last target build. */
	    ba->buildAmount |= cleanFlags;

	printf(_("Building for target %s\n"), target);

	/* Read in configuration for target. */
	rpmFreeMacros(NULL);
	rpmFreeRpmrc();
	(void) rpmReadConfigFiles(rcfile, target);
	free(target);
	rc = buildForTarget(ts, arg, ba);
	if (rc)
	    break;
    }

exit:
    vsflags = rpmtsSetVSFlags(ts, ovsflags);
    /* Restore original configuration. */
    rpmFreeMacros(NULL);
    rpmFreeRpmrc();
    (void) rpmReadConfigFiles(rcfile, NULL);

    return rc;
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
    const char *pkg = NULL;
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

    /* rpmbuild is rather chatty by default */
    rpmSetVerbosity(quiet ? RPMLOG_WARNING : RPMLOG_INFO);

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
	break;
    case MODE_BUILD:
    case MODE_TARBUILD:
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
