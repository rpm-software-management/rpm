/** \ingroup rpmts
 * \file lib/depends.c
 */

#include "system.h"

#include <rpm/rpmlib.h>		/* rpmVersionCompare, rpmlib provides */
#include <rpm/rpmtag.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>

#include "lib/rpmts_internal.h"
#include "lib/rpmte_internal.h"
#include "lib/rpmds_internal.h"
#include "lib/misc.h"

#include "debug.h"

const char * const RPMVERSION = VERSION;

const char * const rpmNAME = PACKAGE;

const char * const rpmEVR = VERSION;

const int rpmFLAGS = RPMSENSE_EQUAL;

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

#define HASHTYPE removedHash
#define HTKEYTYPE unsigned int
#define HTDATATYPE struct rpmte_s *
#include "rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

#define HASHTYPE conflictsCache
#define HTKEYTYPE const char *
#include "rpmhash.H"
#include "rpmhash.C"
#undef HASHTYPE
#undef HTKEYTYPE

/**
 * Check for supported payload format in header.
 * @param h		header to check
 * @return		RPMRC_OK if supported, RPMRC_FAIL otherwise
 */
static rpmRC headerCheckPayloadFormat(Header h) {
    rpmRC rc = RPMRC_OK;
    const char *payloadfmt = headerGetString(h, RPMTAG_PAYLOADFORMAT);
    /* 
     * XXX Ugh, rpm 3.x packages don't have payload format tag. Instead
     * of blindly allowing, should check somehow (HDRID existence or... ?)
     */
    if (!payloadfmt) return rc;

    if (!rstreq(payloadfmt, "cpio")) {
        char *nevra = headerGetAsString(h, RPMTAG_NEVRA);
        if (payloadfmt && rstreq(payloadfmt, "drpm")) {
            rpmlog(RPMLOG_ERR,
                     _("%s is a Delta RPM and cannot be directly installed\n"),
                     nevra);
        } else {
            rpmlog(RPMLOG_ERR, 
                     _("Unsupported payload (%s) in package %s\n"),
                     payloadfmt ? payloadfmt : "none", nevra);
        } 
        free(nevra);
	rc = RPMRC_FAIL;
    }
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
    tsMembers tsmem = rpmtsMembers(ts);
    rpmte p;
    unsigned int dboffset = headerGetInstance(h);

    /* Can't remove what's not installed */
    if (dboffset == 0) return 1;

    /* Filter out duplicate erasures. */
    if (removedHashHasEntry(tsmem->removedPackages, dboffset)) {
        return 0;
    }

    p = rpmteNew(ts, h, TR_REMOVED, NULL, NULL);
    if (p == NULL)
	return 1;

    removedHashAddEntry(tsmem->removedPackages, dboffset, p);

    if (tsmem->orderCount >= tsmem->orderAlloced) {
	tsmem->orderAlloced += (tsmem->orderCount - tsmem->orderAlloced) + tsmem->delta;
	tsmem->order = xrealloc(tsmem->order, sizeof(*tsmem->order) * tsmem->orderAlloced);
    }

    rpmteSetDependsOn(p, depends);

    tsmem->order[tsmem->orderCount] = p;
    tsmem->orderCount++;

    return 0;
}

/* Return rpmdb iterator with removals optionally pruned out */
static rpmdbMatchIterator rpmtsPrunedIterator(rpmts ts, rpmDbiTagVal tag,
					      const char * key, int prune)
{
    rpmdbMatchIterator mi = rpmtsInitIterator(ts, tag, key, 0);
    if (prune) {
	tsMembers tsmem = rpmtsMembers(ts);
	rpmdbPruneIterator(mi, tsmem->removedPackages);
    }
    return mi;
}

/**
 * Decides whether to skip a package upgrade/obsoletion on TE color.
 *
 * @param tscolor	color of this transaction
 * @param color 	color of this transaction element
 * @param ocolor 	header color of the upgraded/obsoleted package
 *
 * @return		non-zero if the package should be skipped
 */
static int skipColor(rpm_color_t tscolor, rpm_color_t color, rpm_color_t ocolor)
{
    return tscolor && color && ocolor && !(color & ocolor);
}

/* Add erase elements for older packages of same color (if any). */
static int addUpgradeErasures(rpmts ts, rpm_color_t tscolor,
				rpmte p, rpm_color_t hcolor, Header h)
{
    Header oh;
    rpmdbMatchIterator mi = rpmtsInitIterator(ts, RPMDBI_NAME, rpmteN(p), 0);
    int rc = 0;

    while((oh = rpmdbNextIterator(mi)) != NULL) {
	/* Ignore colored packages not in our rainbow. */
	if (skipColor(tscolor, hcolor, headerGetNumber(oh, RPMTAG_HEADERCOLOR)))
	    continue;

	/* Skip packages that contain identical NEVR. */
	if (rpmVersionCompare(h, oh) == 0)
	    continue;

	if (removePackage(ts, oh, p)) {
	    rc = 1;
	    break;
	}
    }
    rpmdbFreeIterator(mi);
    return rc;
}

/* Add erase elements for obsoleted packages of same color (if any). */
static int addObsoleteErasures(rpmts ts, rpm_color_t tscolor, rpmte p)
{
    rpmstrPool tspool = rpmtsPool(ts);
    rpmds obsoletes = rpmdsInit(rpmteDS(p, RPMTAG_OBSOLETENAME));
    Header oh;
    int rc = 0;

    while (rpmdsNext(obsoletes) >= 0 && rc == 0) {
	const char * Name;
	rpmdbMatchIterator mi = NULL;

	if ((Name = rpmdsN(obsoletes)) == NULL)
	    continue;	/* XXX can't happen */

	mi = rpmtsPrunedIterator(ts, RPMDBI_NAME, Name, 1);

	while((oh = rpmdbNextIterator(mi)) != NULL) {
	    const char *oarch = headerGetString(oh, RPMTAG_ARCH);
	    int match;

	    /* avoid self-obsoleting packages */
	    if (rstreq(rpmteN(p), Name) && rstreq(rpmteA(p), oarch)) {
		char * ohNEVRA = headerGetAsString(oh, RPMTAG_NEVRA);
		rpmlog(RPMLOG_DEBUG, "  Not obsoleting: %s\n", ohNEVRA);
		free(ohNEVRA);
		continue;
	    }

	    /*
	     * Rpm prior to 3.0.3 does not have versioned obsoletes.
	     * If no obsoletes version info is available, match all names.
	     */
	    match = (rpmdsEVR(obsoletes) == NULL);
	    if (!match)
		match = rpmdsMatches(tspool, oh, -1, obsoletes, 1,
					 _rpmds_nopromote);

	    if (match) {
		char * ohNEVRA = headerGetAsString(oh, RPMTAG_NEVRA);
		rpmlog(RPMLOG_DEBUG, "  Obsoletes: %s\t\terases %s\n",
			rpmdsDNEVR(obsoletes)+2, ohNEVRA);
		free(ohNEVRA);

		if (removePackage(ts, oh, p)) {
		    rc = 1;
		    break;
		}
	    }
	}
	rpmdbFreeIterator(mi);
    }
    return rc;
}

/*
 * Lookup obsoletions in the added set. In theory there could
 * be more than one obsoleting package, but we only care whether this
 * has been obsoleted by *something* or not.
 */
static rpmte checkObsoleted(rpmal addedPackages, rpmds thisds)
{
    rpmte p = NULL;
    rpmte *matches = NULL;

    matches = rpmalAllObsoletes(addedPackages, thisds);
    if (matches) {
	p = matches[0];
	free(matches);
    }
    return p;
}

/*
 * Filtered rpmal lookup: on colored transactions there can be more
 * than one identical NEVR but different arch, this must be allowed.
 * Only a single element needs to be considred as there can only ever
 * be one previous element to be replaced.
 */
static rpmte checkAdded(rpmal addedPackages, rpm_color_t tscolor,
			rpmte te, rpmds ds)
{
    rpmte p = NULL;
    rpmte *matches = NULL;

    matches = rpmalAllSatisfiesDepend(addedPackages, ds);
    if (matches) {
	const char * arch = rpmteA(te);
	const char * os = rpmteO(te);

	for (rpmte *m = matches; m && *m; m++) {
	    if (tscolor) {
		const char * parch = rpmteA(*m);
		const char * pos = rpmteO(*m);

		if (arch == NULL || parch == NULL || os == NULL || pos == NULL)
		    continue;
		if (!rstreq(arch, parch) || !rstreq(os, pos))
		    continue;
	    }
	    p = *m;
	    break;
  	}
	free(matches);
    }
    return p;
}

/*
 * Check for previously added versions and obsoletions.
 * Return index where to place this element, or -1 to skip.
 * XXX OBSOLETENAME is a bit of a hack, but gives us what
 * we want from rpmal: we're only interested in added package
 * names here, not their provides.
 */
static int findPos(rpmts ts, rpm_color_t tscolor, rpmte te, int upgrade)
{
    tsMembers tsmem = rpmtsMembers(ts);
    int oc = tsmem->orderCount;
    int skip = 0;
    const char * name = rpmteN(te);
    const char * evr = rpmteEVR(te);
    rpmte p;
    rpmstrPool tspool = rpmtsPool(ts);
    rpmds oldChk = rpmdsSinglePool(tspool, RPMTAG_OBSOLETENAME,
				   name, evr, (RPMSENSE_LESS));
    rpmds newChk = rpmdsSinglePool(tspool, RPMTAG_OBSOLETENAME,
				   name, evr, (RPMSENSE_GREATER));
    rpmds sameChk = rpmdsSinglePool(tspool, RPMTAG_OBSOLETENAME,
				    name, evr, (RPMSENSE_EQUAL));
    rpmds obsChk = rpmteDS(te, RPMTAG_OBSOLETENAME);

    /* If obsoleting package has already been added, skip this. */
    if ((p = checkObsoleted(tsmem->addedPackages, rpmteDS(te, RPMTAG_NAME)))) {
	skip = 1;
	goto exit;
    }

    /* If obsoleted package has already been added, replace with this. */
    rpmdsInit(obsChk);
    while (rpmdsNext(obsChk) >= 0) {
	/* XXX Obsoletes are not colored */
	if ((p = checkAdded(tsmem->addedPackages, 0, te, obsChk))) {
	    goto exit;
	}
    }

    /* If same NEVR has already been added, skip this. */
    if ((p = checkAdded(tsmem->addedPackages, tscolor, te, sameChk))) {
	skip = 1;
	goto exit;
    }

    /* On upgrades... */
    if (upgrade) {
	/* ...if newer NEVR has already been added, skip this. */
	if ((p = checkAdded(tsmem->addedPackages, tscolor, te, newChk))) {
	    skip = 1;
	    goto exit;
	}

	/* ...if older NEVR has already been added, replace with this. */
	if ((p = checkAdded(tsmem->addedPackages, tscolor, te, oldChk))) {
	    goto exit;
	}
    }

exit:
    /* If we found a previous element we've something to say */
    if (p != NULL && rpmIsVerbose()) {
	const char *msg = skip ?
		    _("package %s was already added, skipping %s\n") :
		    _("package %s was already added, replacing with %s\n");
	rpmlog(RPMLOG_WARNING, msg, rpmteNEVRA(p), rpmteNEVRA(te));
    }

    /* If replacing a previous element, find out where it is. Pooh. */
    if (!skip && p != NULL) {
	for (oc = 0; oc < tsmem->orderCount; oc++) {
	    if (p == tsmem->order[oc])
		break;
	}
    }

    rpmdsFree(oldChk);
    rpmdsFree(newChk);
    rpmdsFree(sameChk);
    return (skip) ? -1 : oc;
}

rpmal rpmtsCreateAl(rpmts ts, rpmElementTypes types)
{
    rpmal al = NULL;
    if (ts) {
	rpmte p;
	rpmtsi pi;
	rpmstrPool tspool = rpmtsPool(ts);

	al = rpmalCreate(tspool, (rpmtsNElements(ts) / 4) + 1, rpmtsFlags(ts),
				rpmtsColor(ts), rpmtsPrefColor(ts));
	pi = rpmtsiInit(ts);
	while ((p = rpmtsiNext(pi, types)))
	    rpmalAdd(al, p);
	rpmtsiFree(pi);
    }
    return al;
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

    /* Source packages are never "upgraded" */
    if (isSource)
	upgrade = 0;

    /* Do lazy (readonly?) open of rpm database for upgrades. */
    if (upgrade && rpmtsGetRdb(ts) == NULL && rpmtsGetDBMode(ts) != -1) {
	if ((ec = rpmtsOpenDB(ts, rpmtsGetDBMode(ts))) != 0)
	    goto exit;
    }

    p = rpmteNew(ts, h, TR_ADDED, key, relocs);
    if (p == NULL) {
	ec = 1;
	goto exit;
    }

    /* Check binary packages for redundancies in the set */
    if (!isSource) {
	oc = findPos(ts, tscolor, p, upgrade);
	/* If we're replacing a previously added element, free the old one */
	if (oc >= 0 && oc < tsmem->orderCount) {
	    rpmalDel(tsmem->addedPackages, tsmem->order[oc]);
	    tsmem->order[oc] = rpmteFree(tsmem->order[oc]);
	/* If newer NEVR was already added, we're done */
	} else if (oc < 0) {
	    p = rpmteFree(p);
	    goto exit;
	}
    }

    if (oc >= tsmem->orderAlloced) {
	tsmem->orderAlloced += (oc - tsmem->orderAlloced) + tsmem->delta;
	tsmem->order = xrealloc(tsmem->order,
			tsmem->orderAlloced * sizeof(*tsmem->order));
    }


    tsmem->order[oc] = p;
    if (oc == tsmem->orderCount) {
	tsmem->orderCount++;
    }
    
    if (tsmem->addedPackages == NULL) {
	tsmem->addedPackages = rpmalCreate(rpmtsPool(ts), 5, rpmtsFlags(ts),
					   tscolor, rpmtsPrefColor(ts));
    }
    rpmalAdd(tsmem->addedPackages, p);

    /* Add erasure elements for old versions and obsoletions on upgrades */
    /* XXX TODO: If either of these fails, we'd need to undo all additions */
    if (upgrade) {
	addUpgradeErasures(ts, tscolor, p, rpmteColor(p), h);
	addObsoleteErasures(ts, tscolor, p);
    }

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
    rpmTagVal deptag = rpmdsTagN(dep);
    int *cachedrc = NULL;
    rpmdbMatchIterator mi = NULL;
    Header h = NULL;
    int rc = 0;
    /* pretrans deps are provided by current packages, don't prune erasures */
    int prune = (rpmdsFlags(dep) & RPMSENSE_PRETRANS) ? 0 : 1;
    unsigned int keyhash = 0;

    /* See if we already looked this up */
    if (prune) {
	keyhash = depCacheKeyHash(dcache, DNEVR);
	if (depCacheGetHEntry(dcache, DNEVR, keyhash, &cachedrc, NULL, NULL)) {
	    rc = *cachedrc;
	    rpmdsNotify(dep, "(cached)", rc);
	    return rc;
	}
    }

    /*
     * See if a filename dependency is a real file in some package,
     * taking file state into account: replaced, wrong colored and
     * not installed files can not satisfy a dependency.
     */
    if (deptag != RPMTAG_OBSOLETENAME && Name[0] == '/') {
	mi = rpmtsPrunedIterator(ts, RPMDBI_INSTFILENAMES, Name, prune);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    rpmdsNotify(dep, "(db files)", rc);
	    break;
	}
	rpmdbFreeIterator(mi);
    }

    /* Otherwise look in provides no matter what the dependency looks like */
    if (h == NULL) {
	rpmstrPool tspool = rpmtsPool(ts);
	/* Obsoletes use just name alone, everything else uses provides */
	rpmTagVal dbtag = RPMDBI_PROVIDENAME;
	int selfevr = 0;
	if (deptag == RPMTAG_OBSOLETENAME) {
	    dbtag = RPMDBI_NAME;
	    selfevr = 1;
	}

	mi = rpmtsPrunedIterator(ts, dbtag, Name, prune);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    /* Provide-indexes can't be used with nevr-only matching */
	    int prix = (selfevr) ? -1 : rpmdbGetIteratorFileNum(mi);
	    int match = rpmdsMatches(tspool, h, prix, dep, selfevr,
					_rpmds_nopromote);
	    if (match) {
		rpmdsNotify(dep, "(db provides)", rc);
		break;
	    }
	}
	rpmdbFreeIterator(mi);
    }
    rc = (h != NULL) ? 0 : 1;

    /* Cache the relatively expensive rpmdb lookup results */
    /* Caching the oddball non-pruned case would mess up other results */
    if (prune)
	depCacheAddHEntry(dcache, xstrdup(DNEVR), keyhash, rc);
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
	if (tsmem->rpmlib == NULL)
	    rpmdsRpmlibPool(rpmtsPool(ts), &(tsmem->rpmlib), NULL);
	
	if (tsmem->rpmlib != NULL && rpmdsSearch(tsmem->rpmlib, dep) >= 0) {
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
	rpmte *matches = rpmalAllSatisfiesDepend(tsmem->addedPackages, dep);
	int match = 0;

	/*
	 * Handle definitive matches within the added package set.
	 * Self-obsoletes and -conflicts fall through here as we need to 
	 * check for possible other matches in the rpmdb.
	 */
	for (rpmte *m = matches; m && *m; m++) {
	    rpmTagVal dtag = rpmdsTagN(dep);
	    /* Requires match, look no further */
	    if (dtag == RPMTAG_REQUIRENAME) {
		match = 1;
		break;
	    }

	    /* Conflicts/obsoletes match on another package, look no further */
	    if (rpmteDS(*m, dtag) != dep) {
		match = 1;
		break;
	    }
	}
	free(matches);
	if (match)
	    goto exit;
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
    if (dsflags & RPMSENSE_MISSINGOK) {
	/* note the result, but missingok deps are never unsatisfied */
	rpmdsNotify(dep, "(missingok)", 1);
    } else {
	/* dependency is unsatisfied */
	rc = 1;
	rpmdsNotify(dep, NULL, rc);
    }

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
    rpmdbMatchIterator mi = rpmtsPrunedIterator(ts, depTag, dep, 1);
    rpmstrPool pool = rpmtsPool(ts);

    while ((h = rpmdbNextIterator(mi)) != NULL) {
	char * pkgNEVRA = headerGetAsString(h, RPMTAG_NEVRA);
	rpmds ds = rpmdsNewPool(pool, h, depTag, 0);

	checkDS(ts, dcache, te, pkgNEVRA, ds, dep, 0);

	rpmdsFree(ds);
	free(pkgNEVRA);
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
    conflictsCache confcache = NULL;
    
    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    /* Do lazy, readonly, open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && rpmtsGetDBMode(ts) != -1) {
	if ((rc = rpmtsOpenDB(ts, rpmtsGetDBMode(ts))) != 0)
	    goto exit;
	closeatexit = 1;
    }

    /* XXX FIXME: figure some kind of heuristic for the cache size */
    dcache = depCacheCreate(5001, rstrhash, strcmp,
				     (depCacheFreeKey)rfree, NULL);

    confcache = conflictsCacheCreate(257, rstrhash, strcmp,
				     (depCacheFreeKey)rfree);
    if (confcache) {
	rpmdbIndexIterator ii = rpmdbIndexIteratorInit(rpmtsGetRdb(ts), RPMTAG_CONFLICTNAME);
	if (ii) {
	    char *key;
	    size_t keylen;
	    while ((rpmdbIndexIteratorNext(ii, (const void**)&key, &keylen)) == 0) {
		char *k;
		if (!key || keylen == 0 || key[0] != '/')
		    continue;
		k = rmalloc(keylen + 1);
		memcpy(k, key, keylen);
		k[keylen] = 0;
		conflictsCacheAddEntry(confcache, k);
	    }
	    rpmdbIndexIteratorFree(ii);
	}
    }

    
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
	checkDS(ts, dcache, p, rpmteNEVRA(p), rpmteDS(p, RPMTAG_OBSOLETENAME),
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

	/* Check filenames against installed conflicts */
        if (conflictsCacheNumKeys(confcache)) {
	    rpmfi fi = rpmfiInit(rpmteFI(p), 0);
	    while (rpmfiNext(fi) >= 0) {
		const char *fn = rpmfiFN(fi);
		if (!conflictsCacheHasEntry(confcache, fn))
		    continue;
		checkInstDeps(ts, dcache, p, RPMTAG_CONFLICTNAME, fn);
	    }
	}
    }
    rpmtsiFree(pi);

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
	    if (RPMFILE_IS_INSTALLED(rpmfiFState(fi)))
		checkInstDeps(ts, dcache, p, RPMTAG_REQUIRENAME, rpmfiFN(fi));
	}
    }
    rpmtsiFree(pi);

exit:
    depCacheFree(dcache);
    conflictsCacheFree(confcache);

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    if (closeatexit)
	(void) rpmtsCloseDB(ts);
    return rc;
}
