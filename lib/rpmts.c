/** \ingroup rpmdep
 * \file lib/rpmts.c
 * Routine(s) to handle an rpmTransactionSet.
 */
#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>		/* XXX rpmtsOpenDB() needs rpmGetPath */
#include <rpmpgp.h>		/* XXX rpmtransFree() needs pgpFreeDig */

#include "rpmds.h"
#include "rpmfi.h"
#include "rpmal.h"
#include "rpmte.h"
#include "rpmts.h"

#include "rpmdb.h"		/* XXX stealing db->db_mode. */

#include "debug.h"

/*@access rpmTransactionSet @*/
/*@access fnpyKey @*/

/*@unchecked@*/
int _ts_debug = 0;

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

    /* XXX there's a db lock race here. */

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

static int rpmtsCloseSDB(rpmTransactionSet ts)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/
{
    int rc = 0;

    if (ts->sdb != NULL) {
	rc = rpmdbClose(ts->sdb);
	ts->sdb = NULL;
    }
    return rc;
}

static int rpmtsOpenSDB(rpmTransactionSet ts)
	/*@globals rpmGlobalMacroContext, fileSystem, internalState @*/
	/*@modifies ts, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    int rc = 0;

    if (ts->sdb != NULL)
	return 0;

    addMacro(NULL, "_dbpath", NULL, "%{?_sdbpath}", RMIL_DEFAULT);
    rc = rpmdbOpen(ts->rootDir, &ts->sdb, O_RDONLY, 0644);
    if (rc) {
	const char * dn;
	/*@-globs -mods@*/ /* FIX: rpmGlobalMacroContext for an error? shrug */
	dn = rpmGetPath(ts->rootDir, "%{_dbpath}", NULL);
	/*@=globs =mods@*/
	rpmMessage(RPMMESS_DEBUG,
			_("cannot open Packages database in %s\n"), dn);
	dn = _free(dn);
    }
    delMacro(NULL, "_dbpath");
    return rc;
}

int rpmtsSolve(rpmTransactionSet ts, rpmDepSet ds)
{
    const char * errstr;
    const char * str;
    const char * qfmt;
    rpmdbMatchIterator mi;
    Header h;
    int rpmtag;
    const char * keyp;
    size_t keylen;
    int rc = 1;	/* assume not found */
    int xx;

    if (ts->goal != TSM_INSTALL)
	return rc;

    if (dsiGetTagN(ds) != RPMTAG_REQUIRENAME)
	return rc;

    keyp = dsiGetN(ds);
    if (keyp == NULL)
	return rc;

    if (ts->sdb == NULL) {
	xx = rpmtsOpenSDB(ts);
	if (xx) return rc;
    }

    qfmt = rpmExpand("%{?_solve_name_fmt}", NULL);
    if (qfmt == NULL || *qfmt == '\0')
	goto exit;

    rpmtag = (*keyp == '/' ? RPMTAG_BASENAMES : RPMTAG_PROVIDENAME);
    keylen = 0;
    mi = rpmdbInitIterator(ts->sdb, rpmtag, keyp, keylen);
    while ((h = rpmdbNextIterator(mi)) != NULL) {

	str = headerSprintf(h, qfmt, rpmTagTable, rpmHeaderFormats, &errstr);
	if (str == NULL) {
	    rpmError(RPMERR_QFMT, _("incorrect format: %s\n"), errstr);
	    break;
	}

	ts->suggests = xrealloc(ts->suggests,
			sizeof(*ts->suggests) * (ts->nsuggests + 2));
	ts->suggests[ts->nsuggests] = str;

	ts->nsuggests++;
	ts->suggests[ts->nsuggests] = NULL;
	break;		/* XXX no alternatives yet */
    }
    mi = rpmdbFreeIterator(mi);

exit:
    qfmt = _free(qfmt);
/*@-nullstate@*/ /* FIX: ts->suggests[] may be NULL */
    return rc;
/*@=nullstate@*/
}

int rpmtsAvailable(rpmTransactionSet ts, const rpmDepSet ds)
{
    fnpyKey * sugkey;
    int rc = 1;	/* assume not found */

    if (ts->availablePackages == NULL)
	return rc;
    sugkey = alAllSatisfiesDepend(ts->availablePackages, ds, NULL);
    if (sugkey == NULL)
	return rc;
    /* XXX no alternatives yet */
    if (sugkey[0] != NULL) {
	ts->suggests = xrealloc(ts->suggests,
			sizeof(*ts->suggests) * (ts->nsuggests + 2));
	ts->suggests[ts->nsuggests] = sugkey[0];
	sugkey[0] = NULL;
	ts->nsuggests++;
	ts->suggests[ts->nsuggests] = NULL;
    }
    sugkey = _free(sugkey);
/*@-nullstate@*/ /* FIX: ts->suggests[] may be NULL */
    return rc;
/*@=nullstate@*/
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

	ts->suggests = _free(ts->suggests);
	ts->nsuggests = 0;

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

	(void) rpmtsCloseSDB(ts);

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
    ts->goal = TSM_UNKNOWN;
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

    ts->solve = rpmtsSolve;
    ts->nsuggests = 0;
    ts->suggests = NULL;
    ts->sdb = NULL;

    ts->numAvailablePackages = 0;
    ts->availablePackages = NULL;

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
