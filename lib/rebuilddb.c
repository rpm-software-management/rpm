#include "system.h"

#include "rpmlib.h"

#include "intl.h"
#include "rpmdb.h"

int rpmdbRebuild(char * rootdir) {
    rpmdb olddb, newdb;
    char * dbpath, * newdbpath;
    int recnum; 
    Header h;
    int failed = 0;

    rpmMessage(RPMMESS_DEBUG, "rebuilding database in rootdir %s\n", rootdir);

    dbpath = rpmGetVar(RPMVAR_DBPATH);
    if (!dbpath) {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	return 1;
    }

    newdbpath = alloca(strlen(dbpath) + 50 + strlen(rootdir));
    sprintf(newdbpath, "%s/%s/rebuilddb.%d", rootdir, dbpath, (int) getpid());

    if (!access(newdbpath, F_OK)) {
	rpmError(RPMERR_MKDIR, _("temporary database %s already exists"),
	      newdbpath);
    }

    rpmMessage(RPMMESS_DEBUG, "creating directory: %s\n", newdbpath);
    if (mkdir(newdbpath, 0755)) {
	rpmError(RPMERR_MKDIR, _("error creating directory %s: %s"),
	      newdbpath, strerror(errno));
    }

    sprintf(newdbpath, "%s/rebuilddb.%d", dbpath, (int) getpid());

    rpmMessage(RPMMESS_DEBUG, "opening old database\n");
    if (openDatabase(rootdir, dbpath, &olddb, O_RDONLY, 0644, 0)) {
	return 1;
    }

    rpmMessage(RPMMESS_DEBUG, "opening new database\n");
    if (openDatabase(rootdir, newdbpath, &newdb, O_RDWR | O_CREAT, 0644, 0)) {
	return 1;
    }

    recnum = rpmdbFirstRecNum(olddb);
    while (recnum > 0) {
	if (!(h = rpmdbGetRecord(olddb, recnum))) {
	    rpmError(RPMERR_INTERNAL, _("cannot read database record at %d"),
			recnum);
	    failed = 1;
	    break;
	}

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
	recnum = rpmdbNextRecNum(olddb, recnum);
    }

    rpmdbClose(olddb);
    rpmdbClose(newdb);

    if (failed) {
	rpmMessage(RPMMESS_NORMAL, "failed to rebuild database; original database "
		"remains in place\n");

	rpmdbRemoveDatabase(rootdir, newdbpath);
	return 1;
    } else {
	if (rpmdbMoveDatabase(rootdir, newdbpath, dbpath)) {
	    rpmMessage(RPMMESS_ERROR, "failed to replace old database with new "
			"database!\n");
	    rpmMessage(RPMMESS_ERROR, "replaces files in %s with files from %s "
			"to recover", dbpath, newdbpath);
	    return 1;
	}
	if (rmdir(newdbpath))
	    rpmMessage(RPMERR_RMDIR, "failed to remove %s: %s\n",
			newdbpath, strerror(errno));
    }


    return 0;
}
