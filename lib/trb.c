#include "system.h"
#include "rpmcli.h"
#include "misc.h"

#include "debug.h"

#define	UP2DATEGLOB	"/var/spool/up2date/*.rpm"

#ifdef	DYING
static int XrpmRollback(struct rpmInstallArguments_s * ia, const char ** argv)
{
    rpmdb db = NULL;
    rpmTransactionSet ts = NULL;
    rpmProblemSet probs = NULL;
    IDTX itids = NULL;
    IDTX rtids = NULL;
    unsigned thistid = 0xffffffff;
    unsigned prevtid;
    time_t tid;
    IDT rp;
    IDT ip;
    int rc;

    if (argv != NULL && *argv != NULL) {
	rc = -1;
	goto exit;
    }

    rc = rpmdbOpen(ia->rootdir, &db, O_RDWR, 0644);
    if (rc != 0)
	goto exit;

    itids = IDTXload(db, RPMTAG_INSTALLTID);
    ip = (itids != NULL && itids->nidt > 0) ? itids->idt : NULL;

    {	const char * globstr = rpmExpand("%{_repackage_dir}/*.rpm", NULL);
	if (globstr == NULL || *globstr == '%') {
	    globstr = _free(globstr);
	    rc = -1;
	    goto exit;
	}
	rtids = IDTXglob(globstr, RPMTAG_REMOVETID);
	rp = (rtids != NULL && rtids->nidt > 0) ? rtids->idt : NULL;
	globstr = _free(globstr);
    }

    /* Run transactions until rollback goal is achieved. */
    do {
	prevtid = thistid;
	rc = 0;
	packagesTotal = 0;

	/* Find larger of the remaining install/erase transaction id's. */
	thistid = 0;
	if (ip != NULL && ip->val.u32 > thistid)
	    thistid = ip->val.u32;
	if (rp != NULL && rp->val.u32 > thistid)
	    thistid = rp->val.u32;

	/* If we've achieved the rollback goal, then we're done. */
	if (thistid == 0 || thistid < ia->rbtid)
	    break;

	ts = rpmtransCreateSet(db, ia->rootdir);

	/* Install the previously erased packages for this transaction. */
	while (rp != NULL && rp->val.u32 == thistid) {

	    rpmMessage(RPMMESS_DEBUG, "\t+++ %s\n", rp->key);

	    rc = rpmtransAddPackage(ts, rp->h, NULL, rp->key,
			(ia->installInterfaceFlags & INSTALL_UPGRADE) != 0,
			ia->relocations);
	    if (rc != 0)
		goto exit;

	    packagesTotal++;

	    rp->h = headerFree(rp->h);
	    rtids->nidt--;
	    if (rtids->nidt > 0)
		rp++;
	    else
		rp = NULL;
	}

	/* Erase the previously installed packages for this transaction. */
	while (ip != NULL && ip->val.u32 == thistid) {

	    rpmMessage(RPMMESS_DEBUG,
			"\t--- rpmdb instance #%u\n", ip->instance);

	    rc = rpmtransRemovePackage(ts, ip->instance);
	    if (rc != 0)
		goto exit;

	    packagesTotal++;

	    ip->instance = 0;
	    itids->nidt--;
	    if (itids->nidt > 0)
		ip++;
	    else
		ip = NULL;
	}

	/* Anything to do? */
	if (packagesTotal <= 0)
	    break;

	tid = (time_t)thistid;
	rpmMessage(RPMMESS_DEBUG, _("rollback %d packages to %s"),
			packagesTotal, ctime(&tid));

	rc = rpmdepOrder(ts);
	if (rc != 0)
	    goto exit;

	probs = NULL;
	rc = rpmRunTransactions(ts,  rpmShowProgress,
		(void *) ((long)ia->installInterfaceFlags),
		NULL, &probs, ia->transFlags,
		(ia->probFilter|RPMPROB_FILTER_OLDPACKAGE));
	if (rc > 0) {
	    rpmProblemSetPrint(stderr, probs);
	    goto exit;
	}

	if (probs != NULL) {
	    rpmProblemSetFree(probs);
	    probs = NULL;
	}
	ts = rpmtransFree(ts);

    } while (1);

exit:
    if (probs != NULL) {
	rpmProblemSetFree(probs);
	probs = NULL;
    }
    ts = rpmtransFree(ts);

    if (db != NULL) (void) rpmdbClose(db);

    rtids = IDTXfree(rtids);
    itids = IDTXfree(itids);

    return rc;
}
#endif

static struct poptOption optionsTable[] = {
 { "verbose", 'v', 0, 0, 'v',
	N_("provide more detailed output"), NULL},
 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
	N_("Install/Upgrade/Erase options:"),
	NULL },
   POPT_AUTOHELP
   POPT_TABLEEND
};

int
main(int argc, const char *argv[])
{
    poptContext optCon;
    struct rpmInstallArguments_s * ia = &rpmIArgs;
    int arg;
    int ec = 0;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);       /* Retrofit glibc __progname */

    /* set up the correct locale */
    (void) setlocale(LC_ALL, "" );

    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    rpmSetVerbosity(RPMMESS_NORMAL);

    optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    (void) poptReadConfigFile(optCon, LIBRPMALIAS_FILENAME);
    (void) poptReadDefaultConfig(optCon, 1);
    poptSetExecPath(optCon, RPMCONFIGDIR, 1);

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	switch(arg) {
	case 'v':
	    rpmIncreaseVerbosity();
	    break;
	default:
	    break;
	}
    }

    if (ia->rbtid == 0) {
	fprintf(stderr, "--rollback <timestamp> is required\n");
	exit(1);
    }

    if (rpmReadConfigFiles(NULL, NULL))
	exit(1);

    ec = rpmRollback(ia, NULL);

    optCon = poptFreeContext(optCon);
    rpmFreeMacros(NULL);

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    return ec;
}
