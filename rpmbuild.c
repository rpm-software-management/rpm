#include "system.h"

#include <errno.h>
#include <libgen.h>
#include <ctype.h>

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

static struct rpmBuildArguments_s rpmBTArgs;

#define	POPT_NOLANG		-1012
#define	POPT_RMSOURCE		-1013
#define	POPT_RMBUILD		-1014
#define	POPT_BUILDROOT		-1015
#define	POPT_TARGETPLATFORM	-1016
#define	POPT_NOBUILD		-1017
#define	POPT_RMSPEC		-1019
#define POPT_NODIRTOKENS	-1020
#define POPT_BUILDINPLACE	-1021

#define	POPT_REBUILD		0x4262 /* Bb */
#define	POPT_RECOMPILE		0x4369 /* Ci */
#define	POPT_BA			0x6261
#define	POPT_BB			0x6262
#define	POPT_BC			0x6263
#define	POPT_BI			0x6269
#define	POPT_BL			0x626c
#define	POPT_BP			0x6270
#define	POPT_BS			0x6273
#define	POPT_BR			0x6272
#define	POPT_RA			0x4261
#define	POPT_RB			0x4262
#define	POPT_RC			0x4263
#define	POPT_RI			0x4269
#define	POPT_RL			0x426c
#define	POPT_RP			0x4270
#define	POPT_RS			0x4273
#define	POPT_RR			0x4272
#define	POPT_TA			0x7461
#define	POPT_TB			0x7462
#define	POPT_TC			0x7463
#define	POPT_TI			0x7469
#define	POPT_TL			0x746c
#define	POPT_TP			0x7470
#define	POPT_TS			0x7473
#define	POPT_TR			0x7472

extern int _fsm_debug;

static rpmSpecFlags spec_flags = 0;	/*!< Bit(s) to control spec parsing. */
static int noDeps = 0;			/*!< from --nodeps */
static int shortCircuit = 0;		/*!< from --short-circuit */
static char buildMode = 0;		/*!< Build mode (one of "btBC") */
static char buildChar = 0;		/*!< Build stage (one of "abcilps ") */
static rpmBuildFlags nobuildAmount = 0;	/*!< Build stage disablers */
static ARGV_t build_targets = NULL;	/*!< Target platform(s) */
static int buildInPlace = 0;		/*!< from --build-in-place */

static void buildArgCallback( poptContext con,
	enum poptCallbackReason reason,
	const struct poptOption * opt, const char * arg,
	const void * data)
{
    BTA_t rba = &rpmBTArgs;

    switch (opt->val) {
    case POPT_REBUILD:
    case POPT_RECOMPILE:
    case POPT_BA:
    case POPT_BB:
    case POPT_BC:
    case POPT_BI:
    case POPT_BL:
    case POPT_BP:
    case POPT_BS:
    case POPT_BR:
    case POPT_RA:
    /* case POPT_RB: same value as POPT_REBUILD */
    case POPT_RC:
    case POPT_RI:
    case POPT_RL:
    case POPT_RP:
    case POPT_RS:
    case POPT_RR:
    case POPT_TA:
    case POPT_TB:
    case POPT_TC:
    case POPT_TI:
    case POPT_TL:
    case POPT_TP:
    case POPT_TS:
    case POPT_TR:
	if (opt->val == POPT_BS || opt->val == POPT_TS)
	    noDeps = 1;
	if (buildMode == '\0' && buildChar == '\0') {
	    buildMode = (((unsigned)opt->val) >> 8) & 0xff;
	    buildChar = (opt->val     ) & 0xff;
	}
	break;

    case POPT_NODIRTOKENS: rba->pkgFlags |= RPMBUILD_PKG_NODIRTOKENS; break;
    case POPT_NOBUILD: rba->buildAmount |= RPMBUILD_NOBUILD; break;
    case POPT_NOLANG: spec_flags |= RPMSPEC_NOLANG; break;
    case POPT_RMSOURCE: rba->buildAmount |= RPMBUILD_RMSOURCE; break;
    case POPT_RMSPEC: rba->buildAmount |= RPMBUILD_RMSPEC; break;
    case POPT_RMBUILD: rba->buildAmount |= RPMBUILD_RMBUILD; break;
    case POPT_BUILDROOT:
	if (rba->buildRootOverride) {
	    rpmlog(RPMLOG_ERR, _("buildroot already specified, ignoring %s\n"), arg);
	    break;
	}
	rba->buildRootOverride = xstrdup(arg);
	break;
    case POPT_TARGETPLATFORM:
	argvSplit(&build_targets, arg, ",");
	break;

    case RPMCLI_POPT_FORCE:
	spec_flags |= RPMSPEC_FORCE;
	break;

    case POPT_BUILDINPLACE:
	rpmDefineMacro(NULL, "_build_in_place 1", 0);
	buildInPlace = 1;
	break;
    }
}

static struct poptOption rpmBuildPoptTable[] = {
 { NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA | POPT_CBFLAG_CONTINUE,
	buildArgCallback, 0, NULL, NULL },

 { "bp", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BP,
	N_("build through %prep (unpack sources and apply patches) from <specfile>"),
	N_("<specfile>") },
 { "bc", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BC,
	N_("build through %build (%prep, then compile) from <specfile>"),
	N_("<specfile>") },
 { "bi", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BI,
	N_("build through %install (%prep, %build, then install) from <specfile>"),
	N_("<specfile>") },
 { "bl", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BL,
	N_("verify %files section from <specfile>"),
	N_("<specfile>") },
 { "ba", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BA,
	N_("build source and binary packages from <specfile>"),
	N_("<specfile>") },
 { "bb", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BB,
	N_("build binary package only from <specfile>"),
	N_("<specfile>") },
 { "bs", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BS,
	N_("build source package only from <specfile>"),
	N_("<specfile>") },
 { "br", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_BR,
	N_("build source package only from <specfile> - calculate dynamic build requires"),
	N_("<specfile>") },

 { "rp", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_RP,
	N_("build through %prep (unpack sources and apply patches) from <source package>"),
	N_("<source package>") },
 { "rc", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_RC,
	N_("build through %build (%prep, then compile) from <source package>"),
	N_("<source package>") },
 { "ri", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_RI,
	N_("build through %install (%prep, %build, then install) from <source package>"),
	N_("<source package>") },
 { "rl", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_RL,
	N_("verify %files section from <source package>"),
	N_("<source package>") },
 { "ra", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_RA,
	N_("build source and binary packages from <source package>"),
	N_("<source package>") },
 { "rb", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_RB,
	N_("build binary package only from <source package>"),
	N_("<source package>") },
 { "rs", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_RS,
	N_("build source package only from <source package>"),
	N_("<source package>") },
 { "rr", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_RR,
	N_("build source package only from <source package> - calculate dynamic build requires"),
	N_("<source package>") },

 { "tp", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TP,
	N_("build through %prep (unpack sources and apply patches) from <tarball>"),
	N_("<tarball>") },
 { "tc", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TC,
	N_("build through %build (%prep, then compile) from <tarball>"),
	N_("<tarball>") },
 { "ti", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TI,
	N_("build through %install (%prep, %build, then install) from <tarball>"),
	N_("<tarball>") },
 { "tl", 0, POPT_ARGFLAG_ONEDASH|POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_TL,
	N_("verify %files section from <tarball>"),
	N_("<tarball>") },
 { "ta", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TA,
	N_("build source and binary packages from <tarball>"),
	N_("<tarball>") },
 { "tb", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TB,
	N_("build binary package only from <tarball>"),
	N_("<tarball>") },
 { "ts", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TS,
	N_("build source package only from <tarball>"),
	N_("<tarball>") },
 { "tr", 0, POPT_ARGFLAG_ONEDASH, 0, POPT_TR,
	N_("build source package only from <tarball> - calculate dynamic build requires"),
	N_("<tarball>") },
 { "rebuild", '\0', 0, 0, POPT_REBUILD,
	N_("build binary package from <source package>"),
	N_("<source package>") },
 { "recompile", '\0', 0, 0, POPT_RECOMPILE,
	N_("build through %install (%prep, %build, then install) from <source package>"),
	N_("<source package>") },

 { "buildroot", '\0', POPT_ARG_STRING, 0,  POPT_BUILDROOT,
	N_("override build root"), "DIRECTORY" },
 { "build-in-place", '\0', 0, 0, POPT_BUILDINPLACE,
	N_("run build in current directory"), NULL },
 { "clean", '\0', 0, 0, POPT_RMBUILD,
	N_("remove build tree when done"), NULL},
 { "force", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, RPMCLI_POPT_FORCE,
        N_("ignore ExcludeArch: directives from spec file"), NULL},
 { "fsmdebug", '\0', (POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN), &_fsm_debug, -1,
	N_("debug file state machine"), NULL},
 { "nobuild", '\0', 0, 0,  POPT_NOBUILD,
	N_("do not execute any stages of the build"), NULL },
 { "nodeps", '\0', POPT_ARG_VAL, &noDeps, 1,
	N_("do not verify build dependencies"), NULL },
 { "nodirtokens", '\0', 0, 0, POPT_NODIRTOKENS,
	N_("generate package header(s) compatible with (legacy) rpm v3 packaging"),
	NULL},

 { "noclean", '\0', POPT_BIT_SET, &nobuildAmount, RPMBUILD_CLEAN,
	N_("do not execute %clean stage of the build"), NULL },
 { "noprep", '\0', POPT_BIT_SET, &nobuildAmount, RPMBUILD_PREP,
	N_("do not execute %prep stage of the build"), NULL },
 { "nocheck", '\0', POPT_BIT_SET, &nobuildAmount, RPMBUILD_CHECK,
	N_("do not execute %check stage of the build"), NULL },

 { "nolang", '\0', POPT_ARGFLAG_DOC_HIDDEN, 0, POPT_NOLANG,
	N_("do not accept i18N msgstr's from specfile"), NULL},
 { "rmsource", '\0', 0, 0, POPT_RMSOURCE,
	N_("remove sources when done"), NULL},
 { "rmspec", '\0', 0, 0, POPT_RMSPEC,
	N_("remove specfile when done"), NULL},
 { "short-circuit", '\0', POPT_ARG_VAL, &shortCircuit,  1,
	N_("skip straight to specified stage (only for c,i)"), NULL },
 { "target", '\0', POPT_ARG_STRING, 0,  POPT_TARGETPLATFORM,
	N_("override target platform"), "CPU-VENDOR-OS" },
   POPT_TABLEEND
};

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

 { "quiet", '\0', POPT_ARGFLAG_DOC_HIDDEN, &quiet, 0, NULL, NULL},

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL },

   POPT_AUTOALIAS
   POPT_AUTOHELP
   POPT_TABLEEND
};

static int isSpecFile(const char * specfile)
{
    char buf[256];
    const char * s;
    FILE * f;
    int count;
    int checking;

    f = fopen(specfile, "r");
    if (f == NULL) {
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
    char *specFinal = NULL;
    const char **spec;
    char tarbuf[BUFSIZ];
    int gotspec = 0;
    FD_t fd = NULL;
    static const char *tryspec[] = { "Specfile", "\\*.spec", NULL };

    specFile = rpmGetPath("%{_specdir}/", "rpm-spec.XXXXXX", NULL);
    if (!(fd = rpmMkTemp(specFile)))
	goto exit;

    for (spec = tryspec; *spec != NULL; spec++) {
	FILE *fp;
	char *cmd;
	int specfiles = 0;

	cmd = rpmExpand("%{uncompress: ", arg, "} | ",
			"%{__tar} xOvof - --wildcards ", *spec,
			" 2>&1 > ", specFile, NULL);

	if (!(fp = popen(cmd, "r"))) {
	    rpmlog(RPMLOG_ERR, _("Failed to open tar pipe: %m\n"));
	} else {
	    char *fok;
	    for (;;) {
		fok = fgets(tarbuf, sizeof(tarbuf) - 1, fp);
		if (!fok) break;
		/* tar sometimes prints "tar: Record size = 16" messages */
		if (strstr(fok, "tar: ")) {
		    continue;
		}
		specfiles++;
	    }
	    pclose(fp);
	    gotspec = (specfiles == 1) && isSpecFile(specFile);
	    if (specfiles > 1) {
		rpmlog(RPMLOG_ERR, _("Found more than one spec file in %s\n"), arg);
		goto exit;
	    }
	}

	if (!gotspec)
	    unlink(specFile);
	free(cmd);
    }

    if (!gotspec) {
    	rpmlog(RPMLOG_ERR, _("Failed to read spec file from %s\n"), arg);
    } else {
	/* remove trailing \n */
	tarbuf[strlen(tarbuf)-1] = '\0';
	specFinal = rpmExpand("%{_specdir}/%{basename:", tarbuf, "}", NULL);
	if (rename(specFile, specFinal)) {
	    rpmlog(RPMLOG_ERR, _("Failed to rename %s to %s: %m\n"),
		    specFile, specFinal);
	    unlink(specFile);
	    specFinal = _free(specFinal);
	}
    }

exit:
    free(specFile);
    Fclose(fd);
    return specFinal;
}

static int buildForTarget(rpmts ts, const char * arg, BTA_t ba)
{
    int buildAmount = ba->buildAmount;
    char * buildRootURL = NULL;
    char * specFile = NULL;
    rpmSpec spec = NULL;
    int rc = 1; /* assume failure */
    rpmSpecFlags specFlags = spec_flags;

    /* Override default BUILD value for _builddir */
    if (buildInPlace) {
	char *cwd = rpmGetCwd();
	rpmPushMacro(NULL, "_builddir", NULL, cwd, 0);
	free(cwd);
    }

    if (ba->buildRootOverride)
	buildRootURL = rpmGenPath(NULL, ba->buildRootOverride, NULL);

    const char *rootdir = rpmtsRootDir(ts);
    const char *root = !rstreq(rootdir, "/") ? rootdir : NULL;

    if (buildMode == 't') {
    	char *srcdir = NULL, *dir;
	if (rpmMkdirs(root, "%{_specdir}"))
	    goto exit;

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
	rpmPushMacro(NULL, "_sourcedir", NULL, srcdir, RMIL_TARBALL);
	free(dir);
    } else {
	specFile = xstrdup(arg);
    }

    if (*specFile != '/') {
	char *cwd = rpmGetCwd();
	char *s = NULL;
	rasprintf(&s, "%s/%s", cwd, specFile);
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
    if (_anyarch(buildAmount))
	specFlags |= RPMSPEC_ANYARCH;
#undef	_anyarch
    
    spec = rpmSpecParse(specFile, specFlags, buildRootURL);
    if (spec == NULL) {
	goto exit;
    }

    /* Create build tree if necessary */
    if (rpmMkdirs(root, "%{_topdir}:%{_builddir}:%{_rpmdir}:%{_srcrpmdir}:%{_buildrootdir}"))
	goto exit;

    if ((rc = rpmSpecBuild(ts, spec, ba))) {
	goto exit;
    }
    
    if (buildMode == 't')
	(void) unlink(specFile);
    rc = 0;

exit:
    free(specFile);
    rpmSpecFree(spec);
    free(buildRootURL);
    return rc;
}

static int build(rpmts ts, const char * arg, BTA_t ba, const char * rcfile)
{
    int rc = 0;
    char * targets = argvJoin(build_targets, ",");
#define	buildCleanMask	(RPMBUILD_RMSOURCE|RPMBUILD_RMSPEC)
    int cleanFlags = ba->buildAmount & buildCleanMask;
    rpmVSFlags vsflags, ovsflags;

    vsflags = rpmExpandNumeric("%{_vsflags_build}");
    vsflags |= rpmcliVSFlags;
    ovsflags = rpmtsSetVSFlags(ts, vsflags);

    if (build_targets == NULL) {
	rc =  buildForTarget(ts, arg, ba);
	goto exit;
    }

    /* parse up the build operators */

    printf(_("Building target platforms: %s\n"), targets);

    ba->buildAmount &= ~buildCleanMask;
    for (ARGV_const_t target = build_targets; target && *target; target++) {
	/* Perform clean-up after last target build. */
	if (*(target + 1) == NULL)
	    ba->buildAmount |= cleanFlags;

	printf(_("Building for target %s\n"), *target);

	/* Read in configuration for target. */
	rpmFreeMacros(NULL);
	if (buildInPlace) {
		/* Need to redefine this after freeing all the macros */
		rpmDefineMacro(NULL, "_build_in_place 1", 0);
	}
	rpmFreeRpmrc();
	(void) rpmReadConfigFiles(rcfile, *target);
	rc = buildForTarget(ts, arg, ba);
	if (rc)
	    break;
    }

exit:
    rpmtsSetVSFlags(ts, ovsflags);
    /* Restore original configuration. */
    rpmFreeMacros(NULL);
    rpmFreeRpmrc();
    (void) rpmReadConfigFiles(rcfile, NULL);
    free(targets);

    return rc;
}

int main(int argc, char *argv[])
{
    rpmts ts = NULL;
    enum modes bigMode = MODE_BUILD;
    BTA_t ba = &rpmBTArgs;

    const char *pkg = NULL;
    int ec = 0;

    poptContext optCon = NULL;

    xsetprogname(argv[0]); /* Portability call -- see system.h */

    optCon = rpmcliInit(argc, argv, optionsTable);

    /* Args required only when building, let lone --eval etc through */
    if (ba->buildAmount && poptPeekArg(optCon) == NULL) {
	printUsage(optCon, stderr, 0);
	exit(EXIT_FAILURE);
    }

    switch (buildMode) {
    case 'b':	bigMode = MODE_BUILD;		break;
    case 't':	bigMode = MODE_TARBUILD;	break;
    case 'B':	bigMode = MODE_REBUILD;		break;
    case 'C':	bigMode = MODE_RECOMPILE;	break;
    }

    if (rpmcliRootDir && rpmcliRootDir[0] != '/') {
	argerror(_("arguments to --root (-r) must begin with a /"));
    }

    /* rpmbuild runs in verbose mode by default */
    if (rpmlogSetMask(0) < RPMLOG_MASK(RPMLOG_INFO))
	rpmSetVerbosity(RPMLOG_INFO);

    if (quiet)
	rpmSetVerbosity(RPMLOG_WARNING);

    if (rpmcliPipeOutput && initPipe())
	exit(EXIT_FAILURE);
	
    ts = rpmtsCreate();
    (void) rpmtsSetRootDir(ts, rpmcliRootDir);
    rpmtsSetFlags(ts, rpmtsFlags(ts) | RPMTRANS_FLAG_NOPLUGINS);

    switch (buildChar) {
    case 'a':
	ba->buildAmount |= RPMBUILD_PACKAGESOURCE;
    case 'b':
	ba->buildAmount |= RPMBUILD_PACKAGEBINARY;
	ba->buildAmount |= RPMBUILD_CLEAN;
	if ((buildChar == 'b') && shortCircuit)
	    break;
    case 'i':
	ba->buildAmount |= RPMBUILD_INSTALL;
	ba->buildAmount |= RPMBUILD_CHECK;
	if ((buildChar == 'i') && shortCircuit)
	    break;
    case 'c':
	ba->buildAmount |= RPMBUILD_BUILD;
	ba->buildAmount |= RPMBUILD_BUILDREQUIRES;
	if (!noDeps) {
	    ba->buildAmount |= RPMBUILD_DUMPBUILDREQUIRES;
	    ba->buildAmount |= RPMBUILD_CHECKBUILDREQUIRES;
	}
	if ((buildChar == 'c') && shortCircuit)
	    break;
    case 'p':
	ba->buildAmount |= RPMBUILD_PREP;
	break;
    case 'l':
	ba->buildAmount |= RPMBUILD_FILECHECK;
	break;
    case 'r':
	ba->buildAmount |= RPMBUILD_PREP;
	ba->buildAmount |= RPMBUILD_BUILDREQUIRES;
	ba->buildAmount |= RPMBUILD_DUMPBUILDREQUIRES;
	if (!noDeps)
	    ba->buildAmount |= RPMBUILD_CHECKBUILDREQUIRES;
    case 's':
	ba->buildAmount |= RPMBUILD_PACKAGESOURCE;
	break;
    }
    ba->buildAmount &= ~(nobuildAmount);

    switch (bigMode) {
    case MODE_REBUILD:
    case MODE_RECOMPILE:
	if (bigMode == MODE_REBUILD &&
	    buildChar != 'p' &&
	    buildChar != 'c' &&
	    buildChar != 'i' &&
	    buildChar != 'l') {
	    ba->buildAmount |= RPMBUILD_RMSOURCE;
	    ba->buildAmount |= RPMBUILD_RMSPEC;
	    ba->buildAmount |= RPMBUILD_RMBUILD;
	}
	ba->buildAmount &= ~(nobuildAmount);

	while ((pkg = poptGetArg(optCon))) {
	    char * specFile = NULL;

	    ba->cookie = NULL;
	    ec = rpmInstallSource(ts, pkg, &specFile, &ba->cookie);
	    if (ec == 0) {
		ba->rootdir = rpmcliRootDir;
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

	while ((pkg = poptGetArg(optCon))) {
	    ba->rootdir = rpmcliRootDir;
	    ba->cookie = NULL;
	    ec = build(ts, pkg, ba, rpmcliRcfile);
	    if (ec)
		break;
	    rpmFreeMacros(NULL);
	    (void) rpmReadConfigFiles(rpmcliRcfile, NULL);
	}
	break;
    }

    rpmtsFree(ts);
    if (finishPipe())
	ec = EXIT_FAILURE;
    free(ba->buildRootOverride);
    argvFree(build_targets);

    rpmcliFini(optCon);

    return RETVAL(ec);
}
