#include "system.h"
#include "rpmcli.h"
#include "misc.h"

#include "debug.h"

#define	UP2DATEGLOB	"/var/spool/up2date/*.rpm"

static int doit = 1;

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

time_t tid;
    rpmProblemSet probs = NULL;
    rpmTransactionSet ts = NULL;
    const char * globstr = UP2DATEGLOB;
    IDTX itids = NULL;
    IDTX rtids = NULL;
    int rc;
    int i;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    setprogname(argv[0]);       /* Retrofit glibc __progname */

    /* set up the correct locale */
    (void) setlocale(LC_ALL, "" );

#ifdef  __LCLINT__
#define LOCALEDIR       "/usr/share/locale"
#endif
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

    if (rpmReadConfigFiles(NULL, NULL))
	exit(1);

    ts = rpmtransCreateSet(NULL, NULL);
    if (rpmtsOpenDB(ts, O_RDWR))
	exit(1);

    itids = IDTXload(ts, RPMTAG_INSTALLTID);
    rtids = IDTXglob(globstr, RPMTAG_REMOVETID);

    packagesTotal = 0;

    if (rtids != NULL && rtids->nidt > 0)
    for (i = 0; i < rtids->nidt; i++) {
	IDT idt = rtids->idt + i;

fprintf(stderr, "*** rbtid %d rtid %d\n", ia->rbtid, idt->val.i32);
	if (ia->rbtid != 0 && idt->val.i32 < ia->rbtid) break;

tid = idt->val.i32;
fprintf(stderr, "\t+++ %s: %s", idt->key, ctime(&tid));

	rc = rpmtransAddPackage(ts, idt->h, idt->key,
			(ia->installInterfaceFlags & INSTALL_UPGRADE) != 0,
			ia->relocations);

	packagesTotal++;

	idt->h = headerFree(idt->h, "idt->h free");
    }

    if (itids != NULL && itids->nidt > 0)
    for (i = 0; i < itids->nidt; i++) {
	IDT idt = itids->idt + i;

fprintf(stderr, "*** rbtid %d itid %d\n", ia->rbtid, idt->val.i32);
	if (ia->rbtid != 0 && idt->val.i32 < ia->rbtid) break;

tid = idt->val.i32;
fprintf(stderr, "\t--- rpmdb instance #%u: %s", idt->instance, ctime(&tid));

	rc = rpmtransRemovePackage(ts, idt->h, idt->instance);

	packagesTotal++;

    }

    rc = rpmdepOrder(ts);
    if (rc)
	doit = 0;

    probs = NULL;
rc = 0;
if (doit)
    rc = rpmRunTransactions(ts,  rpmShowProgress,
		(void *) ((long)ia->installInterfaceFlags),
		NULL, &probs, ia->transFlags, ia->probFilter);
    if (rc > 0)
	rpmProblemSetPrint(stderr, probs);
    if (probs != NULL) rpmProblemSetFree(probs);

    itids = IDTXfree(itids);
    rtids = IDTXfree(rtids);
    ts = rpmtransFree(ts);

    optCon = poptFreeContext(optCon);
    rpmFreeMacros(NULL);

#if HAVE_MCHECK_H && HAVE_MTRACE
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    return ec;
}
