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

#include "debug.h"

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

/**
 * Compare removed package instances (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
static int intcmp(const void * a, const void * b)
{
    const int * aptr = a;
    const int * bptr = b;
    int rc = (*aptr - *bptr);
    return rc;
}

/**
 * Add removed package instance to ordered transaction set.
 * @param ts		transaction set
 * @param h		header
 * @param depends	installed package of pair (or RPMAL_NOMATCH on erase)
 * @return		0 on success
 */
static int removePackage(rpmts ts, Header h, rpmte depends)
{
    rpmte p;
    unsigned int dboffset = headerGetInstance(h);

    /* Can't remove what's not installed */
    if (dboffset == 0) return 1;

    /* Filter out duplicate erasures. */
    if (ts->numRemovedPackages > 0 && ts->removedPackages != NULL) {
	if (bsearch(&dboffset, ts->removedPackages, ts->numRemovedPackages,
			sizeof(*ts->removedPackages), intcmp) != NULL)
	    return 0;
    }

    if (ts->numRemovedPackages == ts->allocedRemovedPackages) {
	ts->allocedRemovedPackages += ts->delta;
	ts->removedPackages = xrealloc(ts->removedPackages,
		sizeof(ts->removedPackages) * ts->allocedRemovedPackages);
    }

    if (ts->removedPackages != NULL) {	/* XXX can't happen. */
	ts->removedPackages[ts->numRemovedPackages] = dboffset;
	ts->numRemovedPackages++;
	if (ts->numRemovedPackages > 1)
	    qsort(ts->removedPackages, ts->numRemovedPackages,
			sizeof(*ts->removedPackages), intcmp);
    }

    if (ts->orderCount >= ts->orderAlloced) {
	ts->orderAlloced += (ts->orderCount - ts->orderAlloced) + ts->delta;
	ts->order = xrealloc(ts->order, sizeof(*ts->order) * ts->orderAlloced);
    }

    p = rpmteNew(ts, h, TR_REMOVED, NULL, NULL, -1);
    rpmteSetDependsOn(p, depends);

    ts->order[ts->orderCount] = p;
    ts->orderCount++;

    return 0;
}

int rpmtsAddInstallElement(rpmts ts, Header h,
			fnpyKey key, int upgrade, rpmRelocation * relocs)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpm_color_t hcolor;
    rpm_color_t ohcolor;
    rpmdbMatchIterator mi;
    Header oh;
    int isSource;
    int duplicate = 0;
    rpmtsi pi = NULL; rpmte p;
    rpmds oldChk = NULL, newChk = NULL, sameChk = NULL;
    rpmds obsoletes;
    int xx;
    int ec = 0;
    int rc;
    int oc;

    /* Check for supported payload format if it's a package */
    if (key && headerCheckPayloadFormat(h) != RPMRC_OK) {
	ec = 1;
	goto exit;
    }

    if (ts->addedPackages == NULL) {
	ts->addedPackages = rpmalCreate(5, tscolor, rpmtsPrefColor(ts));
    }

    /* XXX Always add source headers. */
    isSource = headerIsSource(h);
    if (isSource) {
	oc = ts->orderCount;
	goto addheader;
    }

    /*
     * Check for previously added versions with the same name and arch/os.
     * FIXME: only catches previously added, older packages.
     */
    oldChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_LESS));
    newChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_GREATER));
    sameChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL));
    /* XXX can't use rpmtsiNext() filter or oc will have wrong value. */
    for (pi = rpmtsiInit(ts), oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
	rpmds this;
	const char *pkgNEVR, *addNEVR;
	int skip = 0;

	/* XXX Only added packages need be checked for dupes. */
	if (rpmteType(p) == TR_REMOVED)
	    continue;

	/* XXX Never check source headers. */
	if (rpmteIsSource(p))
	    continue;

	if (tscolor) {
	    const char * arch = headerGetString(h, RPMTAG_ARCH);
	    const char * os = headerGetString(h, RPMTAG_OS);
	    const char * parch = rpmteA(p);
	    const char * pos = rpmteO(p);

	    if (arch == NULL || parch == NULL)
		continue;
	    if (os == NULL || pos == NULL)
		continue;
	    if (!rstreq(arch, parch) || !rstreq(os, pos))
		continue;
	}

	/* OK, binary rpm's with same arch and os.  Check NEVR. */
	if ((this = rpmteDS(p, RPMTAG_NAME)) == NULL)
	    continue;	/* XXX can't happen */

	/* 
	 * Always skip identical NEVR. 
 	 * On upgrade, if newer NEVR was previously added, 
 	 * then skip adding older. 
 	 */
	if (rpmdsCompare(sameChk, this)) {
	    skip = 1;
	    addNEVR = rpmdsDNEVR(sameChk);
	} else if (upgrade && rpmdsCompare(newChk, this)) {
	    skip = 1;
	    addNEVR = rpmdsDNEVR(newChk);
	}

	if (skip) {
	    pkgNEVR = rpmdsDNEVR(this);
	    if (rpmIsVerbose())
		rpmlog(RPMLOG_WARNING,
		    _("package %s was already added, skipping %s\n"),
		    (pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		    (addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    ec = 0;
	    goto exit;
	}

	/*
 	 * On upgrade, if older NEVR was previously added, 
 	 * then replace old with new. 
 	 */
	rc = rpmdsCompare(oldChk, this);
	if (upgrade && rc != 0) {
	    pkgNEVR = rpmdsDNEVR(this);
	    addNEVR = rpmdsDNEVR(newChk);
	    if (rpmIsVerbose())
		rpmlog(RPMLOG_WARNING,
		    _("package %s was already added, replacing with %s\n"),
		    (pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		    (addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    duplicate = 1;
	    rpmalDel(ts->addedPackages, p);
	    break;
	}
    }
    pi = rpmtsiFree(pi);

    /* If newer NEVR was already added, exit now. */
    if (ec)
	goto exit;

addheader:
    if (oc >= ts->orderAlloced) {
	ts->orderAlloced += (oc - ts->orderAlloced) + ts->delta;
	ts->order = xrealloc(ts->order, ts->orderAlloced * sizeof(*ts->order));
    }

    p = rpmteNew(ts, h, TR_ADDED, key, relocs, -1);

    if (duplicate && oc < ts->orderCount) {
	ts->order[oc] = rpmteFree(ts->order[oc]);
    }

    ts->order[oc] = p;
    if (!duplicate) {
	ts->orderCount++;
	rpmcliPackagesTotal++;
    }
    
    rpmalAdd(ts->addedPackages, p);

    if (!duplicate) {
	ts->numAddedPackages++;
    }

    /* XXX rpmgi hack: Save header in transaction element if requested. */
    if (upgrade & 0x2)
	(void) rpmteSetHeader(p, h);

    /* If not upgrading, then we're done. */
    if (!(upgrade & 0x1))
	goto exit;

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (isSource)
	goto exit;

    /* Do lazy (readonly?) open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsGetDBMode(ts) != -1) {
	if ((ec = rpmtsOpenDB(ts, rpmtsGetDBMode(ts))) != 0)
	    goto exit;
    }

    /* On upgrade, erase older packages of same color (if any). */
    hcolor = headerGetNumber(h, RPMTAG_HEADERCOLOR);
    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
    while((oh = rpmdbNextIterator(mi)) != NULL) {

	/* Ignore colored packages not in our rainbow. */
	ohcolor = headerGetNumber(oh, RPMTAG_HEADERCOLOR);
	if (tscolor && hcolor && ohcolor && !(hcolor & ohcolor))
	    continue;

	/* Skip packages that contain identical NEVR. */
	if (rpmVersionCompare(h, oh) == 0)
	    continue;

	xx = removePackage(ts, oh, p);
    }
    mi = rpmdbFreeIterator(mi);

    obsoletes = rpmdsLink(rpmteDS(p, RPMTAG_OBSOLETENAME), RPMDBG_M("Obsoletes"));
    obsoletes = rpmdsInit(obsoletes);
    if (obsoletes != NULL)
    while (rpmdsNext(obsoletes) >= 0) {
	const char * Name;

	if ((Name = rpmdsN(obsoletes)) == NULL)
	    continue;	/* XXX can't happen */

	/* XXX avoid self-obsoleting packages. */
	if (rstreq(rpmteN(p), Name))
	    continue;

	if (Name[0] == '/')
	    mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, Name, 0);
	else
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, Name, 0);

	xx = rpmdbPruneIterator(mi,
	    ts->removedPackages, ts->numRemovedPackages, 1);

	while((oh = rpmdbNextIterator(mi)) != NULL) {
	    /* Ignore colored packages not in our rainbow. */
	    ohcolor = headerGetNumber(oh, RPMTAG_HEADERCOLOR);
	    /* XXX provides *are* colored, effectively limiting Obsoletes:
		to matching only colored Provides: based on pkg coloring. */
	    if (tscolor && hcolor && ohcolor && !(hcolor & ohcolor))
		continue;

	    /*
	     * Rpm prior to 3.0.3 does not have versioned obsoletes.
	     * If no obsoletes version info is available, match all names.
	     */
	    if (rpmdsEVR(obsoletes) == NULL
	     || rpmdsAnyMatchesDep(oh, obsoletes, _rpmds_nopromote)) {
		char * ohNEVRA = headerGetAsString(oh, RPMTAG_NEVRA);
#ifdef	DYING	/* XXX see http://bugzilla.redhat.com #134497 */
		if (rpmVersionCompare(h, oh))
#endif
		    xx = removePackage(ts, oh, p);
		rpmlog(RPMLOG_DEBUG, "  Obsoletes: %s\t\terases %s\n",
			rpmdsDNEVR(obsoletes)+2, ohNEVRA);
		ohNEVRA = _free(ohNEVRA);
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }
    obsoletes = rpmdsFree(obsoletes);

    ec = 0;

exit:
    oldChk = rpmdsFree(oldChk);
    newChk = rpmdsFree(newChk);
    sameChk = rpmdsFree(sameChk);
    pi = rpmtsiFree(pi);
    return ec;
}

int rpmtsAddEraseElement(rpmts ts, Header h, int dboffset)
{
    return removePackage(ts, h, NULL);
}

/**
 * Check dep for an unsatisfied dependency.
 * @param ts		transaction set
 * @param dep		dependency
 * @param adding	dependency is from added package set?
 * @return		0 if satisfied, 1 if not satisfied, 2 if error
 */
static int unsatisfiedDepend(rpmts ts, depCache dcache, rpmds dep, int adding)
{
    rpmdbMatchIterator mi;
    const char * Name = rpmdsN(dep);
    const char * DNEVR = rpmdsDNEVR(dep);
    Header h;
    int rc;
    int xx;
    int retrying = 0;
    int *cachedrc = NULL;
    int cacheThis = 0;

    if (Name == NULL || DNEVR == NULL)
	return 0;	/* XXX can't happen */

retry:
    rc = 0;	/* assume dependency is satisfied */

    /*
     * New features in rpm packaging implicitly add versioned dependencies
     * on rpmlib provides. The dependencies look like "rpmlib(YaddaYadda)".
     * Check those dependencies now.
     */
    if (rpmdsFlags(dep) & RPMSENSE_RPMLIB) {
	static int oneshot = -1;
	if (oneshot) 
	    oneshot = rpmdsRpmlib(&rpmlibP, NULL);
	
	if (rpmlibP != NULL && rpmdsSearch(rpmlibP, dep) >= 0) {
	    rpmdsNotify(dep, _("(rpmlib provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    /* Search added packages for the dependency. */
    if (rpmalSatisfiesDepend(ts->addedPackages, dep) != NULL) {
	goto exit;
    }

    /* See if we already looked this up */
    if (depCacheGetEntry(dcache, DNEVR, &cachedrc, NULL, NULL)) {
	rc = *cachedrc;
	rpmdsNotify(dep, _("(cached)"), rc);
	return rc;
    }
    /* Only bother caching the expensive rpmdb lookups */
    cacheThis = 1;

    /* XXX only the installer does not have the database open here. */
    if (rpmtsGetRdb(ts) != NULL) {
	if (Name[0] == '/') {
	    /* depFlags better be 0! */

	    mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, Name, 0);

	    (void) rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);

	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		rpmdsNotify(dep, _("(db files)"), rc);
		mi = rpmdbFreeIterator(mi);
		goto exit;
	    }
	    mi = rpmdbFreeIterator(mi);
	}

	mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, Name, 0);
	(void) rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (rpmdsAnyMatchesDep(h, dep, _rpmds_nopromote)) {
		rpmdsNotify(dep, _("(db provides)"), rc);
		mi = rpmdbFreeIterator(mi);
		goto exit;
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }

    /*
     * Search for an unsatisfied dependency.
     */
    if (adding && !retrying && !(rpmtsFlags(ts) & RPMTRANS_FLAG_NOSUGGEST)) {
	if (ts->solve != NULL) {
	    xx = (*ts->solve) (ts, dep, ts->solveData);
	    if (xx == 0)
		goto exit;
	    if (xx == -1) {
		retrying = 1;
		rpmalMakeIndex(ts->addedPackages);
		goto retry;
	    }
	}
    }

unsatisfied:
    rc = 1;	/* dependency is unsatisfied */
    rpmdsNotify(dep, NULL, rc);

exit:
    if (cacheThis) {
	char *key = xstrdup(DNEVR);
	depCacheAddEntry(dcache, key, rc);
    }
	
    return rc;
}

/**
 * Check added requires/conflicts against against installed+added packages.
 * @param ts		transaction set
 * @param pkgNEVRA	package name-version-release.arch
 * @param requires	Requires: dependencies (or NULL)
 * @param conflicts	Conflicts: dependencies (or NULL)
 * @param depName	dependency name to filter (or NULL)
 * @param tscolor	color bits for transaction set (0 disables)
 * @param adding	dependency is from added package set?
 * @return		0 no problems found
 */
static int checkPackageDeps(rpmts ts, depCache dcache, const char * pkgNEVRA,
		rpmds requires, rpmds conflicts,
		const char * depName, rpm_color_t tscolor, int adding)
{
    rpm_color_t dscolor;
    const char * Name;
    int rc;
    int ourrc = 0;

    requires = rpmdsInit(requires);
    if (requires != NULL)
    while (!ourrc && rpmdsNext(requires) >= 0) {

	if ((Name = rpmdsN(requires)) == NULL)
	    continue;	/* XXX can't happen */

	/* Filter out requires that came along for the ride. */
	if (depName != NULL && !rstreq(depName, Name))
	    continue;

	/* Ignore colored requires not in our rainbow. */
	dscolor = rpmdsColor(requires);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	rc = unsatisfiedDepend(ts, dcache, requires, adding);

	switch (rc) {
	case 0:		/* requirements are satisfied. */
	    break;
	case 1:		/* requirements are not satisfied. */
	    rpmdsProblem(ts->probs, pkgNEVRA, requires, NULL, adding);
	    break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    break;
	}
    }

    conflicts = rpmdsInit(conflicts);
    if (conflicts != NULL)
    while (!ourrc && rpmdsNext(conflicts) >= 0) {

	if ((Name = rpmdsN(conflicts)) == NULL)
	    continue;	/* XXX can't happen */

	/* Filter out conflicts that came along for the ride. */
	if (depName != NULL && !rstreq(depName, Name))
	    continue;

	/* Ignore colored conflicts not in our rainbow. */
	dscolor = rpmdsColor(conflicts);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	rc = unsatisfiedDepend(ts, dcache, conflicts, adding);

	/* 1 == unsatisfied, 0 == satsisfied */
	switch (rc) {
	case 0:		/* conflicts exist. */
	    rpmdsProblem(ts->probs, pkgNEVRA, conflicts, NULL, adding);
	    break;
	case 1:		/* conflicts don't exist. */
	    break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    break;
	}
    }

    return ourrc;
}

/**
 * Check dependency against installed packages.
 * Adding: check name/provides dep against each conflict match,
 * Erasing: check name/provides/filename dep against each requiredby match.
 * @param ts		transaction set
 * @param dep		dependency name
 * @param mi		rpm database iterator
 * @param adding	dependency is from added package set?
 * @return		0 no problems found
 */
static int checkPackageSet(rpmts ts, depCache dcache, const char * dep,
		rpmdbMatchIterator mi, int adding)
{
    Header h;
    int ec = 0;

    (void) rpmdbPruneIterator(mi,
		ts->removedPackages, ts->numRemovedPackages, 1);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	char * pkgNEVRA;
	rpmds requires, conflicts;
	int rc;

	pkgNEVRA = headerGetAsString(h, RPMTAG_NEVRA);
	requires = rpmdsNew(h, RPMTAG_REQUIRENAME, 0);
	(void) rpmdsSetNoPromote(requires, _rpmds_nopromote);
	conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, 0);
	(void) rpmdsSetNoPromote(conflicts, _rpmds_nopromote);
	rc = checkPackageDeps(ts, dcache, pkgNEVRA, requires, conflicts, dep, 0, adding);
	conflicts = rpmdsFree(conflicts);
	requires = rpmdsFree(requires);
	pkgNEVRA = _free(pkgNEVRA);

	if (rc) {
	    ec = 1;
	    break;
	}
    }
    mi = rpmdbFreeIterator(mi);

    return ec;
}

/**
 * Check to-be-erased dependencies against installed requires.
 * @param ts		transaction set
 * @param dep		requires name
 * @return		0 no problems found
 */
static int checkDependentPackages(rpmts ts, depCache dcache, const char * dep)
{
    rpmdbMatchIterator mi;
    mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, dep, 0);
    return checkPackageSet(ts, dcache, dep, mi, 0);
}

/**
 * Check to-be-added dependencies against installed conflicts.
 * @param ts		transaction set
 * @param dep		conflicts name
 * @return		0 no problems found
 */
static int checkDependentConflicts(rpmts ts, depCache dcache, const char * dep)
{
    int rc = 0;

    if (rpmtsGetRdb(ts) != NULL) {	/* XXX is this necessary? */
	rpmdbMatchIterator mi;
	mi = rpmtsInitIterator(ts, RPMTAG_CONFLICTNAME, dep, 0);
	rc = checkPackageSet(ts, dcache, dep, mi, 0);
    }

    return rc;
}

int rpmtsCheck(rpmts ts)
{
    rpm_color_t tscolor = rpmtsColor(ts);
    rpmdbMatchIterator mi = NULL;
    rpmtsi pi = NULL; rpmte p;
    int closeatexit = 0;
    int xx;
    int rc;
    depCache dcache = NULL;
    
    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    /* Do lazy, readonly, open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsGetDBMode(ts) != -1) {
	if ((rc = rpmtsOpenDB(ts, rpmtsGetDBMode(ts))) != 0)
	    goto exit;
	closeatexit = 1;
    }

    ts->probs = rpmpsFree(ts->probs);
    ts->probs = rpmpsCreate();

    rpmalMakeIndex(ts->addedPackages);

    /* XXX FIXME: figure some kind of heuristic for the cache size */
    dcache = depCacheCreate(5001, hashFunctionString, strcmp,
				     (depCacheFreeKey)rfree, NULL);

    /*
     * Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmds provides;

	/* FIX: rpmts{A,O} can return null. */
	rpmlog(RPMLOG_DEBUG, "========== +++ %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));
	rc = checkPackageDeps(ts, dcache, rpmteNEVRA(p),
			rpmteDS(p, RPMTAG_REQUIRENAME),
			rpmteDS(p, RPMTAG_CONFLICTNAME),
			NULL,
			tscolor, 1);
	if (rc)
	    goto exit;

	rc = 0;
	provides = rpmdsInit(rpmteDS(p, RPMTAG_PROVIDENAME));
	while (rpmdsNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = rpmdsN(provides)) == NULL)
		continue;	/* XXX can't happen */

	    /* Adding: check provides key against conflicts matches. */
	    if (!checkDependentConflicts(ts, dcache, Name))
		continue;
	    rc = 1;
	    break;
	}
	if (rc)
	    goto exit;
    }
    pi = rpmtsiFree(pi);

    /*
     * Look at the removed packages and make sure they aren't critical.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
	rpmds provides = rpmdsInit(rpmteDS(p, RPMTAG_PROVIDENAME));
	rpmfi fi;

	/* FIX: rpmts{A,O} can return null. */
	rpmlog(RPMLOG_DEBUG, "========== --- %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	rc = 0;
	while (rpmdsNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = rpmdsN(provides)) == NULL)
		continue;	/* XXX can't happen */

	    /* Erasing: check provides against requiredby matches. */
	    if (!checkDependentPackages(ts, dcache, Name))
		continue;
	    rc = 1;
	    break;
	}
	if (rc)
	    goto exit;

	rc = 0;
	fi = rpmteFI(p);
	fi = rpmfiInit(fi, 0);
	while (rpmfiNext(fi) >= 0) {
	    const char * fn = rpmfiFN(fi);

	    /* Erasing: check filename against requiredby matches. */
	    if (!checkDependentPackages(ts, dcache, fn))
		continue;
	    rc = 1;
	    break;
	}
	if (rc)
	    goto exit;
    }
    pi = rpmtsiFree(pi);

    rc = 0;

exit:
    mi = rpmdbFreeIterator(mi);
    pi = rpmtsiFree(pi);
    depCacheFree(dcache);

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    if (closeatexit)
	xx = rpmtsCloseDB(ts);
    return rc;
}
