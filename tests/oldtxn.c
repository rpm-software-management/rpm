#include <stdio.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>

int main(int argc, char *argv[])
{
    int rc = EXIT_FAILURE;
    if (rpmReadConfigFiles(NULL, NULL) == 0) {
	rpmts ts = rpmtsCreate();
	/* XXX it's 2026 and this is still needed :facepalm: */
	rpmtsSetRootDir(ts, NULL);
	rpmtxn txn = rpmtxnBegin(ts, RPMTXN_WRITE);

	if (txn) {
	    rpmSetVerbosity(RPMLOG_DEBUG);
	    rc = rpmtxnRebuildKeystore(txn, NULL);
	    rpmSetVerbosity(RPMLOG_NOTICE);
	    rpmtxnEnd(txn);
	}
	rpmtsFree(ts);
    }

    return rc;
}
