#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>

#include "depends.h"

#include "manifest.h"
#include "misc.h"
#include "debug.h"

extern int _depends_debug;

static int noAvailable = 0;
static const char * avdbpath =
	"/usr/lib/rpmdb/%{_arch}-%{_vendor}-%{_os}/redhat";
static int noChainsaw = 0;
static int noDeps = 0;

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param this		memory to free
 * @retval		NULL always
 */
static /*@null@*/ void * _free(/*@only@*/ /*@null@*/ const void * this) {
    if (this)   free((void *)this);
    return NULL;
}

static int
do_tsort(const char *fileArgv[])
{
    const char * rootdir = "/";
    rpmdb rpmdb = NULL;
    rpmTransactionSet ts = NULL;
    const char ** pkgURL = NULL;
    char * pkgState = NULL;
    const char ** fnp;
    const char * fileURL = NULL;
    int numPkgs = 0;
    int numFailed = 0;
    int prevx;
    int pkgx;
    const char ** argv = NULL;
    int argc = 0;
    const char ** av = NULL;
    int ac = 0;
    Header h;
    int rc = 0;
    int i;

    if (fileArgv == NULL)
	return 0;

    rc = rpmdbOpen(rootdir, &rpmdb, O_RDONLY, 0644);
    if (rc) {
	rpmMessage(RPMMESS_ERROR, _("cannot open Packages database\n"));
	rc = -1;
	goto exit;
    }

    ts = rpmtransCreateSet(rpmdb, rootdir);
    if (!noChainsaw)
	ts->transFlags = RPMTRANS_FLAG_CHAINSAW;

    /* Load all the available packages. */
    if (!(noDeps || noAvailable)) {
	rpmdbMatchIterator mi = NULL;
	struct rpmdb_s * avdb = NULL;

	addMacro(NULL, "_dbpath", NULL, avdbpath, RMIL_CMDLINE);
	rc = rpmdbOpen(rootdir, &avdb, O_RDONLY, 0644);
	delMacro(NULL, "_dbpath");
	if (rc) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open Available database\n"));
	    goto endavail;
	}
        mi = rpmdbInitIterator(avdb, RPMDBI_PACKAGES, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    rpmtransAvailablePackage(ts, h, NULL);
	}

endavail:
	if (mi) rpmdbFreeIterator(mi);
	if (avdb) rpmdbClose(avdb);
    }

    /* Build fully globbed list of arguments in argv[argc]. */
    for (fnp = fileArgv; *fnp; fnp++) {
	av = _free(av);
	ac = 0;
	rc = rpmGlob(*fnp, &ac, &av);
	if (rc || ac == 0) continue;

	argv = (argc == 0)
	    ? xmalloc((argc+2) * sizeof(*argv))
	    : xrealloc(argv, (argc+2) * sizeof(*argv));
	memcpy(argv+argc, av, ac * sizeof(*av));
	argc += ac;
	argv[argc] = NULL;
    }
    av = _free(av);

    numPkgs = 0;
    prevx = 0;
    pkgx = 0;

restart:
    /* Allocate sufficient storage for next set of args. */
    if (pkgx >= numPkgs) {
	numPkgs = pkgx + argc;
	pkgURL = (pkgURL == NULL)
	    ? xmalloc( (numPkgs + 1) * sizeof(*pkgURL))
	    : xrealloc(pkgURL, (numPkgs + 1) * sizeof(*pkgURL));
	memset(pkgURL + pkgx, 0, ((argc + 1) * sizeof(*pkgURL)));
	pkgState = (pkgState == NULL)
	    ? xmalloc( (numPkgs + 1) * sizeof(*pkgState))
	    : xrealloc(pkgState, (numPkgs + 1) * sizeof(*pkgState));
	memset(pkgState + pkgx, 0, ((argc + 1) * sizeof(*pkgState)));
    }

    /* Copy next set of args. */
    for (i = 0; i < argc; i++) {
	fileURL = _free(fileURL);
	fileURL = argv[i];
	argv[i] = NULL;
	pkgURL[pkgx] = fileURL;
	fileURL = NULL;
	pkgx++;
    }
    fileURL = _free(fileURL);

    /* Continue processing file arguments, building transaction set. */
    for (fnp = pkgURL+prevx; *fnp; fnp++, prevx++) {
	const char * fileName;
	int isSource;
	FD_t fd;

	(void) urlPath(*fnp, &fileName);

	/* Try to read the header from a package file. */
	fd = Fopen(*fnp, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) Fclose(fd);
	    numFailed++; *fnp = NULL;
	    continue;
	}

	rc = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);
	Fclose(fd);

	if (rc == 2) {
	    numFailed++; *fnp = NULL;
	    continue;
	}

	if (rc == 0) {
	    rc = rpmtransAddPackage(ts, h, NULL, fileName, 0, NULL);
	    headerFree(h);  /* XXX reference held by transaction set */
	    continue;
	}

	if (rc != 1) {
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *fnp);
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Try to read a package manifest. */
	fd = Fopen(*fnp, "r.fpio");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) Fclose(fd);
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Read list of packages from manifest. */
	rc = rpmReadPackageManifest(fd, &argc, &argv);
	if (rc)
	    rpmError(RPMERR_MANIFEST, _("%s: read manifest failed: %s\n"),
			fileURL, Fstrerror(fd));
	Fclose(fd);

	/* If successful, restart the query loop. */
	if (rc == 0) {
	    prevx++;
	    goto restart;
	}

	numFailed++; *fnp = NULL;
	break;
    }

    if (numFailed) goto exit;

    if (!noDeps) {
	struct rpmDependencyConflict * conflicts = NULL;
	int numConflicts = 0;

	rc = rpmdepCheck(ts, &conflicts, &numConflicts);

	if (conflicts) {
	    rpmMessage(RPMMESS_ERROR, _("failed dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    rpmdepFreeConflicts(conflicts, numConflicts);
	    rc = -1;
	    goto exit;
	}
    }

    rc = rpmdepOrder(ts);
    if (rc)
	goto exit;

    {	int oc;
	for (oc = 0; oc < ts->orderCount; oc++) {
	    struct availablePackage *alp;
	    rpmdbMatchIterator mi;
	    const char * str;
	    int i;

	    alp = NULL;
	    str = "???";
	    switch (ts->order[oc].type) {
	    case TR_ADDED:
		i = ts->order[oc].u.addedIndex;
		alp = ts->addedPackages.list + ts->order[oc].u.addedIndex;
		h = headerLink(alp->h);
		str = "+++";
		break;
	    case TR_REMOVED:
		i = ts->order[oc].u.removed.dboffset;
		mi = rpmdbInitIterator(ts->rpmdb, RPMDBI_PACKAGES, &i, sizeof(i));
		h = rpmdbNextIterator(mi);
		if (h)
		    h = headerLink(h);
		rpmdbFreeIterator(mi);
		str = "---";
		break;
	    }

	    if (h) {
		if (alp && alp->key) {
		    const char * fn = alp->key;
		    fprintf(stdout, "%s %s\n", str, fn);
		} else {
		    const char *n, *v, *r;
		    headerNVR(h, &n, &v, &r);
		    fprintf(stdout, "%s %s-%s-%s\n", str, n, v, r);
		}
		headerFree(h);
	    }

	}
    }

    rc = 0;

exit:
    if (ts) rpmtransFree(ts);
    for (i = 0; i < numPkgs; i++) {
        pkgURL[i] = _free(pkgURL[i]);
    }
    pkgState = _free(pkgState);
    pkgURL = _free(pkgURL);
    argv = _free(argv);
    if (rpmdb) rpmdbClose(rpmdb);
    return rc;
}

static struct poptOption optionsTable[] = {
 { "noavailable", '\0', 0, &noAvailable, 0,	NULL, NULL},
 { "nochainsaw", '\0', 0, &noChainsaw, 0,	NULL, NULL},
 { "nodeps", '\0', 0, &noDeps, 0,		NULL, NULL},
 { "verbose", 'v', 0, 0, 'v',			NULL, NULL},
 { NULL,	0, 0, 0, 0,			NULL, NULL}
};

int
main(int argc, const char *argv[])
{
    poptContext optCon;
    const char * optArg;
    int arg;
    int ec = 0;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();	/* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    (void)setlocale(LC_ALL, "" );

#ifdef  __LCLINT__
#define LOCALEDIR	"/usr/share/locale"
#endif
    (void)bindtextdomain(PACKAGE, LOCALEDIR);
    (void)textdomain(PACKAGE);

    _depends_debug = 1;

    optCon = poptGetContext("rpmsort", argc, argv, optionsTable, 0);
    poptReadDefaultConfig(optCon, 1);

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);
	switch (arg) {
	case 'v':
	    rpmIncreaseVerbosity();
	    break;
	default:
	    errx(EXIT_FAILURE, _("unknown popt return (%d)"), arg);
	    /*@notreached@*/ break;
	}
    }

    rpmReadConfigFiles(NULL, NULL);

    ec = do_tsort(poptGetArgs(optCon));

    poptFreeContext(optCon);

    return ec;
}
