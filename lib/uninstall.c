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

#define	SUFFIX_RPMSAVE	".rpmsave"

/**
 * Remove (or rename) file according to file disposition.
 * @param fi		transaction element file info
 * @param i		file info index
 * @param file		file name (root prepended)
 * @return
 */
static int removeFile(TFI_t fi, int i, const char * ofn)
{
    int rc = 0;
	
    switch (fi->actions[i]) {

    case FA_BACKUP:
    {	char * nfn = alloca(strlen(ofn) + sizeof(SUFFIX_RPMSAVE));
	(void)stpcpy(stpcpy(nfn, ofn), SUFFIX_RPMSAVE);

	if (rename(ofn, nfn)) {
	    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s\n"),
			ofn, nfn, strerror(errno));
	    rc = 1;
	}
    }	break;

    case FA_REMOVE:
	if (S_ISDIR(fi->fmodes[i])) {
	    /* XXX should permit %missingok for directories as well */
	    if (rmdir(ofn)) {
		switch (errno) {
		case ENOENT:	/* XXX rmdir("/") linux 2.2.x kernel hack */
		case ENOTEMPTY:
		    rpmError(RPMERR_RMDIR, 
			_("cannot remove %s - directory not empty\n"), 
			ofn);
		    break;
		default:
		    rpmError(RPMERR_RMDIR, _("rmdir of %s failed: %s\n"),
				ofn, strerror(errno));
		    break;
		}
		rc = 1;
	    }
	} else {
	    if (unlink(ofn)) {
		if (errno != ENOENT || !(fi->fflags[i] & RPMFILE_MISSINGOK)) {
		    rpmError(RPMERR_UNLINK, 
			      _("removal of %s failed: %s\n"),
				ofn, strerror(errno));
		}
		rc = 1;
	    }
	}
	break;
    case FA_UNKNOWN:
    case FA_CREATE:
    case FA_SAVE:
    case FA_ALTNAME:
    case FA_SKIP:
    case FA_SKIPNSTATE:
    case FA_SKIPNETSHARED:
    case FA_SKIPMULTILIB:
	break;
    }
 
    return 0;
}

int removeBinaryPackage(const rpmTransactionSet ts, TFI_t fi)
{
    static char * stepName = "erase";
    Header h;
    const void * pkgKey = NULL;
    int rc = 0;
    int i;

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

    if (!(ts->transFlags & RPMTRANS_FLAG_TEST)) {
	rc = runInstScript(ts, h, RPMTAG_PREUN, RPMTAG_PREUNPROG, fi->scriptArg,
		          (ts->transFlags & RPMTRANS_FLAG_NOSCRIPTS));
	if (rc)
	    return 1;
    }
    if (!(ts->transFlags & RPMTRANS_FLAG_JUSTDB) && fi->fc > 0) {
	int rdlen = (ts->rootDir && !(ts->rootDir[0] == '/' && ts->rootDir[1] == '\0'))
			? strlen(ts->rootDir) : 0;
	int fnmaxlen = fi->dnlmax + fi->bnlmax + rdlen + sizeof("/") - 1;
	char * ofn = alloca(fnmaxlen);

	*ofn = '\0';
	if (rdlen) {
	    strcpy(ofn, ts->rootDir);
	    (void) rpmCleanPath(ofn);
	    rdlen = strlen(ofn);
	}

	if (ts->notify)
	    (void)ts->notify(h, RPMCALLBACK_UNINST_START, fi->fc, fi->fc,
		pkgKey, ts->notifyData);

	/* Traverse filelist backwards to help insure that rmdir() will work. */
	for (i = fi->fc - 1; i >= 0; i--) {

	    /* XXX this assumes that dirNames always starts/ends with '/' */
	    (void) stpcpy( stpcpy(ofn+rdlen, fi->dnl[fi->dil[i]]), fi->bnl[i]);

	    rpmMessage(RPMMESS_DEBUG, _("   file: %s action: %s\n"),
			ofn, fileActionString(fi->actions[i]));

	    if (ts->transFlags & RPMTRANS_FLAG_TEST)
		continue;
	    if (ts->notify)
		(void) ts->notify(h, RPMCALLBACK_UNINST_PROGRESS, i,
			fi->actions[i], ofn, ts->notifyData);
	    removeFile(fi, i, ofn);
	}

	if (ts->notify)
	    (void)ts->notify(h, RPMCALLBACK_UNINST_STOP, 0, fi->fc,
			pkgKey, ts->notifyData);
    }

    if (!(ts->transFlags & RPMTRANS_FLAG_TEST)) {
	rpmMessage(RPMMESS_DEBUG, _("%s: running %s script(s) (if any)\n"),
		stepName, "post-erase");

	rc = runInstScript(ts, h, RPMTAG_POSTUN, RPMTAG_POSTUNPROG,
			fi->scriptArg, (ts->transFlags & RPMTRANS_FLAG_NOSCRIPTS));
	/* XXX postun failures are not cause for erasure failure. */
    }

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
