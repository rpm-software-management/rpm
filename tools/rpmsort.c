#include "system.h"

#include "rpmbuild.h"
#include "buildio.h"

#include "depends.h"
#include "header.h"

extern int _depends_debug;

static int
do_tsort(const char *fileArgv[])
{
    const char * rootdir = "/";
    const char ** fileURL;
    int numPkgs;
    int numFailed = 0;
    rpmdb rpmdb = NULL;
    rpmTransactionSet ts = NULL;
    Header h;
    int rc;

    if (fileArgv == NULL)
	return 0;

    if (rpmdbOpen(rootdir, &rpmdb, O_RDONLY, 0644)) {
	rpmMessage(RPMMESS_ERROR, _("cannot open Packages database\n"));
	rc = -1;
	goto exit;
    }

    ts = rpmtransCreateSet(rpmdb, rootdir);

    for (fileURL = fileArgv, numPkgs = 0; *fileURL; fileURL++, numPkgs++)
	;

    for (fileURL = fileArgv; *fileURL; fileURL++) {
	const char * fileName;
	FD_t fd;

	(void) urlPath(*fileURL, &fileName);
	fd = Fopen(*fileURL, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open file %s: %s\n"), *fileURL,
		Fstrerror(fd));
	    if (fd) Fclose(fd);
	    numFailed++;
            continue;
        }

	rc = rpmReadPackageHeader(fd, &h, NULL, NULL, NULL);

	rc = rpmtransAddPackage(ts, h, NULL, fileName, 0, NULL);

	headerFree(h);  /* XXX reference held by transaction set */
	Fclose(fd);

    }

    {	struct rpmDependencyConflict * conflicts = NULL;
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
		const char *n, *v, *r;
		headerNVR(h, &n, &v, &r);
		fprintf(stdout, "%s %s-%s-%s\n", str, n, v, r);
		headerFree(h);
	    }

	}
    }

    rc = 0;

exit:
    if (ts) {
	rpmtransFree(ts);
	ts = NULL;
    }
    if (rpmdb) {
	rpmdbClose(rpmdb);
	rpmdb = NULL;
    }
    return rc;
}

static struct poptOption optionsTable[] = {
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
