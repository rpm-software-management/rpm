/** \ingroup rpmdep
 * \file lib/rpmts.c
 * Routine(s) to handle an rpmTransactionSet.
 */
#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>		/* XXX rpmtsOpenDB() needs rpmGetPath */
#include <rpmpgp.h>		/* XXX rpmtransFree() needs pgpFreeDig */

#include "rpmte.h"
#include "rpmts.h"

#include "rpmdb.h"		/* XXX stealing db->db_mode. */

#include "debug.h"

/*@access rpmTransactionSet @*/

/*@unchecked@*/
int _ts_debug = 0;

rpmTransactionSet XrpmtsUnlink(rpmTransactionSet ts, const char * msg, const char * fn, unsigned ln)
{
/*@-modfilesystem@*/
if (_ts_debug)
fprintf(stderr, "--> ts %p -- %d %s at %s:%u\n", ts, ts->nrefs, msg, fn, ln);
/*@=modfilesystem@*/
    ts->nrefs--;
    return NULL;
}

rpmTransactionSet XrpmtsLink(rpmTransactionSet ts, const char * msg, const char * fn, unsigned ln)
{
    ts->nrefs++;
/*@-modfilesystem@*/
if (_ts_debug)
fprintf(stderr, "--> ts %p ++ %d %s at %s:%u\n", ts, ts->nrefs, msg, fn, ln);
/*@=modfilesystem@*/
    /*@-refcounttrans@*/ return ts; /*@=refcounttrans@*/
}

int rpmtsCloseDB(rpmTransactionSet ts)
{
    int rc = 0;

    if (ts->rpmdb != NULL) {
	rc = rpmdbClose(ts->rpmdb);
	ts->rpmdb = NULL;
    }
    return rc;
}

int rpmtsOpenDB(rpmTransactionSet ts, int dbmode)
{
    int rc = 0;

    if (ts->rpmdb != NULL && ts->dbmode == dbmode)
	return 0;

    (void) rpmtsCloseDB(ts);

    /* XXX there's a lock race here. */

    ts->dbmode = dbmode;
    rc = rpmdbOpen(ts->rootDir, &ts->rpmdb, ts->dbmode, 0644);
    if (rc) {
	const char * dn;
	/*@-globs -mods@*/ /* FIX: rpmGlobalMacroContext for an error? shrug */
	dn = rpmGetPath(ts->rootDir, "%{_dbpath}", NULL);
	/*@=globs =mods@*/
	rpmMessage(RPMMESS_ERROR,
			_("cannot open Packages database in %s\n"), dn);
	dn = _free(dn);
    }
    return rc;
}

rpmdbMatchIterator rpmtsInitIterator(const rpmTransactionSet ts, int rpmtag,
			const void * keyp, size_t keylen)
{
    return rpmdbInitIterator(ts->rpmdb, rpmtag, keyp, keylen);
}

char * hGetNEVR(Header h, const char ** np)
{
    const char * n, * v, * r;
    char * NVR, * t;

    (void) headerNVR(h, &n, &v, &r);
    NVR = t = xcalloc(1, strlen(n) + strlen(v) + strlen(r) + sizeof("--"));
    t = stpcpy(t, n);
    t = stpcpy(t, "-");
    t = stpcpy(t, v);
    t = stpcpy(t, "-");
    t = stpcpy(t, r);
    if (np)
	*np = n;
    return NVR;
}

void rpmtransClean(rpmTransactionSet ts)
{
    if (ts) {
	teIterator pi; transactionElement p;

	/* Clean up after dependency checks. */
	pi = teInitIterator(ts);
	while ((p = teNextIterator(pi)) != NULL)
	    teCleanDS(p);
	pi = teFreeIterator(pi);

	ts->addedPackages = alFree(ts->addedPackages);
	ts->numAddedPackages = 0;

	if (ts->sig != NULL)
	    ts->sig = headerFreeData(ts->sig, ts->sigtype);
	if (ts->dig != NULL)
	    ts->dig = pgpFreeDig(ts->dig);
    }
}

rpmTransactionSet rpmtransFree(rpmTransactionSet ts)
{
    if (ts) {
	teIterator pi; transactionElement p;
	int oc;

	(void) rpmtsUnlink(ts, "tsCreate");

	/*@-usereleased@*/
	if (ts->nrefs > 0)
	    return NULL;

	(void) rpmtsCloseDB(ts);

	ts->availablePackages = alFree(ts->availablePackages);
	ts->numAvailablePackages = 0;
	ts->di = _free(ts->di);
	ts->removedPackages = _free(ts->removedPackages);
	if (ts->scriptFd != NULL) {
	    ts->scriptFd =
		fdFree(ts->scriptFd, "rpmtransSetScriptFd (rpmtransFree");
	    ts->scriptFd = NULL;
	}
	ts->rootDir = _free(ts->rootDir);
	ts->currDir = _free(ts->currDir);

	for (pi = teInitIterator(ts), oc = 0; (p = teNextIterator(pi)) != NULL; oc++) {
/*@-type -unqualifiedtrans @*/
	    ts->order[oc] = teFree(ts->order[oc]);
/*@=type =unqualifiedtrans @*/
	}
	pi = teFreeIterator(pi);
/*@-type +voidabstract @*/	/* FIX: double indirection */
	ts->order = _free(ts->order);
/*@=type =voidabstract @*/

/*@-nullstate@*/	/* FIX: partial annotations */
	rpmtransClean(ts);
/*@=nullstate@*/

	/*@-refcounttrans@*/ ts = _free(ts); /*@=refcounttrans@*/
	/*@=usereleased@*/
    }
    return NULL;
}

rpmTransactionSet rpmtransCreateSet(rpmdb db, const char * rootDir)
{
    rpmTransactionSet ts;
    int rootLen;

    /*@-branchstate@*/
    if (!rootDir) rootDir = "";
    /*@=branchstate@*/

    ts = xcalloc(1, sizeof(*ts));
    ts->filesystemCount = 0;
    ts->filesystems = NULL;
    ts->di = NULL;
    if (db != NULL) {
	ts->rpmdb = rpmdbLink(db, "tsCreate");
	/*@-type@*/ /* FIX: silly wrapper */
	ts->dbmode = db->db_mode;
	/*@=type@*/
    } else {
	ts->rpmdb = NULL;
	ts->dbmode = O_RDONLY;
    }
    ts->scriptFd = NULL;
    ts->id = (int_32) time(NULL);
    ts->delta = 5;

    ts->numRemovedPackages = 0;
    ts->allocedRemovedPackages = ts->delta;
    ts->removedPackages = xcalloc(ts->allocedRemovedPackages,
			sizeof(*ts->removedPackages));

    /* This canonicalizes the root */
    rootLen = strlen(rootDir);
    if (!(rootLen && rootDir[rootLen - 1] == '/')) {
	char * t;

	t = alloca(rootLen + 2);
	*t = '\0';
	(void) stpcpy( stpcpy(t, rootDir), "/");
	rootDir = t;
    }

    ts->rootDir = (rootDir != NULL ? xstrdup(rootDir) : xstrdup(""));
    ts->currDir = NULL;
    ts->chrootDone = 0;

    ts->numAddedPackages = 0;
    ts->addedPackages = NULL;
    ts->numAvailablePackages = 0;
    ts->availablePackages = NULL;
    ts->numAddedPackages = 0;
    ts->orderAlloced = 0;
    ts->orderCount = 0;
    ts->order = NULL;

    ts->sig = NULL;
    ts->dig = NULL;

    ts->nrefs = 0;

    return rpmtsLink(ts, "tsCreate");
}

rpmtransFlags rpmtsGetFlags(rpmTransactionSet ts)
{
    rpmtransFlags otransFlags = 0;
    if (ts != NULL) {
	otransFlags = ts->transFlags;
    }
    return otransFlags;
}

rpmtransFlags rpmtsSetFlags(rpmTransactionSet ts, rpmtransFlags ntransFlags)
{
    rpmtransFlags otransFlags = 0;
    if (ts != NULL) {
	otransFlags = ts->transFlags;
	ts->transFlags = ntransFlags;
    }
    return otransFlags;
}

int rpmtsSetNotifyCallback(rpmTransactionSet ts,
		rpmCallbackFunction notify, rpmCallbackData notifyData)
{
    if (ts != NULL) {
	ts->notify = notify;
	ts->notifyData = notifyData;
    }
    return 0;
}
