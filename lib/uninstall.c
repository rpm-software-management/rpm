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

#define	SUFFIX_RPMORIG	".rpmorig"
#define	SUFFIX_RPMSAVE	".rpmsave"
#define	SUFFIX_RPMNEW	".rpmnew"

/**
 * Perform erase file actions.
 * @todo Permit missingok for dirs, add %missingok VFA_t when building.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @return		0 on success, 1 on failure
 */
static int eraseActions(const rpmTransactionSet ts, TFI_t fi)
{
/*@observer@*/ static char * stepName = "erase";
    int nb = (!ts->chrootDone ? strlen(ts->rootDir) : 0);
    char * opath = alloca(nb + fi->dnlmax + fi->bnlmax + 64);
    char * o = (!ts->chrootDone ? stpcpy(opath, ts->rootDir) : opath);
    char * npath = alloca(nb + fi->dnlmax + fi->bnlmax + 64);
    char * n = (!ts->chrootDone ? stpcpy(npath, ts->rootDir) : npath);
    int rc = 0;
    int i;

    if (fi->actions == NULL)
	return rc;

    /* Traverse filelist backwards to help insure that rmdir() will work. */
    for (i = fi->fc - 1; i >= 0; i--) {

	rpmMessage(RPMMESS_DEBUG, _("   file: %s action: %s\n"),
			o, fileActionString(fi->actions[i]));

	if (ts->transFlags & RPMTRANS_FLAG_TEST)
	    continue;

	if (ts->notify)
	    (void) ts->notify(fi->h, RPMCALLBACK_UNINST_PROGRESS, i,
			fi->actions[i], o, ts->notifyData);

	switch (fi->actions[i]) {
	case FA_ALTNAME:
	case FA_SAVE:
	case FA_CREATE:
	case FA_SKIP:
	case FA_SKIPMULTILIB:
	case FA_SKIPNSTATE:
	case FA_SKIPNETSHARED:
	case FA_UNKNOWN:
	    continue;
	    /*@notreached@*/ break;

	case FA_BACKUP:
	    /* Append file name to (possible) root dir. */
	    (void) stpcpy( stpcpy(o, fi->dnl[fi->dil[i]]), fi->bnl[i]);
	    (void) stpcpy( stpcpy(n, o), SUFFIX_RPMSAVE);
	    rpmMessage(RPMMESS_WARNING, _("%s saved as %s\n"), o, n);
	    if (!rename(opath, npath))
		continue;
	    rpmError(RPMERR_RENAME, _("%s rename of %s to %s failed: %s\n"),
			stepName, o, n, strerror(errno));
	    rc = 1;
	    break;

	case FA_REMOVE:
	    /* Append file name to (possible) root dir. */
	    (void) stpcpy( stpcpy(o, fi->dnl[fi->dil[i]]), fi->bnl[i]);
	    if (S_ISDIR(fi->fmodes[i])) {
		if (!rmdir(opath))
		    continue;
		switch (errno) {
		case ENOENT: /* XXX rmdir("/") linux 2.2.x kernel hack */
		case ENOTEMPTY:
#ifdef	NOTYET
		    if (fi->fflags[i] & RPMFILE_MISSINGOK)
			continue;
#endif
		    rpmError(RPMERR_RMDIR, 
				_("cannot remove %s - directory not empty\n"), 
				o);
		    break;
		default:
		    rpmError(RPMERR_RMDIR,
				_("rmdir of %s failed: %s\n"),
				o, strerror(errno));
		    break;
		}
		rc = 1;
		break;
	    }
	    if (!unlink(opath))
		continue;
	    if (errno == ENOENT && (fi->fflags[i] & RPMFILE_MISSINGOK))
		continue;
	    rpmError(RPMERR_UNLINK, _("removal of %s failed: %s\n"),
				o, strerror(errno));
	    rc = 1;
	    break;
	}
    }
    return rc;
}

int removeBinaryPackage(const rpmTransactionSet ts, TFI_t fi)
{
/*@observer@*/ static char * stepName = "erase";
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

	rc = eraseActions(ts, fi);

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
