#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */

#include "rpmdb.h"

int rpmdbRebuild(const char * rootdir)
{
    rpmdb olddb, newdb;
    const char * dbpath = NULL;
    const char * newdbpath = NULL;
    const char * newrootdbpath = NULL;
    int recnum; 
    Header h;
    int failed = 0;
    int rc = 0;
    char tfn[64];

    rpmMessage(RPMMESS_DEBUG, _("rebuilding database in rootdir %s\n"), rootdir);

    dbpath = rpmGetPath("%{_dbpath}", NULL);
    if (!(dbpath && dbpath[0] != '%')) {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	rc = 1;
	goto exit;
    }

    sprintf(tfn, "rebuilddb.%d", (int) getpid());
    newrootdbpath = rpmGetPath(rootdir, dbpath, tfn, NULL);
    newdbpath = newrootdbpath + strlen(rootdir);

    if (!access(newrootdbpath, F_OK)) {
	rpmError(RPMERR_MKDIR, _("temporary database %s already exists"),
	      newrootdbpath);
    }

    rpmMessage(RPMMESS_DEBUG, _("creating directory: %s\n"), newrootdbpath);
    if (mkdir(newrootdbpath, 0755)) {
	rpmError(RPMERR_MKDIR, _("error creating directory %s: %s"),
	      newrootdbpath, strerror(errno));
    }

    rpmMessage(RPMMESS_DEBUG, _("opening old database\n"));
    if (openDatabase(rootdir, dbpath, &olddb, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("opening new database\n"));
    if (openDatabase(rootdir, newdbpath, &newdb, O_RDWR | O_CREAT, 0644, 0)) {
	rc = 1;
	goto exit;
    }

    recnum = rpmdbFirstRecNum(olddb);
    while (recnum > 0) {
	if ((h = rpmdbGetRecord(olddb, recnum)) == NULL) {
	    rpmError(RPMERR_INTERNAL,
			_("record number %d in database is bad -- skipping it"),
			recnum);
	    break;
	} else {
	    /* let's sanity check this record a bit, otherwise just skip it */
	    if (headerIsEntry(h, RPMTAG_NAME) &&
		headerIsEntry(h, RPMTAG_VERSION) &&
		headerIsEntry(h, RPMTAG_RELEASE) &&
		headerIsEntry(h, RPMTAG_RELEASE) &&
		headerIsEntry(h, RPMTAG_BUILDTIME)) {
		if (rpmdbAdd(newdb, h)) {
		    rpmError(RPMERR_INTERNAL,
			_("cannot add record originally at %d"), recnum);
		    failed = 1;
		    break;
		}
	    } else {
		rpmError(RPMERR_INTERNAL,
			_("record number %d in database is bad -- skipping it"), 
			recnum);
	    }

	    headerFree(h);
	}
	recnum = rpmdbNextRecNum(olddb, recnum);
    }

    rpmdbClose(olddb);
    rpmdbClose(newdb);

    if (failed) {
	rpmMessage(RPMMESS_NORMAL, _("failed to rebuild database; original database "
		"remains in place\n"));

	rpmdbRemoveDatabase(rootdir, newdbpath);
	rc = 1;
	goto exit;
    } else {
	if (rpmdbMoveDatabase(rootdir, newdbpath, dbpath)) {
	    rpmMessage(RPMMESS_ERROR, _("failed to replace old database with new "
			"database!\n"));
	    rpmMessage(RPMMESS_ERROR, _("replaces files in %s with files from %s "
			"to recover"), dbpath, newdbpath);
	    rc = 1;
	    goto exit;
	}
	if (rmdir(newrootdbpath))
	    rpmMessage(RPMMESS_ERROR, _("failed to remove directory %s: %s\n"),
			newrootdbpath, strerror(errno));
    }
    rc = 0;

exit:
    if (dbpath)		xfree(dbpath);
    if (newrootdbpath)	xfree(newrootdbpath);

    return rc;
}
