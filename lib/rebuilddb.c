#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */

#include "rpmdb.h"
#include "misc.h"

extern int _useDbiMajor;	/* XXX shared with dbindex.c */
extern int __do_dbenv_remove;   /* XXX in dbindex.c, shared with rebuilddb.c */

/** */
int rpmdbRebuild(const char * rootdir)
{
    rpmdb olddb;
    const char * dbpath = NULL;
    const char * rootdbpath = NULL;
    rpmdb newdb;
    const char * newdbpath = NULL;
    const char * newrootdbpath = NULL;
    const char * tfn;
    int nocleanup = 1;
    int failed = 0;
    int rc = 0;
    int _filterDbDups;	/* Filter duplicate entries ? (bug in pre rpm-3.0.4) */
    int _preferDbiMajor;

    _filterDbDups = rpmExpandNumeric("%{_filterdbdups}");
    _preferDbiMajor = rpmExpandNumeric("%{_preferdb}");
fprintf(stderr, "*** rpmdbRebuild: filterdbdups %d preferdb %d\n", _filterDbDups, _preferDbiMajor);

    tfn = rpmGetPath("%{_dbpath}", NULL);
    if (!(tfn && tfn[0] != '%')) {
	rpmMessage(RPMMESS_DEBUG, _("no dbpath has been set"));
	rc = 1;
	goto exit;
    }
    dbpath = rootdbpath = rpmGetPath(rootdir, tfn, NULL);
    if (!(rootdir[0] == '/' && rootdir[1] == '\0'))
	dbpath += strlen(rootdir);
    xfree(tfn);

    tfn = rpmGetPath("%{_rebuilddbpath}", NULL);
    if (!(tfn && tfn[0] != '%' && strcmp(tfn, dbpath))) {
	char pidbuf[20];
	char *t;
	sprintf(pidbuf, "rebuilddb.%d", (int) getpid());
	t = xmalloc(strlen(dbpath) + strlen(pidbuf) + 1);
	(void)stpcpy(stpcpy(t, dbpath), pidbuf);
	if (tfn) xfree(tfn);
	tfn = t;
	nocleanup = 0;
    }
    newdbpath = newrootdbpath = rpmGetPath(rootdir, tfn, NULL);
    if (!(rootdir[0] == '/' && rootdir[1] == '\0'))
	newdbpath += strlen(rootdir);
    xfree(tfn);

    rpmMessage(RPMMESS_DEBUG, _("rebuilding database %s into %s\n"),
	rootdbpath, newrootdbpath);

    if (!access(newrootdbpath, F_OK)) {
	rpmError(RPMERR_MKDIR, _("temporary database %s already exists"),
	      newrootdbpath);
	rc = 1;
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("creating directory: %s\n"), newrootdbpath);
    if (Mkdir(newrootdbpath, 0755)) {
	rpmError(RPMERR_MKDIR, _("error creating directory %s: %s"),
	      newrootdbpath, strerror(errno));
	rc = 1;
	goto exit;
    }

#if 0
    _useDbiMajor = ((_preferDbiMajor >= 0) ? (_preferDbiMajor & 0x03) : -1);
#else
    _useDbiMajor = -1;
#endif
    rpmMessage(RPMMESS_DEBUG, _("opening old database with dbi_major %d\n"),
		_useDbiMajor);
    if (openDatabase(rootdir, dbpath, &olddb, O_RDONLY, 0644, 
		     RPMDB_FLAG_MINIMAL)) {
	rc = 1;
	goto exit;
    }

    _useDbiMajor = ((_preferDbiMajor >= 0) ? (_preferDbiMajor & 0x03) : -1);
    rpmMessage(RPMMESS_DEBUG, _("opening new database with dbi_major %d\n"),
		_useDbiMajor);
    if (openDatabase(rootdir, newdbpath, &newdb, O_RDWR | O_CREAT, 0644, 0)) {
	rc = 1;
	goto exit;
    }

    {	Header h = NULL;
	rpmdbMatchIterator mi;
#define	_RECNUM	rpmdbGetIteratorOffset(mi)

	/* RPMDBI_PACKAGES */
	mi = rpmdbInitIterator(olddb, 0, NULL, 0);
	while ((h = rpmdbNextIterator(mi)) != NULL) {

	    /* let's sanity check this record a bit, otherwise just skip it */
	    if (!(headerIsEntry(h, RPMTAG_NAME) &&
		headerIsEntry(h, RPMTAG_VERSION) &&
		headerIsEntry(h, RPMTAG_RELEASE) &&
		headerIsEntry(h, RPMTAG_BUILDTIME)))
	    {
		rpmError(RPMERR_INTERNAL,
			_("record number %d in database is bad -- skipping."), 
			_RECNUM);
		continue;
	    }

	    /* Filter duplicate entries ? (bug in pre rpm-3.0.4) */
	    if (_filterDbDups) {
		const char * name, * version, * release;
		int skip = 0;

		headerNVR(h, &name, &version, &release);

		{   rpmdbMatchIterator mi;
		    mi = rpmdbInitIterator(newdb, RPMTAG_NAME, name, 0);
		    rpmdbSetIteratorVersion(mi, version);
		    rpmdbSetIteratorRelease(mi, release);
		    while (rpmdbNextIterator(mi)) {
			skip = 1;
			break;
		    }
		    rpmdbFreeIterator(mi);
		}

		if (skip)
		    continue;
	    }

	    /* Retrofit "Provide: name = EVR" for binary packages. */
	    providePackageNVR(h);

	    if (rpmdbAdd(newdb, h)) {
		rpmError(RPMERR_INTERNAL,
			_("cannot add record originally at %d"), _RECNUM);
		failed = 1;
		break;
	    }
	}

	rpmdbFreeIterator(mi);

    }

    if (!nocleanup)
	__do_dbenv_remove = 1;	/* XXX in dbindex.c, shared with rebuilddb.c */
    rpmdbClose(olddb);
    rpmdbClose(newdb);

    if (failed) {
	rpmMessage(RPMMESS_NORMAL, _("failed to rebuild database; original database "
		"remains in place\n"));

	rpmdbRemoveDatabase(rootdir, newdbpath);
	rc = 1;
	goto exit;
    } else if (!nocleanup) {
	if (rpmdbMoveDatabase(rootdir, newdbpath, dbpath)) {
	    rpmMessage(RPMMESS_ERROR, _("failed to replace old database with new "
			"database!\n"));
	    rpmMessage(RPMMESS_ERROR, _("replace files in %s with files from %s "
			"to recover"), dbpath, newdbpath);
	    rc = 1;
	    goto exit;
	}
	if (Rmdir(newrootdbpath))
	    rpmMessage(RPMMESS_ERROR, _("failed to remove directory %s: %s\n"),
			newrootdbpath, strerror(errno));
    }
    rc = 0;

exit:
    if (rootdbpath)		xfree(rootdbpath);
    if (newrootdbpath)	xfree(newrootdbpath);

    return rc;
}
