/** \ingroup rpmtrans payload
 * \file lib/uninstall.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmExpand */

#include "depends.h"	/* XXX for headerMatchesDepFlags */
#include "install.h"
#include "misc.h"	/* XXX for makeTempFile, doputenv */
#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */

static char * SCRIPT_PATH = "PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin";

#define	SUFFIX_RPMSAVE	".rpmsave"

/**
 * Remove (or rename) file according to file disposition.
 * @param file		file
 * @param fileAttrs	file attributes (from package header)
 * @param mode		file type
 * @param action	file disposition
 * @return
 */
static int removeFile(const char * file, rpmfileAttrs fileAttrs, short mode, 
		      enum fileActions action)
{
    int rc = 0;
    char * newfile;
	
    switch (action) {

    case FA_BACKUP:
	newfile = alloca(strlen(file) + sizeof(SUFFIX_RPMSAVE));
	(void)stpcpy(stpcpy(newfile, file), SUFFIX_RPMSAVE);

	if (rename(file, newfile)) {
	    rpmError(RPMERR_RENAME, _("rename of %s to %s failed: %s\n"),
			file, newfile, strerror(errno));
	    rc = 1;
	}
	break;

    case FA_REMOVE:
	if (S_ISDIR(mode)) {
	    /* XXX should permit %missingok for directories as well */
	    if (rmdir(file)) {
		switch (errno) {
		case ENOENT:	/* XXX rmdir("/") linux 2.2.x kernel hack */
		case ENOTEMPTY:
		    rpmError(RPMERR_RMDIR, 
			_("cannot remove %s - directory not empty\n"), 
			file);
		    break;
		default:
		    rpmError(RPMERR_RMDIR, _("rmdir of %s failed: %s\n"),
				file, strerror(errno));
		    break;
		}
		rc = 1;
	    }
	} else {
	    if (unlink(file)) {
		if (errno != ENOENT || !(fileAttrs & RPMFILE_MISSINGOK)) {
		    rpmError(RPMERR_UNLINK, 
			      _("removal of %s failed: %s\n"),
				file, strerror(errno));
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

int removeBinaryPackage(const rpmTransactionSet ts, unsigned int offset,
		Header h, const void * pkgKey, enum fileActions * actions)
{
    rpmtransFlags transFlags = ts->transFlags;
    const char * name, * version, * release;
    const char ** baseNames;
    int scriptArg;
    int rc = 0;
    int fileCount;
    int i;

    if (transFlags & RPMTRANS_FLAG_JUSTDB)
	transFlags |= RPMTRANS_FLAG_NOSCRIPTS;

    headerNVR(h, &name, &version, &release);

    /*
     * When we run scripts, we pass an argument which is the number of 
     * versions of this package that will be installed when we are finished.
     */
    if ((scriptArg = rpmdbCountPackages(ts->rpmdb, name)) < 0)
	return 1;
    scriptArg -= 1;

    if (!(transFlags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* run triggers from this package which are keyed on installed 
	   packages */
	if (runImmedTriggers(ts, RPMSENSE_TRIGGERUN, h, -1))
	    return 2;

	/* run triggers which are set off by the removal of this package */
	if (runTriggers(ts, RPMSENSE_TRIGGERUN, h, -1))
	    return 1;
    }

    if (!(transFlags & RPMTRANS_FLAG_TEST)) {
	rc = runInstScript(ts, h, RPMTAG_PREUN, RPMTAG_PREUNPROG, scriptArg,
		          (transFlags & RPMTRANS_FLAG_NOSCRIPTS));
	if (rc)
	    return 1;
    }
    
    rpmMessage(RPMMESS_DEBUG, _("will remove files test = %d\n"), 
		transFlags & RPMTRANS_FLAG_TEST);

    if (!(transFlags & RPMTRANS_FLAG_JUSTDB) &&
	headerGetEntry(h, RPMTAG_BASENAMES, NULL, (void **) &baseNames, 
	               &fileCount)) {
	const char ** fileMd5List;
	uint_32 * fileFlagsList;
	int_16 * fileModesList;
	const char ** dirNames;
	int_32 * dirIndexes;
	int type;
	char * fileName;
	int fnmaxlen;
	int rdlen = (ts->rootDir && !(ts->rootDir[0] == '/' && ts->rootDir[1] == '\0'))
			? strlen(ts->rootDir) : 0;

	headerGetEntry(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
	headerGetEntry(h, RPMTAG_DIRNAMES, NULL, (void **) &dirNames, NULL);

	/* Get buffer for largest possible rootDir + dirname + filename. */
	fnmaxlen = 0;
	for (i = 0; i < fileCount; i++) {
		size_t fnlen;
		fnlen = strlen(baseNames[i]) + 
			strlen(dirNames[dirIndexes[i]]);
		if (fnlen > fnmaxlen)
		    fnmaxlen = fnlen;
	}
	fnmaxlen += rdlen + sizeof("/");	/* XXX one byte too many */

	fileName = alloca(fnmaxlen);

	if (rdlen) {
	    strcpy(fileName, ts->rootDir);
	    (void)rpmCleanPath(fileName);
	    rdlen = strlen(fileName);
	} else
	    *fileName = '\0';

	headerGetEntry(h, RPMTAG_FILEMD5S, &type, (void **) &fileMd5List, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, 
		 &fileCount);
	headerGetEntry(h, RPMTAG_FILEMODES, &type, (void **) &fileModesList, 
		 &fileCount);

	if (ts->notify) {
	    (void)ts->notify(h, RPMCALLBACK_UNINST_START, fileCount, fileCount,
		pkgKey, ts->notifyData);
	}

	/* Traverse filelist backwards to help insure that rmdir() will work. */
	for (i = fileCount - 1; i >= 0; i--) {

	    /* XXX this assumes that dirNames always starts/ends with '/' */
	    (void)stpcpy(stpcpy(fileName+rdlen, dirNames[dirIndexes[i]]), baseNames[i]);

	    rpmMessage(RPMMESS_DEBUG, _("   file: %s action: %s\n"),
			fileName, fileActionString(actions[i]));

	    if (!(transFlags & RPMTRANS_FLAG_TEST)) {
		if (ts->notify) {
		    (void)ts->notify(h, RPMCALLBACK_UNINST_PROGRESS,
			i, actions[i], fileName, ts->notifyData);
		}
		removeFile(fileName, fileFlagsList[i], fileModesList[i], 
			   actions[i]);
	    }
	}

	if (ts->notify) {
	    (void)ts->notify(h, RPMCALLBACK_UNINST_STOP, 0, fileCount,
			pkgKey, ts->notifyData);
	}

	free(baseNames);
	free(dirNames);
	free(fileMd5List);
    }

    if (!(transFlags & RPMTRANS_FLAG_TEST)) {
	rpmMessage(RPMMESS_DEBUG, _("running postuninstall script (if any)\n"));
	rc = runInstScript(ts, h, RPMTAG_POSTUN, RPMTAG_POSTUNPROG,
			scriptArg, (transFlags & RPMTRANS_FLAG_NOSCRIPTS));
	/* XXX postun failures are not cause for erasure failure. */
    }

    if (!(transFlags & RPMTRANS_FLAG_NOTRIGGERS)) {
	/* Run postun triggers which are set off by this package's removal. */
	rc = runTriggers(ts, RPMSENSE_TRIGGERPOSTUN, h, -1);
	if (rc)
	    return 2;
    }

    if (!(transFlags & RPMTRANS_FLAG_TEST))
	rpmdbRemove(ts->rpmdb, ts->id, offset);

    return 0;
}
