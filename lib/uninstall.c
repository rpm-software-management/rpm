/** \ingroup rpmtrans payload
 * \file lib/uninstall.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmExpand */

#include "rollback.h"
#include "misc.h"	/* XXX for makeTempFile, doputenv */
#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */

int removeBinaryPackage(const rpmTransactionSet ts, TFI_t fi)
{
/*@observer@*/ static char * stepName = "   erase";
    Header h;
    const void * pkgKey = NULL;
    int rc = 0;

    rpmMessage(RPMMESS_DEBUG, _("%s: %s-%s-%s has %d files, test = %d\n"),
		stepName, fi->name, fi->version, fi->release,
		fi->fc, (ts->transFlags & RPMTRANS_FLAG_TEST));

    /*
     * When we run scripts, we pass an argument which is the number of 
     * versions of this package that will be installed when we are finished.
     */
    fi->scriptArg = rpmdbCountPackages(ts->rpmdb, fi->name) - 1;
    if (fi->scriptArg < 0)
	return 1;

    {	rpmdbMatchIterator mi = NULL;

	mi = rpmdbInitIterator(ts->rpmdb, RPMDBI_PACKAGES,
				&fi->record, sizeof(fi->record));

	h = rpmdbNextIterator(mi);
	if (h == NULL) {
	    rpmdbFreeIterator(mi);
	    return 2;
	}
	/* XXX was headerLink but iterators destroy dbh data. */
	h = headerCopy(h);
	rpmdbFreeIterator(mi);
    }

    if (!(ts->transFlags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* run triggers from this package which are keyed on installed 
	   packages */
	if (runImmedTriggers(ts, RPMSENSE_TRIGGERUN, h, -1))
	    return 2;

	/* run triggers which are set off by the removal of this package */
	if (runTriggers(ts, RPMSENSE_TRIGGERUN, h, -1))
	    return 1;
    }

    rpmMessage(RPMMESS_DEBUG, _("%s: running %s script(s) (if any)\n"),
		stepName, "pre-erase");

    rc = runInstScript(ts, h, RPMTAG_PREUN, RPMTAG_PREUNPROG, fi->scriptArg,
		          (ts->transFlags & RPMTRANS_FLAG_NOSCRIPTS));
    if (rc)
	return 1;

    if (fi->fc > 0 && !(ts->transFlags & RPMTRANS_FLAG_JUSTDB)) {

	if (ts->notify)
	    (void)ts->notify(h, RPMCALLBACK_UNINST_START, fi->fc, fi->fc,
		pkgKey, ts->notifyData);

	rc = pkgActions(ts, fi, FSM_COMMIT);

	if (ts->notify)
	    (void)ts->notify(h, RPMCALLBACK_UNINST_STOP, 0, fi->fc,
			pkgKey, ts->notifyData);
    }

    rpmMessage(RPMMESS_DEBUG, _("%s: running %s script(s) (if any)\n"),
		stepName, "post-erase");

    rc = runInstScript(ts, h, RPMTAG_POSTUN, RPMTAG_POSTUNPROG,
		fi->scriptArg, (ts->transFlags & RPMTRANS_FLAG_NOSCRIPTS));
    /* XXX postun failures are not cause for erasure failure. */

    if (!(ts->transFlags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* Run postun triggers which are set off by this package's removal. */
	rc = runTriggers(ts, RPMSENSE_TRIGGERPOSTUN, h, -1);
	if (rc)
	    return 2;
    }

    if (!(ts->transFlags & RPMTRANS_FLAG_TEST))
	rpmdbRemove(ts->rpmdb, ts->id, fi->record);

    return 0;
}
