#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>

#include "rpmte.h"
#include "rpmts.h"

#include "manifest.h"
#include "misc.h"
#include "debug.h"

static int _depends_debug;

static int noAvailable = 1;
static const char * avdbpath =
	"/usr/lib/rpmdb/%{_arch}-%{_vendor}-%{_os}/redhat";
static int noChainsaw = 0;
static int noDeps = 0;

static inline /*@observer@*/ const char * const identifyDepend(int_32 f)
	/*@*/
{
    if (isLegacyPreReq(f))
	return "PreReq:";
    f = _notpre(f);
    if (f & RPMSENSE_SCRIPT_PRE)
	return "Requires(pre):";
    if (f & RPMSENSE_SCRIPT_POST)
	return "Requires(post):";
    if (f & RPMSENSE_SCRIPT_PREUN)
	return "Requires(preun):";
    if (f & RPMSENSE_SCRIPT_POSTUN)
	return "Requires(postun):";
    if (f & RPMSENSE_SCRIPT_VERIFY)
	return "Requires(verify):";
    if (f & RPMSENSE_FIND_REQUIRES)
	return "Requires(auto):";
    return "Requires:";
}

static int
do_tsort(const char *fileArgv[])
{
    const char * rootdir = "/";
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

    ts = rpmtransCreateSet(NULL, NULL);
    if (!noChainsaw)
	ts->transFlags = RPMTRANS_FLAG_CHAINSAW;

    rc = rpmtsOpenDB(ts, O_RDONLY);
    if (rc) {
	rpmMessage(RPMMESS_ERROR, _("cannot open Packages database\n"));
	rc = -1;
	goto exit;
    }

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

	rc = rpmReadPackageFile(ts, fd, *fnp, &h);
	Fclose(fd);

	if (rc == 2) {
	    numFailed++; *fnp = NULL;
	    continue;
	}

	if (rc == 0) {
	    rc = rpmtransAddPackage(ts, h, (fnpyKey)fileName, 0, NULL);
	    headerFree(h, "do_tsort"); 
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
	rpmProblem conflicts = NULL;
	int numConflicts = 0;

	rc = rpmdepCheck(ts, &conflicts, &numConflicts);

	if (conflicts) {
	    rpmMessage(RPMMESS_ERROR, _("failed dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    conflicts = rpmdepFreeConflicts(conflicts, numConflicts);
	    rc = -1;
	    goto exit;
	}
    }

    rc = rpmdepOrder(ts);
    if (rc)
	goto exit;

    {	rpmDepSet requires;
	teIterator pi; transactionElement p;
	teIterator qi; transactionElement q;
	unsigned char * selected =
			alloca(sizeof(*selected) * (ts->orderCount + 1));
	int oType = TR_ADDED;

fprintf(stdout, "digraph XXX {\n");

fprintf(stdout, "  rankdir=LR\n");

fprintf(stdout, "//===== Packages:\n");
	pi = teInitIterator(ts);
	while ((p = teNext(pi, oType)) != NULL) {
fprintf(stdout, "//%5d%5d %s\n", teGetTree(p), teGetDepth(p), teGetN(p));
	    q = teGetParent(p);
	    if (q != NULL)
fprintf(stdout, "  \"%s\" -> \"%s\"\n", teGetN(p), teGetN(q));
	    else {
fprintf(stdout, "  \"%s\"\n", teGetN(p));
fprintf(stdout, "  { rank=max ; \"%s\" }\n", teGetN(p));
	    }
	}
	pi = teFreeIterator(pi);

#ifdef	NOTNOW
fprintf(stdout, "//===== Relations:\n");
	pi = teInitIterator(ts);
	while ((p = teNext(pi, oType)) != NULL) {
	    int printed;

	    if ((requires = teGetDS(p, RPMTAG_REQUIRENAME)) == NULL)
		continue;

	    memset(selected, 0, sizeof(*selected) * ts->orderCount);
	    selected[teiGetOc(pi)] = 1;
	    printed = 0;

	    requires = dsiInit(requires);
	    while (dsiNext(requires) >= 0) {
		int_32 Flags;
		const char * qName;
		fnpyKey key;
		alKey pkgKey;
		int i;

		Flags = dsiGetFlags(requires);

		switch (teGetType(p)) {
		case TR_REMOVED:
		    /* Skip if not %preun/%postun requires or legacy prereq. */
		    if (isInstallPreReq(Flags)
#ifdef	NOTYET
		     || !( isErasePreReq(Flags)
		        || isLegacyPreReq(Flags) )
#endif
		        )
		        /*@innercontinue@*/ continue;
		    /*@switchbreak@*/ break;
		case TR_ADDED:
		    /* Skip if not %pre/%post requires or legacy prereq. */
		    if (isErasePreReq(Flags)
#ifdef	NOTYET
		     || !( isInstallPreReq(Flags)
		        || isLegacyPreReq(Flags) )
#endif
		        )
		        /*@innercontinue@*/ continue;
		    /*@switchbreak@*/ break;
		}

		if ((qName = dsiGetN(requires)) == NULL)
		    continue;	/* XXX can't happen */
		if (!strncmp(qName, "rpmlib(", sizeof("rpmlib(")-1))
		    continue;

		pkgKey = RPMAL_NOMATCH;
		key = alSatisfiesDepend(ts->addedPackages, requires, &pkgKey);
		if (pkgKey == RPMAL_NOMATCH)
		    continue;

		for (qi = teInitIterator(ts), i = 0;
		     (q = teNextIterator(qi)) != NULL; i++)
		{
		    if (teGetType(q) == TR_REMOVED)
			continue;
		    if (pkgKey == teGetAddedKey(q))
			break;
		}
		qi = teFreeIterator(qi);

		if (q == NULL || i == ts->orderCount)
		    continue;
		if (selected[i] != 0)
		    continue;
		selected[i] = 1;

		if (teGetTree(p) == teGetTree(q)) {
		    int pdepth = teGetDepth(p);
		    int qdepth = teGetDepth(q);

#if 0
		    if (pdepth == qdepth)
			continue;
		    if (pdepth < qdepth) {
			if ((qdepth - pdepth) > 1)	continue;
			if (!strcmp(teGetN(q), "glibc"))	continue;
			if (!strcmp(teGetN(q), "bash"))	continue;
		    }
#endif
		    if (pdepth > qdepth) {
			if (!strcmp(teGetN(q), "glibc"))	continue;
			if (!strcmp(teGetN(q), "bash"))		continue;
			if (!strcmp(teGetN(q), "info"))		continue;
			if (!strcmp(teGetN(q), "mktemp"))	continue;
		    }
		}

if (!printed) {
fprintf(stdout, "// %s\n", teGetN(p));
printed = 1;
}
fprintf(stdout, "//\t%s (0x%x)\n", dsDNEVR(identifyDepend(Flags), requires), Flags);
fprintf(stdout, "\t\"%s\" -> \"%s\"\n", teGetN(p), teGetN(q));

	    }

	}
	pi = teFreeIterator(pi);
#endif	/* NOTNOW */

fprintf(stdout, "}\n");

    }

    rc = 0;

exit:
    for (i = 0; i < numPkgs; i++) {
        pkgURL[i] = _free(pkgURL[i]);
    }
    pkgState = _free(pkgState);
    pkgURL = _free(pkgURL);
    argv = _free(argv);

    ts = rpmtransFree(ts);
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
	    fprintf(stderr, _("unknown popt return (%d)"), arg);
	    exit(EXIT_FAILURE);
	    /*@notreached@*/ break;
	}
    }

    rpmReadConfigFiles(NULL, NULL);

    ec = do_tsort(poptGetArgs(optCon));

    optCon = poptFreeContext(optCon);

    return ec;
}
