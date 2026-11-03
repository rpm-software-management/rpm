/* Print the problems recorded on a transaction after running it:
 * the scriptlet failures of the transaction, retrievable through
 * the rpmte/rpmts problem APIs.
 *
 * Usage: scriptletprobs <pkgfile> [macro value ...]
 */
#include <stdlib.h>
#include <stdio.h>

#include <rpm/header.h>
#include <rpm/rpmio.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmprob.h>
#include <rpm/rpmps.h>
#include <rpm/rpmte.h>
#include <rpm/rpmts.h>

static void printProblems(rpmps ps)
{
    rpmProblem p;
    rpmpsi psi = rpmpsInitIterator(ps);
    while ((p = rpmpsiNext(psi)) != NULL) {
	if (rpmProblemGetType(p) == RPMPROB_SCRIPTLET_ERROR) {
	    printf("problem scriptlet-error %s: %s\n",
		   rpmProblemGetPkgNEVR(p), rpmProblemGetStr(p));
	} else {
	    char *msg = rpmProblemString(p);
	    printf("problem other-%d %s: %s\n", rpmProblemGetType(p),
		   rpmProblemGetPkgNEVR(p), msg);
	    free(msg);
	}
    }
    rpmpsFreeIterator(psi);
}


/* Minimal notify callback: librpm needs INST_OPEN_FILE served to
 * verify package files during the transaction (mirrors the CLI) */
static void * notifyCB(const void * h, const rpmCallbackType what,
		       const rpm_loff_t amount, const rpm_loff_t total,
		       fnpyKey key, void * data)
{
    static FD_t fd = NULL;

    switch (what) {
    case RPMCALLBACK_INST_OPEN_FILE:
	if (key == NULL || ((const char *)key)[0] == '\0')
	    return NULL;
	fd = Fopen((const char *)key, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    if (fd != NULL) {
		Fclose(fd);
		fd = NULL;
	    }
	} else
	    fd = fdLink(fd);
	return (void *)fd;
    case RPMCALLBACK_INST_CLOSE_FILE:
	fd = fdFree(fd);
	if (fd != NULL) {
	    Fclose(fd);
	    fd = NULL;
	}
	break;
    default:
	break;
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int rc = EXIT_FAILURE;
    rpmts ts = NULL;
    FD_t fd = NULL;
    Header h = NULL;

    if (argc < 2)
	goto exit;

    if (rpmReadConfigFiles(NULL, NULL))
	goto exit;

    /* Remaining arguments are macro name/value pairs */
    for (int i = 2; i + 1 < argc; i += 2)
	rpmPushMacro(NULL, argv[i], NULL, argv[i + 1], RMIL_GLOBAL);

    /* Keep stdout deterministic: the test asserts on the problem
     * data, the log output is covered by the CLI tests */
    rpmSetVerbosity(RPMLOG_EMERG);

    ts = rpmtsCreate();
    rpmtsSetRootDir(ts, NULL);
    rpmtsSetFlags(ts, RPMTRANS_FLAG_NOPLUGINS);
    rpmtsSetNotifyCallback(ts, notifyCB, NULL);

    fd = Fopen(argv[1], "r.ufdio");
    if (fd == NULL)
	goto exit;
    if (rpmReadPackageFile(ts, fd, argv[1], &h) != RPMRC_OK)
	goto exit;
    Fclose(fd);
    fd = NULL;

    if (rpmtsAddInstallElement(ts, h, (fnpyKey)argv[1], 1, NULL))
	goto exit;
    headerFree(h);
    h = NULL;

    rpmtsCheck(ts);
    rpmtsOrder(ts);
    printf("run: %d\n", rpmtsRun(ts, NULL, RPMPROB_FILTER_NONE));

    rpmte te;
    rpmtsi tsi = rpmtsiInit(ts);
    while ((te = rpmtsiNext(tsi, 0)) != NULL) {
	rpmps eps = rpmteProblems(te);
	printf("element %s: %d problems\n",
	       rpmteNEVRA(te), rpmpsNumProblems(eps));
	rpmpsFree(eps);
    }
    rpmtsiFree(tsi);

    rpmps ps = rpmtsProblems(ts);
    if (ps)
	printProblems(ps);
    printf("transaction: %d problems\n", rpmpsNumProblems(ps));
    rpmpsFree(ps);

    rc = EXIT_SUCCESS;

exit:
    if (fd)
	Fclose(fd);
    if (h)
	headerFree(h);
    rpmtsFree(ts);
    return rc;
}
