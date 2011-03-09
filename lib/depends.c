/** \ingroup rpmts
 * \file lib/depends.c
 */

#include "system.h"

#include <rpm/rpmcli.h>		/* XXX rpmcliPackagesTotal */

#include <rpm/rpmlib.h>		/* rpmVersionCompare, rpmlib provides */
#include <rpm/rpmtag.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>

#include "lib/rpmts_internal.h"
#include "lib/rpmte_internal.h"
#include "lib/misc.h"

#include "debug.h"

const char * const RPMVERSION = VERSION;

const char * const rpmNAME = PACKAGE;

const char * const rpmEVR = VERSION;

const int rpmFLAGS = RPMSENSE_EQUAL;

/* rpmlib provides */
static rpmds rpmlibP = NULL;

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

#define HASHTYPE depCache
#define HTKEYTYPE const char *
#define HTDATATYPE int
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

#define HASHTYPE intHash
#define HTKEYTYPE unsigned int
#include "rpmhash.C"
#undef HASHTYPE
#undef HASHKEYTYPE

/**
 * Add removed package instance to ordered transaction set.
 * @param ts		transaction set
 * @param h		header
 * @param depends	installed package of pair (or RPMAL_NOMATCH on erase)
 * @return		0 on success
 */
static int removePackage(rpmts ts, Header h, rpmte depends)
{
    tsMembers tsmem = rpmtsMembers(ts);
    rpmte p;
    unsigned int dboffset = headerGetInstance(h);

    /* Can't remove what's not installed */
    if (dboffset == 0) return 1;

    /* Filter out duplicate erasures. */
    if (intHashHasEntry(tsmem->removedPackages, dboffset)) {
        return 0;
    }

    intHashAddEntry(tsmem->removedPackages, dboffset);

    if (tsmem->orderCount >= tsmem->orderAlloced) {
	tsmem->orderAlloced += (tsmem->orderCount - tsmem->orderAlloced) + tsmem->delta;
	tsmem->order = xrealloc(tsmem->order, sizeof(*tsmem->order) * tsmem->orderAlloced);
    }

    p = rpmteNew(ts, h, TR_REMOVED, NULL, NULL);
    rpmteSetDependsOn(p, depends);

    tsmem->order[tsmem->orderCount] = p;
    tsmem->orderCount++;

    return 0;
}

/* Return rpmdb iterator with removals pruned out */
static rpmdbMatchIterator rpmtsPrunedIterator(rpmts ts, rpmDbiTagVal tag, const char * key)
{
    rpmdbMatchIterator mi = rpmtsInitIterator(ts, tag, key, 0);
    tsMembers tsmem = rpmtsMembers(ts);
    rpmdbPruneIterator(mi, tsmem->removedPackages);
    return mi;
}

#define skipColor(_tscolor, _color, _ocolor) \
	((_tscolor) && (_color) && (_ocolor) && !((_color) & (_ocolor)))

/* Add erase elements for older packages of same color (if any). */
static void addUpgradeErasures(rpmts ts, tsMembers tsmem, rpm_color_t tscolor,
				rpmte p, rpm_color_t hcolor, Header h)
{
    Header oh;
    rpmdbMatchIterator mi = rpmtsInitIterator(ts, RPMDBI_NAME, rpmteN(p), 0);

    while((oh = rpmdbNextIterator(mi)) != NULL) {
	/* Ignore colored packages not in our rainbow. */
	if (skipColor(tscolor, hcolor, headerGetNumber(oh, RPMTAG_HEADERCOLOR)))
	    continue;

	/* Skip packages that contain identical NEVR. */
	if (rpmVersionCompare(h, oh) == 0)
	    continue;

	removePackage(ts, oh, p);
    }
    mi = rpmdbFreeIterator(mi);
}

/* Add erase elements for obsoleted packages of same color (if any). */
static void addObsoleteErasures(rpmts ts, tsMembers tsmem, rpm_color_t tscolor,
				rpmte p, rpm_color_t hcolor)
{
    rpmds obsoletes = rpmdsInit(rpmteDS(p, RPMTAG_OBSOLETENAME));
    Header oh;

    while (rpmdsNext(obsoletes) >= 0) {
	const char * Name;
	rpmdbMatchIterator mi = NULL;

	if ((Name = rpmdsN(obsoletes)) == NULL)
	    continue;	/* XXX can't happen */

	/* XXX avoid self-obsoleting packages. */
	if (rstreq(rpmteN(p), Name))
	    continue;

	mi = rpmtsPrunedIterator(ts, RPMDBI_NAME, Name);

	while((oh = rpmdbNextIterator(mi)) != NULL) {
	    /* Ignore colored packages not in our rainbow. */
	    if (skipColor(tscolor, hcolor, 
			  headerGetNumber(oh, RPMTAG_HEADERCOLOR)))
		continue;

	    /*
	     * Rpm prior to 3.0.3 does not have versioned obsoletes.
	     * If no obsoletes version info is available, match all names.
	     */
	    if (rpmdsEVR(obsoletes) == NULL
		     || rpmdsAnyMatchesDep(oh, obsoletes, _rpmds_nopromote)) {
		char * ohNEVRA = headerGetAsString(oh, RPMTAG_NEVRA);
		rpmlog(RPMLOG_DEBUG, "  Obsoletes: %s\t\terases %s\n",
			rpmdsDNEVR(obsoletes)+2, ohNEVRA);
		ohNEVRA = _free(ohNEVRA);

		removePackage(ts, oh, p);
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }
}

/*
 * Check for previously added versions and obsoletions.
 * Return index where to place this element, or -1 to skip.
 */
static int findPos(rpmts ts, rpm_color_t tscolor, Header h, int upgrade)
{
    int oc;
    int obsolete = 0;
    const char * arch = headerGetString(h, RPMTAG_ARCH);
    const char * os = headerGetString(h, RPMTAG_OS);
    rpmte p;
    rpmds oldChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_LESS));
    rpmds newChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_GREATER));
    rpmds sameChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL));
    rpmds obsChk = rpmdsNew(h, RPMTAG_OBSOLETENAME, 0);
    rpmtsi pi = rpmtsiInit(ts);

    /* XXX can't use rpmtsiNext() filter or oc will have wrong value. */
    for (oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
	rpmds thisds, obsoletes;

	/* Only added binary packages need checking */
	if (rpmteType(p) == TR_REMOVED || rpmteIsSource(p))
	    continue;

	/* Skip packages obsoleted by already added packages */
	obsoletes = rpmdsInit(rpmteDS(p, RPMTAG_OBSOLETENAME));
	while (rpmdsNext(obsoletes) >= 0) {
	    if (rpmdsCompare(obsoletes, sameChk)) {
		obsolete = 1;
		oc = -1;
		break;
	    }
	}

	/* Replace already added obsoleted packages by obsoleting package */
	thisds = rpmteDS(p, RPMTAG_NAME);
	rpmdsInit(obsChk);
	while (rpmdsNext(obsChk) >= 0) {
	    if (rpmdsCompare(obsChk, thisds)) {
		obsolete = 1;
		break;
	    }
	}

	if (obsolete)
	    break;

	if (tscolor) {
	    const char * parch = rpmteA(p);
	    const char * pos = rpmteO(p);

	    if (arch == NULL || parch == NULL || os == NULL || pos == NULL)
		continue;
	    if (!rstreq(arch, parch) || !rstreq(os, pos))
		continue;
	}

	/* 
	 * Always skip identical NEVR. 
 	 * On upgrade, if newer NEVR was previously added, skip adding older.
 	 */
	if (rpmdsCompare(sameChk, thisds) ||
		(upgrade && rpmdsCompare(newChk, thisds))) {
	    oc = -1;
	    break;;
	}

 	/* On upgrade, if older NEVR was previously added, replace with new */
	if (upgrade && rpmdsCompare(oldChk, thisds) != 0) {
	    break;
	}
    }

    /* If we broke out of the loop early we've something to say */
    if (p != NULL && rpmIsVerbose()) {
	char *nevra = headerGetAsString(h, RPMTAG_NEVRA);
	const char *msg = (oc < 0) ?
		    _("package %s was already added, skipping %s\n") :
		    _("package %s was already added, replacing with %s\n");
	rpmlog(RPMLOG_WARNING, msg, rpmteNEVRA(p), nevra);
	free(nevra);
    }

    rpmtsiFree(pi);
    rpmdsFree(oldChk);
    rpmdsFree(newChk);
    rpmdsFree(sameChk);
    rpmdsFree(obsChk);
    return oc;
}


int rpmtsAddInstallElement(rpmts ts, Header h,
			fnpyKey key, int upgrade, rpmRelocation * relocs)
{
    tsMembers tsmem = rpmtsMembers(ts);
    rpm_color_t tscolor = rpmtsColor(ts);
    rpmte p = NULL;
    int isSource = headerIsSource(h);
    int ec = 0;
    int oc = tsmem->orderCount;

    /* Check for supported payload format if it's a package */
    if (key && headerCheckPayloadFormat(h) != RPMRC_OK) {
	ec = 1;
	goto exit;
    }

    /* Check binary packages for redundancies in the set */
    if (!isSource) {
	oc = findPos(ts, tscolor, h, upgrade);
	/* If we're replacing a previously added element, free the old one */
	if (oc >= 0 && oc < tsmem->orderCount) {
	    rpmalDel(tsmem->addedPackages, tsmem->order[oc]);
	    tsmem->order[oc] = rpmteFree(tsmem->order[oc]);
	/* If newer NEVR was already added, we're done */
	} else if (oc < 0) {
	    goto exit;
	}
    }

    if (tsmem->addedPackages == NULL) {
	tsmem->addedPackages = rpmalCreate(5, tscolor, rpmtsPrefColor(ts));
    }

    if (oc >= tsmem->orderAlloced) {
	tsmem->orderAlloced += (oc - tsmem->orderAlloced) + tsmem->delta;
	tsmem->order = xrealloc(tsmem->order,
			tsmem->orderAlloced * sizeof(*tsmem->order));
    }

    p = rpmteNew(ts, h, TR_ADDED, key, relocs);

    tsmem->order[oc] = p;
    if (oc == tsmem->orderCount) {
	tsmem->orderCount++;
	rpmcliPackagesTotal++;
    }
    
    rpmalAdd(tsmem->addedPackages, p);

    /* If not upgrading or a source package, then we're done. */
    if (!(upgrade & 0x1) || isSource)
	goto exit;

    /* Do lazy (readonly?) open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsGetDBMode(ts) != -1) {
	if ((ec = rpmtsOpenDB(ts, rpmtsGetDBMode(ts))) != 0)
	    goto exit;
    }

    /* Add erasure elements for old versions and obsoletions */
    addUpgradeErasures(ts, tsmem, tscolor, p, rpmteColor(p), h);
    addObsoleteErasures(ts, tsmem, tscolor, p, rpmteColor(p));

exit:
    return ec;
}

int rpmtsAddEraseElement(rpmts ts, Header h, int dboffset)
{
    return removePackage(ts, h, NULL);
}

/* Cached rpmdb provide lookup, returns 0 if satisfied, 1 otherwise */
static int rpmdbProvides(rpmts ts, depCache dcache, rpmds dep)
{
    const char * Name = rpmdsN(dep);
    const char * DNEVR = rpmdsDNEVR(dep);
    int *cachedrc = NULL;
    rpmdbMatchIterator mi = NULL;
    Header h = NULL;
    int rc = 0;

    /* See if we already looked this up */
    if (depCacheGetEntry(dcache, DNEVR, &cachedrc, NULL, NULL)) {
	rc = *cachedrc;
	rpmdsNotify(dep, "(cached)", rc);
	return rc;
    }

    /*
     * See if a filename dependency is a real file in some package,
     * taking file state into account: replaced, wrong colored and
     * not installed files can not satisfy a dependency.
     */
    if (Name[0] == '/') {
	mi = rpmtsPrunedIterator(ts, RPMDBI_BASENAMES, Name);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    int fs = RPMFILE_STATE_MISSING;
	    struct rpmtd_s states;
	    if (headerGet(h, RPMTAG_FILESTATES, &states, HEADERGET_MINMEM)) {
		rpmtdSetIndex(&states, rpmdbGetIteratorFileNum(mi));
		fs = rpmtdGetNumber(&states);
		rpmtdFreeData(&states);
	    }
	    if (fs == RPMFILE_STATE_NORMAL || fs == RPMFILE_STATE_NETSHARED) {
		rpmdsNotify(dep, "(db files)", rc);
		break;
	    }
	}
	rpmdbFreeIterator(mi);
    }

    /* Otherwise look in provides no matter what the dependency looks like */
    if (h == NULL) {
	mi = rpmtsPrunedIterator(ts, RPMDBI_PROVIDENAME, Name);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (rpmdsAnyMatchesDep(h, dep, _rpmds_nopromote)) {
		rpmdsNotify(dep, "(db provides)", rc);
		break;
	    }
	}
	rpmdbFreeIterator(mi);
    }
    rc = (h != NULL) ? 0 : 1;

    /* Cache the relatively expensive rpmdb lookup results */
    depCacheAddEntry(dcache, xstrdup(DNEVR), rc);
    return rc;
}

/**
 * Check dep for an unsatisfied dependency.
 * @param ts		transaction set
 * @param dep		dependency
 * @return		0 if satisfied, 1 if not satisfied
 */
static int unsatisfiedDepend(rpmts ts, depCache dcache, rpmds dep)
{
    tsMembers tsmem = rpmtsMembers(ts);
    int rc;
    int retrying = 0;
    int adding = (rpmdsInstance(dep) == 0);
    rpmsenseFlags dsflags = rpmdsFlags(dep);

retry:
    rc = 0;	/* assume dependency is satisfied */

    /*
     * New features in rpm packaging implicitly add versioned dependencies
     * on rpmlib provides. The dependencies look like "rpmlib(YaddaYadda)".
     * Check those dependencies now.
     */
    if (dsflags & RPMSENSE_RPMLIB) {
	static int oneshot = -1;
	if (oneshot) 
	    oneshot = rpmdsRpmlib(&rpmlibP, NULL);
	
	if (rpmlibP != NULL && rpmdsSearch(rpmlibP, dep) >= 0) {
	    rpmdsNotify(dep, "(rpmlib provides)", rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    /* Dont look at pre-requisites of already installed packages */
    if (!adding && isInstallPreReq(dsflags) && !isErasePreReq(dsflags))
	goto exit;

    /* Pretrans dependencies can't be satisfied by added packages. */
    if (!(dsflags & RPMSENSE_PRETRANS)) {
	rpmte match = rpmalSatisfiesDepend(tsmem->addedPackages, dep);

	/*
	 * Handle definitive matches within the added package set.
	 * Self-obsoletes and -conflicts fall through here as we need to 
	 * check for possible other matches in the rpmdb.
	 */
	if (match) {
	    rpmTagVal dtag = rpmdsTagN(dep);
	    /* Requires match, look no further */
	    if (dtag == RPMTAG_REQUIRENAME)
		goto exit;

	    /* Conflicts/obsoletes match on another package, look no further */
	    if (rpmteDS(match, dtag) != dep)
		goto exit;
	}
    }

    /* See if the rpmdb provides it */
    if (rpmdbProvides(ts, dcache, dep) == 0)
	goto exit;

    /* Search for an unsatisfied dependency. */
    if (adding && !retrying && !(dsflags & RPMSENSE_PRETRANS)) {
	int xx = rpmtsSolve(ts, dep);
	if (xx == 0)
	    goto exit;
	if (xx == -1) {
	    retrying = 1;
	    goto retry;
	}
    }

unsatisfied:
    rc = 1;	/* dependency is unsatisfied */
    rpmdsNotify(dep, NULL, rc);

exit:
    return rc;
}

/* Check a dependency set for problems */
static void checkDS(rpmts ts, depCache dcache, rpmte te,
		const char * pkgNEVRA, rpmds ds,
		const char * depName, rpm_color_t tscolor)
{
    rpm_color_t dscolor;
    /* require-problems are unsatisfied, others appear "satisfied" */
    int is_problem = (rpmdsTagN(ds) == RPMTAG_REQUIRENAME);

    ds = rpmdsInit(ds);
    while (rpmdsNext(ds) >= 0) {
	/* Filter out dependencies that came along for the ride. */
	if (depName != NULL && !rstreq(depName, rpmdsN(ds)))
	    continue;

	/* Ignore colored dependencies not in our rainbow. */
	dscolor = rpmdsColor(ds);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	if (unsatisfiedDepend(ts, dcache, ds) == is_problem)
	    rpmteAddDepProblem(te, pkgNEVRA, ds, NULL);
    }
}

/* Check a given dependency type against installed packages */
static void checkInstDeps(rpmts ts, depCache dcache, rpmte te,
			  rpmTag depTag, const char *dep)
{
    Header h;
    rpmdbMatchIterator mi = rpmtsPrunedIterator(ts, depTag, dep);

    while ((h = rpmdbNextIterator(mi)) != NULL) {
	char * pkgNEVRA = headerGetAsString(h, RPMTAG_NEVRA);
	rpmds ds = rpmdsNew(h, depTag, 0);

	checkDS(ts, dcache, te, pkgNEVRA, ds, dep, 0);

	ds = rpmdsFree(ds);
	pkgNEVRA = _free(pkgNEVRA);
    }
    rpmdbFreeIterator(mi);
}

int rpmtsCheck(rpmts ts)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpmtsi pi = NULL; rpmte p;
    int closeatexit = 0;
    int rc = 0;
    depCache dcache = NULL;
    
    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    /* Do lazy, readonly, open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsGetDBMode(ts) != -1) {
	if ((rc = rpmtsOpenDB(ts, rpmtsGetDBMode(ts))) != 0)
	    goto exit;
	closeatexit = 1;
    }

    /* XXX FIXME: figure some kind of heuristic for the cache size */
    dcache = depCacheCreate(5001, hashFunctionString, strcmp,
				     (depCacheFreeKey)rfree, NULL);

    /*
     * Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmds provides = rpmdsInit(rpmteDS(p, RPMTAG_PROVIDENAME));

	rpmlog(RPMLOG_DEBUG, "========== +++ %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	checkDS(ts, dcache, p, rpmteNEVRA(p), rpmteDS(p, RPMTAG_REQUIRENAME),
		NULL, tscolor);
	checkDS(ts, dcache, p, rpmteNEVRA(p), rpmteDS(p, RPMTAG_CONFLICTNAME),
		NULL, tscolor);

	/* Check provides against conflicts in installed packages. */
	while (rpmdsNext(provides) >= 0) {
	    checkInstDeps(ts, dcache, p, RPMTAG_CONFLICTNAME, rpmdsN(provides));
	}

	/* Skip obsoletion checks for source packages (ie build) */
	if (rpmteIsSource(p))
	    continue;

	/* Check package name (not provides!) against installed obsoletes */
	checkInstDeps(ts, dcache, p, RPMTAG_OBSOLETENAME, rpmteN(p));
    }
    pi = rpmtsiFree(pi);

    /*
     * Look at the removed packages and make sure they aren't critical.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
	rpmds provides = rpmdsInit(rpmteDS(p, RPMTAG_PROVIDENAME));
	rpmfi fi = rpmfiInit(rpmteFI(p), 0);

	rpmlog(RPMLOG_DEBUG, "========== --- %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	/* Check provides and filenames against installed dependencies. */
	while (rpmdsNext(provides) >= 0) {
	    checkInstDeps(ts, dcache, p, RPMTAG_REQUIRENAME, rpmdsN(provides));
	}

	while (rpmfiNext(fi) >= 0) {
	    checkInstDeps(ts, dcache, p, RPMTAG_REQUIRENAME, rpmfiFN(fi));
	}
    }
    pi = rpmtsiFree(pi);

exit:
    depCacheFree(dcache);

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    if (closeatexit)
	(void) rpmtsCloseDB(ts);
    return rc;
}
