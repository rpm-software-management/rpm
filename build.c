#include "system.h"

#include "rpmbuild.h"
#include "build.h"
#include "install.h"

static int checkSpec(Header h)
{
    char *rootdir = NULL;
    rpmdb db = NULL;
    int mode = O_RDONLY;
    rpmTransactionSet ts;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    int rc;

    if (!headerIsEntry(h, RPMTAG_REQUIREFLAGS))
	return 0;

    if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	const char *dn;
	dn = rpmGetPath( (rootdir ? rootdir : ""), "%{_dbpath}", NULL);
	rpmMessage(RPMMESS_ERROR, _("cannot open %s/packages.rpm\n"), dn);
	xfree(dn);
	exit(EXIT_FAILURE);
    }
    ts = rpmtransCreateSet(db, rootdir);

    rc = rpmtransAddPackage(ts, h, NULL, NULL, 0, NULL);

    rc = rpmdepCheck(ts, &conflicts, &numConflicts);
    if (rc == 0 && conflicts) {
	rpmMessage(RPMMESS_ERROR, _("failed build dependencies:\n"));
	printDepProblems(stderr, conflicts, numConflicts);
	rpmdepFreeConflicts(conflicts, numConflicts);
	rc = 1;
    }

    if (ts)
	rpmtransFree(ts);
    if (db)
	rpmdbClose(db);

    return rc;
}

/*
 * Kurwa, durni ameryka?ce sobe zawsze my?l?, ?e ca?y ?wiat mówi po
 * angielsku...
 */
/* XXX this is still a dumb test but at least it's i18n aware */
static int isSpecFile(const char *specfile)
{
    char buf[256];
    const char * s;
    FD_t fd;
    int count;
    int checking;

    if (fdFileno((fd = fdOpen(specfile, O_RDONLY, 0))) < 0) {
	fprintf(stderr, _("Unable to open spec file: %s\n"), specfile);
	return 0;
    }
    count = fdRead(fd, buf, sizeof(buf));
    fdClose(fd);

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
	    if (checking && !(isprint(*s) || isspace(*s))) return 0;
	    break;
	}
    }
    return 1;
}

static int buildForTarget(const char *arg, struct rpmBuildArguments *ba,
	const char *passPhrase, int fromTarball, char *cookie,
	int force, int nodeps)
{
    int buildAmount = ba->buildAmount;
    const char *buildRoot = ba->buildRootOverride;
    int test = ba->noBuild;
    FILE *f;
    const char * specfile;
    int res = 0;
    struct stat statbuf;
    char buf[BUFSIZ];
    Spec spec = NULL;

    rpmSetTables(RPM_MACHTABLE_BUILDARCH, RPM_MACHTABLE_BUILDOS);

    if (fromTarball) {
	const char *specDir;
	const char * tmpSpecFile;
	char * cmd, *s;

	specDir = rpmGetPath("%{_specdir}", NULL);

	{   char tfn[64];
	    strcpy(tfn, "rpm-spec.XXXXXX");
	    tmpSpecFile = rpmGetPath("%{_specdir}/", mktemp(tfn), NULL);
	}

	cmd = alloca(strlen(arg) + 50 + strlen(tmpSpecFile));
	sprintf(cmd, "gunzip < %s | tar xOvf - Specfile 2>&1 > %s",
			arg, tmpSpecFile);
	if (!(f = popen(cmd, "r"))) {
	    fprintf(stderr, _("Failed to open tar pipe: %s\n"), 
			strerror(errno));
	    xfree(specDir);
	    xfree(tmpSpecFile);
	    return 1;
	}
	if ((!fgets(buf, sizeof(buf) - 1, f)) || !strchr(buf, '/')) {
	    /* Try again */
	    pclose(f);

	    sprintf(cmd, "gunzip < %s | tar xOvf - \\*.spec 2>&1 > %s", arg,
		    tmpSpecFile);
	    if (!(f = popen(cmd, "r"))) {
		fprintf(stderr, _("Failed to open tar pipe: %s\n"), 
			strerror(errno));
		xfree(specDir);
		xfree(tmpSpecFile);
		return 1;
	    }
	    if (!fgets(buf, sizeof(buf) - 1, f)) {
		/* Give up */
		fprintf(stderr, _("Failed to read spec file from %s\n"), arg);
		unlink(tmpSpecFile);
		xfree(specDir);
		xfree(tmpSpecFile);
	    	return 1;
	    }
	}
	pclose(f);

	cmd = s = buf;
	while (*cmd) {
	    if (*cmd == '/') s = cmd + 1;
	    cmd++;
	}

	cmd = s;

	/* remove trailing \n */
	s = cmd + strlen(cmd) - 1;
	*s = '\0';

	s = alloca(strlen(specDir) + strlen(cmd) + 5);
	sprintf(s, "%s/%s", specDir, cmd);
	
	if (rename(tmpSpecFile, s)) {
	    fprintf(stderr, _("Failed to rename %s to %s: %s\n"),
		    tmpSpecFile, s, strerror(errno));
	    unlink(tmpSpecFile);
	    xfree(specDir);
	    xfree(tmpSpecFile);
	    return 1;
	}

	/* Make the directory which contains the tarball the source 
	   directory for this run */

	if (*arg != '/') {
	    (void)getcwd(buf, BUFSIZ);
	    strcat(buf, "/");
	    strcat(buf, arg);
	} else 
	    strcpy(buf, arg);

	cmd = buf + strlen(buf) - 1;
	while (*cmd != '/') cmd--;
	*cmd = '\0';

	addMacro(NULL, "_sourcedir", NULL, buf, RMIL_TARBALL);
	xfree(specDir);
	xfree(tmpSpecFile);
	specfile = s;
    } else if (arg[0] == '/') {
	specfile = arg;
    } else {
	char *s = alloca(BUFSIZ);
	(void)getcwd(s, BUFSIZ);
	strcat(s, "/");
	strcat(s, arg);
	specfile = s;
    }

    stat(specfile, &statbuf);
    if (! S_ISREG(statbuf.st_mode)) {
	fprintf(stderr, _("File is not a regular file: %s\n"), specfile);
	return 1;
    }

    /* Try to verify that the file is actually a specfile */
    if (!isSpecFile(specfile)) {
	fprintf(stderr, _("File %s does not appear to be a specfile.\n"),
		specfile);
	return 1;
    }
    
    /* Parse the spec file */
#define	_anyarch(_f)	\
(((_f)&(RPMBUILD_PREP|RPMBUILD_BUILD|RPMBUILD_INSTALL|RPMBUILD_PACKAGEBINARY)) == 0)
    if (parseSpec(&spec, specfile, buildRoot, 0, passPhrase, cookie,
	_anyarch(buildAmount), force)) {
	    return 1;
    }
#undef	_anyarch

    /* Assemble source header from parsed components */
    initSourceHeader(spec);

    /* Check build prerequisites */
    if (!nodeps && checkSpec(spec->sourceHeader)) {
	freeSpec(spec);
	return 1;
    }

    if (buildSpec(spec, buildAmount, test)) {
	freeSpec(spec);
	return 1;
    }
    
    if (fromTarball) unlink(specfile);

    freeSpec(spec);
    
    return res;
}

int build(const char *arg, struct rpmBuildArguments *ba, const char *passPhrase,
	  int fromTarball, char *cookie, const char * rcfile, int force,
	  int nodeps)
{
    char *t, *te;
    int rc;
    char *targets = ba->targets;

    if (targets == NULL) {
	rc =  buildForTarget(arg, ba, passPhrase, fromTarball, cookie,
		force, nodeps);
	return rc;
    }

    /* parse up the build operators */

    printf(_("Building target platforms: %s\n"), targets);

    for (t = targets; *t != '\0'; t = te) {
	char *target;
	if ((te = strchr(t, ',')) == NULL)
	    te = t + strlen(t);
	target = alloca(te-t+1);
	strncpy(target, t, (te-t));
	target[te-t] = '\0';
	printf(_("Building for target %s\n"), target);

	rpmReadConfigFiles(rcfile, target);
	rc = buildForTarget(arg, ba, passPhrase, fromTarball, cookie,
		force, nodeps);
	if (rc)
	    return rc;

	freeMacros(NULL);
    }

    return 0;
}

#define	POPT_USECATALOG		1000
#define	POPT_NOLANG		1001
#define	POPT_RMSOURCE		1002
#define	POPT_RMBUILD		1003
#define	POPT_BUILDROOT		1004
#define	POPT_BUILDARCH		1005
#define	POPT_BUILDOS		1006
#define	POPT_TARGETPLATFORM	1007
#define	POPT_NOBUILD		1008
#define	POPT_SHORTCIRCUIT	1009
#define	POPT_RMSPEC		1010

extern int noLang;
static int noBuild = 0;
static int useCatalog = 0;

static void buildArgCallback(poptContext con, enum poptCallbackReason reason,
                             const struct poptOption * opt, const char * arg,
                             struct rpmBuildArguments * data)
{
    switch (opt->val) {
    case POPT_USECATALOG: data->useCatalog = 1; break;
    case POPT_NOBUILD: data->noBuild = 1; break;
    case POPT_NOLANG: data->noLang = 1; break;
    case POPT_SHORTCIRCUIT: data->shortCircuit = 1; break;
    case POPT_RMSOURCE: data->buildAmount |= RPMBUILD_RMSOURCE; break;
    case POPT_RMSPEC: data->buildAmount |= RPMBUILD_RMSPEC; break;
    case POPT_RMBUILD: data->buildAmount |= RPMBUILD_RMBUILD; break;
    case POPT_BUILDROOT:
	if (data->buildRootOverride) {
	    fprintf(stderr, _("buildroot already specified"));
	    exit(EXIT_FAILURE);
	}
	data->buildRootOverride = xstrdup(arg);
	break;
    case POPT_BUILDARCH:
	fprintf(stderr, _("--buildarch has been obsoleted.  Use the --target option instead.\n")); 
	exit(EXIT_FAILURE);
	break;
    case POPT_BUILDOS:
	fprintf(stderr, _("--buildos has been obsoleted.  Use the --target option instead.\n")); 
	exit(EXIT_FAILURE);
	break;
    case POPT_TARGETPLATFORM:
	if (data->targets) {
	    int len = strlen(data->targets) + strlen(arg) + 2;
	    data->targets = xrealloc(data->targets, len);
	    strcat(data->targets, ",");
	} else {
	    data->targets = xmalloc(strlen(arg) + 1);
	    data->targets[0] = '\0';
	}
	strcat(data->targets, arg);
	break;
    }
}

struct poptOption rpmBuildPoptTable[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA,
		buildArgCallback, 0, NULL, NULL },
	{ "buildarch", '\0', POPT_ARG_STRING, 0,  POPT_BUILDARCH,
		N_("override build architecture"), "ARCH" },
	{ "buildos", '\0', POPT_ARG_STRING, 0,  POPT_BUILDOS,
		N_("override build operating system"), "OS" },
	{ "buildroot", '\0', POPT_ARG_STRING, 0,  POPT_BUILDROOT,
		N_("override build root"), "DIRECTORY" },
	{ "clean", '\0', 0, 0, POPT_RMBUILD,
		N_("remove build tree when done"), NULL},
	{ "nobuild", '\0', 0, &noBuild,  POPT_NOBUILD,
		N_("do not execute any stages of the build"), NULL },
	{ "nolang", '\0', 0, &noLang, POPT_NOLANG,
		N_("do not accept I18N msgstr's from specfile"), NULL},
	{ "rmsource", '\0', 0, 0, POPT_RMSOURCE,
		N_("remove sources when done"), NULL},
	{ "rmspec", '\0', 0, 0, POPT_RMSPEC,
		N_("remove specfile when done"), NULL},
	{ "short-circuit", '\0', 0, 0,  POPT_SHORTCIRCUIT,
		N_("skip straight to specified stage (only for c,i)"), NULL },
	{ "target", '\0', POPT_ARG_STRING, 0,  POPT_TARGETPLATFORM,
		N_("override target platform"), "CPU-VENDOR-OS" },
	{ "usecatalog", '\0', 0, &useCatalog, POPT_USECATALOG,
		N_("lookup I18N strings in specfile catalog"), NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL }
};
