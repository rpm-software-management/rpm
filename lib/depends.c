/** \ingroup rpmts
 * \file lib/depends.c
 */

#include "system.h"

#include <rpm/rpmcli.h>		/* XXX rpmcliPackagesTotal */

#include <rpm/rpmlib.h>		/* rpmVersionCompare, rpmlib provides */
#include <rpm/rpmtag.h>
#include <rpm/rpmmacro.h>       /* XXX rpmExpand("%{_dependency_whiteout */
#include <rpm/rpmlog.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>

#include "lib/rpmdb_internal.h"	/* XXX response cache needs dbiOpen et al. */
#include "lib/rpmte_internal.h"	/* XXX tsortInfo_s */
#include "lib/rpmts_internal.h"

#include "debug.h"


static int _cacheDependsRC = 1;

const char * const rpmNAME = PACKAGE;

const char * const rpmEVR = VERSION;

const int rpmFLAGS = RPMSENSE_EQUAL;

/* rpmlib provides */
static rpmds rpmlibP = NULL;

/*
 * Strongly Connected Components
 * set of packages (indirectly) requiering each other
 */
struct scc_s {
    int count; /* # of external requires this SCC has */
    /* int qcnt;  # of external requires pointing to this SCC */
    int size;  /* # of members */
    rpmte * members;
};

typedef struct scc_s * scc;

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
    rpm_color_t dscolor;
    rpm_color_t hcolor;
    rpm_color_t ohcolor;
    rpmdbMatchIterator mi;
    Header oh;
    int isSource;
    int duplicate = 0;
    rpmtsi pi = NULL; rpmte p;
    struct rpmtd_s td;
    const char * arch = NULL;
    const char * os = NULL;
    rpmds oldChk = NULL, newChk = NULL, sameChk = NULL;
    rpmds obsoletes;
    int xx;
    int ec = 0;
    int rc;
    int oc;

    /*
     * Check for previously added versions with the same name and arch/os.
     * FIXME: only catches previously added, older packages.
     */
    if (headerGet(h, RPMTAG_ARCH, &td, HEADERGET_MINMEM))
	arch = rpmtdGetString(&td);
    if (headerGet(h, RPMTAG_OS, &td, HEADERGET_MINMEM))
	os = rpmtdGetString(&td);
    hcolor = headerGetColor(h);

    /* Check for supported payload format if it's a package */
    if (key && headerCheckPayloadFormat(h) != RPMRC_OK) {
	ec = 1;
	goto exit;
    }

    if (ts->addedPackages == NULL) {
	ts->addedPackages = rpmalCreate(5, tscolor);
    }

    /* XXX Always add source headers. */
    isSource = headerIsSource(h);
    if (isSource) {
	oc = ts->orderCount;
	goto addheader;
    }

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
	    const char * parch;
	    const char * pos;

	    if (arch == NULL || (parch = rpmteA(p)) == NULL)
		continue;
	    if (os == NULL || (pos = rpmteO(p)) == NULL)
		continue;
	    if (strcmp(arch, parch) || strcmp(os, pos))
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

    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
    while((oh = rpmdbNextIterator(mi)) != NULL) {

	/* Ignore colored packages not in our rainbow. */
	ohcolor = headerGetColor(oh);
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

	/* Ignore colored obsoletes not in our rainbow. */
#if 0
	dscolor = rpmdsColor(obsoletes);
#else
	dscolor = hcolor;
#endif
	/* XXX obsoletes are never colored, so this is for future devel. */
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	/* XXX avoid self-obsoleting packages. */
	if (!strcmp(rpmteN(p), Name))
	    continue;

	if (Name[0] == '/')
	    mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, Name, 0);
	else
	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, Name, 0);

	xx = rpmdbPruneIterator(mi,
	    ts->removedPackages, ts->numRemovedPackages, 1);

	while((oh = rpmdbNextIterator(mi)) != NULL) {
	    /* Ignore colored packages not in our rainbow. */
	    ohcolor = headerGetColor(oh);
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
		char * ohNEVRA = headerGetNEVRA(oh, NULL);
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
static int unsatisfiedDepend(rpmts ts, rpmds dep, int adding)
{
    DBT key;
    DBT data;
    rpmdbMatchIterator mi;
    const char * Name;
    Header h;
    int _cacheThisRC = 1;
    int rc;
    int xx;
    int retrying = 0;

    if ((Name = rpmdsN(dep)) == NULL)
	return 0;	/* XXX can't happen */

    /*
     * Check if dbiOpen/dbiPut failed (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(rpmtsGetRdb(ts), RPMDBI_DEPENDS, 0);
	if (dbi == NULL)
	    _cacheDependsRC = 0;
	else {
	    const char * DNEVR;

	    rc = -1;
	    if ((DNEVR = rpmdsDNEVR(dep)) != NULL) {
		DBC * dbcursor = NULL;
		void * datap = NULL;
		size_t datalen = 0;
		size_t DNEVRlen = strlen(DNEVR);

		xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

		memset(&key, 0, sizeof(key));
		key.data = (void *) DNEVR;
		key.size = DNEVRlen;
		memset(&data, 0, sizeof(data));
		data.data = datap;
		data.size = datalen;
/* FIX: data->data may be NULL */
		xx = dbiGet(dbi, dbcursor, &key, &data, DB_SET);
		DNEVR = key.data;
		DNEVRlen = key.size;
		datap = data.data;
		datalen = data.size;

		if (xx == 0 && datap && datalen == 4)
		    memcpy(&rc, datap, datalen);
		xx = dbiCclose(dbi, dbcursor, 0);
	    }

	    if (rc >= 0) {
		rpmdsNotify(dep, _("(cached)"), rc);
		return rc;
	    }
	}
    }

retry:
    rc = 0;	/* assume dependency is satisfied */

    /*
     * New features in rpm packaging implicitly add versioned dependencies
     * on rpmlib provides. The dependencies look like "rpmlib(YaddaYadda)".
     * Check those dependencies now.
     */
    if (!strncmp(Name, "rpmlib(", sizeof("rpmlib(")-1)) {
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
	/*
	 * XXX Ick, context sensitive answers from dependency cache.
	 * XXX Always resolve added dependencies within context to disambiguate.
	 */
	if (_rpmds_nopromote)
	    _cacheThisRC = 0;
	goto exit;
    }

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
    /*
     * If dbiOpen/dbiPut fails (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC && _cacheThisRC) {
	dbiIndex dbi;
	dbi = dbiOpen(rpmtsGetRdb(ts), RPMDBI_DEPENDS, 0);
	if (dbi == NULL) {
	    _cacheDependsRC = 0;
	} else {
	    const char * DNEVR;
	    xx = 0;
	    if ((DNEVR = rpmdsDNEVR(dep)) != NULL) {
		DBC * dbcursor = NULL;
		size_t DNEVRlen = strlen(DNEVR);

		xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

		memset(&key, 0, sizeof(key));
		key.data = (void *) DNEVR;
		key.size = DNEVRlen;
		memset(&data, 0, sizeof(data));
		data.data = &rc;
		data.size = sizeof(rc);

		xx = dbiPut(dbi, dbcursor, &key, &data, 0);
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    }
	    if (xx)
		_cacheDependsRC = 0;
	}
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
static int checkPackageDeps(rpmts ts, const char * pkgNEVRA,
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
	if (depName != NULL && strcmp(depName, Name))
	    continue;

	/* Ignore colored requires not in our rainbow. */
	dscolor = rpmdsColor(requires);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	rc = unsatisfiedDepend(ts, requires, adding);

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
	if (depName != NULL && strcmp(depName, Name))
	    continue;

	/* Ignore colored conflicts not in our rainbow. */
	dscolor = rpmdsColor(conflicts);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	rc = unsatisfiedDepend(ts, conflicts, adding);

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
static int checkPackageSet(rpmts ts, const char * dep,
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

	pkgNEVRA = headerGetNEVRA(h, NULL);
	requires = rpmdsNew(h, RPMTAG_REQUIRENAME, 0);
	(void) rpmdsSetNoPromote(requires, _rpmds_nopromote);
	conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, 0);
	(void) rpmdsSetNoPromote(conflicts, _rpmds_nopromote);
	rc = checkPackageDeps(ts, pkgNEVRA, requires, conflicts, dep, 0, adding);
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
static int checkDependentPackages(rpmts ts, const char * dep)
{
    rpmdbMatchIterator mi;
    mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, dep, 0);
    return checkPackageSet(ts, dep, mi, 0);
}

/**
 * Check to-be-added dependencies against installed conflicts.
 * @param ts		transaction set
 * @param dep		conflicts name
 * @return		0 no problems found
 */
static int checkDependentConflicts(rpmts ts, const char * dep)
{
    int rc = 0;

    if (rpmtsGetRdb(ts) != NULL) {	/* XXX is this necessary? */
	rpmdbMatchIterator mi;
	mi = rpmtsInitIterator(ts, RPMTAG_CONFLICTNAME, dep, 0);
	rc = checkPackageSet(ts, dep, mi, 1);
    }

    return rc;
}

struct badDeps_s {
    char * pname;
    const char * qname;
};

static int badDepsInitialized = 0;
static struct badDeps_s * badDeps = NULL;

/**
 */
static void freeBadDeps(void)
{
    if (badDeps) {
	struct badDeps_s * bdp;
	/* bdp->qname is a pointer to pname so doesn't need freeing */
	for (bdp = badDeps; bdp->pname != NULL && bdp->qname != NULL; bdp++)
	    bdp->pname = _free(bdp->pname);
	badDeps = _free(badDeps);
    }
    badDepsInitialized = 0;
}

/**
 * Check for dependency relations to be ignored.
 *
 * @param ts		transaction set
 * @param p		successor element (i.e. with Requires: )
 * @param q		predecessor element (i.e. with Provides: )
 * @return		1 if dependency is to be ignored.
 */
static int ignoreDep(const rpmts ts, const rpmte p, const rpmte q)
{
    struct badDeps_s * bdp;

    if (!badDepsInitialized) {
	char * s = rpmExpand("%{?_dependency_whiteout}", NULL);
	const char ** av = NULL;
	int msglvl = (rpmtsFlags(ts) & RPMTRANS_FLAG_DEPLOOPS)
			? RPMLOG_WARNING : RPMLOG_DEBUG;
	int ac = 0;
	int i;

	if (s != NULL && *s != '\0'
	&& !(i = poptParseArgvString(s, &ac, (const char ***)&av))
	&& ac > 0 && av != NULL)
	{
	    bdp = badDeps = xcalloc(ac+1, sizeof(*badDeps));
	    for (i = 0; i < ac; i++, bdp++) {
		char * pname, * qname;

		if (av[i] == NULL)
		    break;
		pname = xstrdup(av[i]);
		if ((qname = strchr(pname, '>')) != NULL)
		    *qname++ = '\0';
		bdp->pname = pname;
		bdp->qname = qname;
		rpmlog(msglvl,
			_("ignore package name relation(s) [%d]\t%s -> %s\n"),
			i, bdp->pname, (bdp->qname ? bdp->qname : "???"));
	    }
	    bdp->pname = NULL;
	    bdp->qname = NULL;
	}
	av = _free(av);
	s = _free(s);
	badDepsInitialized++;
    }

    if (badDeps != NULL)
    for (bdp = badDeps; bdp->pname != NULL && bdp->qname != NULL; bdp++) {
	if (!strcmp(rpmteN(p), bdp->pname) && !strcmp(rpmteN(q), bdp->qname))
	    return 1;
    }
    return 0;
}

static inline const char * identifyDepend(rpmsenseFlags f)
{
    f = _notpre(f);
    if (f & RPMSENSE_SCRIPT_PRE)
	return "Requires(pre):";
    if (f & RPMSENSE_SCRIPT_POST)
	return "Requires(post):";
    if (f & RPMSENSE_SCRIPT_PREUN)
	return "Requires(preun):";
    if (f & RPMSENSE_SCRIPT_POSTUN)
	return "Requires(postun):";
    if (f & RPMSENSE_SCRIPT_VERIFY)
	return "Requires(verify):";
    if (f & RPMSENSE_FIND_REQUIRES)
	return "Requires(auto):";
    return "Requires:";
}

/**
 * Record next "q <- p" relation (i.e. "p" requires "q").
 * @param ts		transaction set
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param requires	relation
 * @return		0 always
 */
static inline int addRelation(rpmts ts,
			      rpmal al,
			      rpmte p,
			      rpmds requires)
{
    rpmte q;
    struct tsortInfo_s * tsi_p, * tsi_q;
    relation rel;
    const char * Name;
    rpmElementType teType = rpmteType(p);
    rpmsenseFlags flags, dsflags;

    if ((Name = rpmdsN(requires)) == NULL)
	return 0;

    dsflags = rpmdsFlags(requires);

    /* Avoid rpmlib feature and package config dependencies */
    if (dsflags & (RPMSENSE_RPMLIB|RPMSENSE_CONFIG))
	return 0;

    q = rpmalSatisfiesDepend(al, requires);

    /* Avoid deps outside this transaction and self dependencies */
    if (q == NULL || q == p)
	return 0;

    /* Avoid certain dependency relations. */
    if (ignoreDep(ts, p, q))
	return 0;

    /* Erasures are reversed installs. */
    if (teType == TR_REMOVED) {
        rpmte r = p;
        p = q;
        q = r;
	flags = isErasePreReq(dsflags);
    } else {
	flags = isInstallPreReq(dsflags);
    }

    /* map legacy prereq to pre/preun as needed */
    if (isLegacyPreReq(dsflags)) {
	flags |= (teType == TR_ADDED) ?
		 RPMSENSE_SCRIPT_PRE : RPMSENSE_SCRIPT_PREUN;
    }

    tsi_p = rpmteTSI(p);
    tsi_q = rpmteTSI(q);

    /* if relation got already added just update the flags */
    if (tsi_q->tsi_relations && tsi_q->tsi_relations->rel_suc == p) {
	tsi_q->tsi_relations->rel_flags |= flags;
	tsi_p->tsi_forward_relations->rel_flags |= flags;
	return 0;
    }

    /* Record next "q <- p" relation (i.e. "p" requires "q"). */
    if (p != q) {
	/* bump p predecessor count */
	rpmteTSI(p)->tsi_count++;
    }

    /* Save max. depth in dependency tree */
    if (rpmteDepth(p) <= rpmteDepth(q))
	(void) rpmteSetDepth(p, (rpmteDepth(q) + 1));
    if (rpmteDepth(p) > ts->maxDepth)
	ts->maxDepth = rpmteDepth(p);

    rel = xcalloc(1, sizeof(*rel));
    rel->rel_suc = p;
    rel->rel_flags = flags;

    rel->rel_next = rpmteTSI(q)->tsi_relations;
    rpmteTSI(q)->tsi_relations = rel;
    if (p != q) {
	/* bump q successor count */
	rpmteTSI(q)->tsi_qcnt++;
    }

    rel = xcalloc(1, sizeof(*rel));
    rel->rel_suc = q;
    rel->rel_flags = flags;

    rel->rel_next = rpmteTSI(p)->tsi_forward_relations;
    rpmteTSI(p)->tsi_forward_relations = rel;

    return 0;
}

/**
 * Add element to list sorting by tsi_qcnt.
 * @param p		new element
 * @retval qp		address of first element
 * @retval rp		address of last element
 * @param prefcolor
 */
static void addQ(rpmte p,
		rpmte * qp,
		rpmte * rp,
		rpm_color_t prefcolor)
{
    rpmte q, qprev;

    /* Mark the package as queued. */
    rpmteTSI(p)->tsi_reqx = 1;

    if ((*rp) == NULL) {	/* 1st element */
	/* FIX: double indirection */
	(*rp) = (*qp) = p;
	return;
    }

    /* Find location in queue using metric tsi_qcnt. */
    for (qprev = NULL, q = (*qp);
	 q != NULL;
	 qprev = q, q = rpmteTSI(q)->tsi_suc)
    {
	/* XXX Insure preferred color first. */
	if (rpmteColor(p) != prefcolor && rpmteColor(p) != rpmteColor(q))
	    continue;

	if (rpmteTSI(q)->tsi_qcnt <= rpmteTSI(p)->tsi_qcnt)
	    break;
    }

    if (qprev == NULL) {	/* insert at beginning of list */
	rpmteTSI(p)->tsi_suc = q;
	(*qp) = p;		/* new head */
    } else if (q == NULL) {	/* insert at end of list */
	rpmteTSI(qprev)->tsi_suc = p;
	(*rp) = p;		/* new tail */
    } else {			/* insert between qprev and q */
	rpmteTSI(p)->tsi_suc = q;
	rpmteTSI(qprev)->tsi_suc = p;
    }
}

/* Search for SCCs and return an array last entry has a .size of 0 */
static scc detectSCCs(rpmts ts)
{
    int index = 0;                /* DFS node number counter */
    rpmte stack[ts->orderCount];  /* An empty stack of nodes */
    int stackcnt = 0;
    rpmtsi pi;
    rpmte p;

    int sccCnt = 2;
    scc SCCs = xcalloc(ts->orderCount+3, sizeof(struct scc_s));

    auto void tarjan(rpmte p);

    void tarjan(rpmte p) {
	tsortInfo tsi = rpmteTSI(p);
	rpmte q;
	tsortInfo tsi_q;
	relation rel;

        /* use negative index numbers */
	index--;
	/* Set the depth index for p */
	tsi->tsi_SccIdx = index;
	tsi->tsi_SccLowlink = index;

	stack[stackcnt++] = p;                   /* Push p on the stack */
	for (rel=tsi->tsi_relations; rel != NULL; rel=rel->rel_next) {
	    /* Consider successors of p */
	    q = rel->rel_suc;
	    tsi_q = rpmteTSI(q);
	    if (tsi_q->tsi_SccIdx > 0)
		/* Ignore already found SCCs */
		continue;
	    if (tsi_q->tsi_SccIdx == 0){
		/* Was successor q not yet visited? */
		tarjan(q);                       /* Recurse */
		/* negative index numers: use max as it is closer to 0 */
		tsi->tsi_SccLowlink = (
		    tsi->tsi_SccLowlink > tsi_q->tsi_SccLowlink
		    ? tsi->tsi_SccLowlink : tsi_q->tsi_SccLowlink);
	    } else {
		tsi->tsi_SccLowlink = (
		    tsi->tsi_SccLowlink > tsi_q->tsi_SccIdx
		    ? tsi->tsi_SccLowlink : tsi_q->tsi_SccIdx);
	    }
	}

	if (tsi->tsi_SccLowlink == tsi->tsi_SccIdx) {
	    /* v is the root of an SCC? */
	    if (stack[stackcnt-1] == p) {
		/* ignore trivial SCCs */
		q = stack[--stackcnt];
		tsi_q = rpmteTSI(q);
		tsi_q->tsi_SccIdx = 1;
	    } else {
		int stackIdx = stackcnt;
		do {
		    q = stack[--stackIdx];
		    tsi_q = rpmteTSI(q);
		    tsi_q->tsi_SccIdx = sccCnt;
		} while (q != p);

		stackIdx = stackcnt;
		do {
		    q = stack[--stackIdx];
		    tsi_q = rpmteTSI(q);
		    /* Calculate count for the SCC */
		    SCCs[sccCnt].count += tsi_q->tsi_count;
		    /* Subtract internal relations */
		    for (rel=tsi_q->tsi_relations; rel != NULL;
							rel=rel->rel_next) {
			if (rel->rel_suc != q &&
				rpmteTSI(rel->rel_suc)->tsi_SccIdx == sccCnt)
			    SCCs[sccCnt].count--;
		    }
		} while (q != p);
		SCCs[sccCnt].size = stackcnt - stackIdx;
		/* copy members */
		SCCs[sccCnt].members = xcalloc(SCCs[sccCnt].size,
					       sizeof(rpmte));
		memcpy(SCCs[sccCnt].members, stack + stackIdx,
		       SCCs[sccCnt].size * sizeof(rpmte));
		stackcnt = stackIdx;
		sccCnt++;
	    }
	}
    }

    pi = rpmtsiInit(ts);
    while ((p=rpmtsiNext(pi, 0)) != NULL) {
	/* Start a DFS at each node */
	if (rpmteTSI(p)->tsi_SccIdx == 0)
	    tarjan(p);
    }
    pi = rpmtsiFree(pi);

    SCCs = xrealloc(SCCs, (sccCnt+1)*sizeof(struct scc_s));
    /* Debug output */
    if (sccCnt > 2) {
	int msglvl = (rpmtsFlags(ts) & RPMTRANS_FLAG_DEPLOOPS) ?
		     RPMLOG_WARNING : RPMLOG_DEBUG;
	rpmlog(msglvl, "%i Strongly Connected Components\n", sccCnt-2);
	for (int i = 2; i < sccCnt; i++) {
	    rpmlog(msglvl, "SCC #%i: requires %i packages\n",
		   i, SCCs[i].count);
	    /* loop over members */
	    for (int j = 0; j < SCCs[i].size; j++) {
		rpmlog(msglvl, "\t%s\n", rpmteNEVRA(SCCs[i].members[j]));
		/* show relations between members */
		rpmte member = SCCs[i].members[j];
		relation rel = rpmteTSI(member)->tsi_forward_relations;
		for (; rel != NULL; rel=rel->rel_next) {
		    if (rpmteTSI(rel->rel_suc)->tsi_SccIdx!=i) continue;
		    rpmlog(msglvl, "\t\t%s %s\n",
			   rel->rel_flags ? "=>" : "->",
			   rpmteNEVRA(rel->rel_suc));
		}
	    }
	}
    }
    return SCCs;
}

static void collectTE(rpm_color_t prefcolor, rpmte q,
		      rpmte * newOrder, int * newOrderCount,
		      scc SCCs,
		      rpmte * queue_end,
		      rpmte * outer_queue,
		      rpmte * outer_queue_end)
{
    int treex = rpmteTree(q);
    int depth = rpmteDepth(q);
    char deptypechar = (rpmteType(q) == TR_REMOVED ? '-' : '+');
    tsortInfo tsi = rpmteTSI(q);

    rpmlog(RPMLOG_DEBUG, "%5d%5d%5d%5d%5d %*s%c%s\n",
	   *newOrderCount, rpmteNpreds(q),
	   rpmteTSI(q)->tsi_qcnt,
	   treex, depth,
	   (2 * depth), "",
	   deptypechar,
	   (rpmteNEVRA(q) ? rpmteNEVRA(q) : "???"));

    (void) rpmteSetDegree(q, 0);

    newOrder[*newOrderCount] = q;
    (*newOrderCount)++;

    /* T6. Erase relations. */
    for (relation rel = rpmteTSI(q)->tsi_relations;
	 				rel != NULL; rel = rel->rel_next) {
	rpmte p = rel->rel_suc;
	tsortInfo p_tsi = rpmteTSI(p);
	/* ignore already collected packages */
	if (p_tsi->tsi_SccIdx == 0) continue;
	if (p == q) continue;

	if (p && (--p_tsi->tsi_count) == 0) {

	    (void) rpmteSetTree(p, treex);
	    (void) rpmteSetDepth(p, depth+1);
	    (void) rpmteSetParent(p, q);
	    (void) rpmteSetDegree(q, rpmteDegree(q)+1);

	    if (tsi->tsi_SccIdx > 1 && tsi->tsi_SccIdx != p_tsi->tsi_SccIdx) {
                /* Relation point outside of this SCC: add to outside queue */
		assert(outer_queue != NULL && outer_queue_end != NULL);
		addQ(p, outer_queue, outer_queue_end, prefcolor);
	    } else {
		addQ(p, &tsi->tsi_suc, queue_end, prefcolor);
	    }
	}
	if (p && p_tsi->tsi_SccIdx > 1 &&
	                 p_tsi->tsi_SccIdx != rpmteTSI(q)->tsi_SccIdx) {
	    if (--SCCs[p_tsi->tsi_SccIdx].count == 0) {
		/* New SCC is ready, add this package as representative */

		(void) rpmteSetTree(p, treex);
		(void) rpmteSetDepth(p, depth+1);
		(void) rpmteSetParent(p, q);
		(void) rpmteSetDegree(q, rpmteDegree(q)+1);

		if (outer_queue != NULL) {
		    addQ(p, outer_queue, outer_queue_end, prefcolor);
		} else {
		    addQ(p, &rpmteTSI(q)->tsi_suc, queue_end, prefcolor);
		}
	    }
	}
    }
    tsi->tsi_SccIdx = 0;
}

static void collectSCC(rpm_color_t prefcolor, rpmte p,
		       rpmte * newOrder, int * newOrderCount,
		       scc SCCs, rpmte * queue_end)
{
    int sccNr = rpmteTSI(p)->tsi_SccIdx;
    struct scc_s SCC = SCCs[sccNr];
    int i;
    int start, end;
    relation rel;

    /* remove p from the outer queue */
    rpmte outer_queue_start = rpmteTSI(p)->tsi_suc;
    rpmteTSI(p)->tsi_suc = NULL;

    /*
     * Run a multi source Dijkstra's algorithm to find relations
     * that can be zapped with least danger to pre reqs.
     * As weight of the edges is always 1 it is not necessary to
     * sort the vertices by distance as the queue gets them
     * already in order
    */

    /* can use a simple queue as edge weights are always 1 */
    rpmte * queue = xmalloc((SCC.size+1) * sizeof(rpmte));

    /*
     * Find packages that are prerequired and use them as
     * starting points for the Dijkstra algorithm
     */
    start = end = 0;
    for (i = 0; i < SCC.size; i++) {
	rpmte q = SCC.members[i];
	tsortInfo tsi = rpmteTSI(q);
	tsi->tsi_SccLowlink = INT_MAX;
	for (rel=tsi->tsi_forward_relations; rel != NULL; rel=rel->rel_next) {
	    if (rel->rel_flags && rpmteTSI(rel->rel_suc)->tsi_SccIdx == sccNr) {
		if (rel->rel_suc != q) {
		    tsi->tsi_SccLowlink =  0;
		    queue[end++] = q;
		} else {
		    tsi->tsi_SccLowlink =  INT_MAX/2;
		}
		break;
	    }
	}
    }

    if (start == end) { /* no regular prereqs; add self prereqs to queue */
	for (i = 0; i < SCC.size; i++) {
	    rpmte q = SCC.members[i];
	    tsortInfo tsi = rpmteTSI(q);
	    if (tsi->tsi_SccLowlink != INT_MAX) {
		queue[end++] = q;
	    }
	}
    }

    /* Do Dijkstra */
    while (start != end) {
	rpmte q = queue[start++];
	tsortInfo tsi = rpmteTSI(q);
	for (rel=tsi->tsi_forward_relations; rel != NULL; rel=rel->rel_next) {
	    tsortInfo next_tsi = rpmteTSI(rel->rel_suc);
	    if (next_tsi->tsi_SccIdx != sccNr) continue;
	    if (next_tsi->tsi_SccLowlink > tsi->tsi_SccLowlink+1) {
		next_tsi->tsi_SccLowlink = tsi->tsi_SccLowlink + 1;
		queue[end++] = rel->rel_suc;
	    }
	}
    }
    queue = _free(queue);


    while (1) {
	rpmte best = NULL;
	rpmte inner_queue_start, inner_queue_end;
	int best_score = 0;

	/* select best candidate to start with */
	for (int i = 0; i < SCC.size; i++) {
	    rpmte q = SCC.members[i];
	    tsortInfo tsi = rpmteTSI(q);
	    if (tsi->tsi_SccIdx == 0) /* package already collected */
		continue;
	    if (tsi->tsi_SccLowlink >= best_score) {
		best = q;
		best_score = tsi->tsi_SccLowlink;
	    }
	}

	if (best == NULL) /* done */
	    break;

	/* collect best candidate and all packages that get freed */
	inner_queue_start = inner_queue_end = NULL;
	addQ(best, &inner_queue_start, &inner_queue_end, prefcolor);

	for (; inner_queue_start != NULL;
	     inner_queue_start = rpmteTSI(inner_queue_start)->tsi_suc) {
	    /* Mark the package as unqueued. */
	    rpmteTSI(inner_queue_start)->tsi_reqx = 0;
	    collectTE(prefcolor, inner_queue_start, newOrder, newOrderCount,
		      SCCs, &inner_queue_end, &outer_queue_start, queue_end);
	}
    }

    /* restore outer queue */
    rpmteTSI(p)->tsi_suc = outer_queue_start;
}

int rpmtsOrder(rpmts ts)
{
    rpmds requires;
    rpm_color_t prefcolor = rpmtsPrefColor(ts);
    rpmtsi pi; rpmte p;
    rpmte q;
    rpmte r;
    rpmte * newOrder;
    int newOrderCount = 0;
    int treex;
    int rc;
    rpmal erasedPackages = rpmalCreate(5, rpmtsColor(ts));
    scc SCCs;

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_ORDER), 0);

    /*
     * XXX FIXME: this gets needlesly called twice on normal usage patterns,
     * should track the need for generating the index somewhere
     */
    rpmalMakeIndex(ts->addedPackages);

    /* Create erased package index. */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_REMOVED)) != NULL) {
        rpmalAdd(erasedPackages, p);
    }
    pi = rpmtsiFree(pi);
    rpmalMakeIndex(erasedPackages);

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteNewTSI(p);
    pi = rpmtsiFree(pi);

    /* Record relations. */
    rpmlog(RPMLOG_DEBUG, "========== recording tsort relations\n");
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {

	if ((requires = rpmteDS(p, RPMTAG_REQUIRENAME)) == NULL)
	    continue;

	requires = rpmdsInit(requires);
	if (requires != NULL)
	while (rpmdsNext(requires) >= 0) {
	    switch (rpmteType(p)) {
	    case TR_REMOVED:
		/* Record next "q <- p" relation (i.e. "p" requires "q")
		   but reversed. */
		(void) addRelation(ts, erasedPackages, p, requires);
		break;
	    case TR_ADDED:
		/* Record next "q <- p" relation (i.e. "p" requires "q"). */
		(void) addRelation(ts, ts->addedPackages, p, requires);
		break;
	    }
	}
    }
    pi = rpmtsiFree(pi);

    /* Save predecessor count and mark tree roots. */
    treex = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	int npreds;

	npreds = rpmteTSI(p)->tsi_count;

	(void) rpmteSetNpreds(p, npreds);
	(void) rpmteSetDepth(p, 1);

	if (npreds == 0)
	    (void) rpmteSetTree(p, treex++);
	else
	    (void) rpmteSetTree(p, -1);
#ifdef	UNNECESSARY
	(void) rpmteSetParent(p, NULL);
#endif

    }
    pi = rpmtsiFree(pi);
    ts->ntrees = treex;

    newOrder = xcalloc(ts->orderCount, sizeof(*newOrder));
    SCCs = detectSCCs(ts);

    rpmlog(RPMLOG_DEBUG, "========== tsorting packages (order, #predecessors, #succesors, tree, depth)\n");

    for (int i = 0; i < 2; i++) {
	/* Do two separate runs: installs first - then erases */
	int oType = !i ? TR_ADDED : TR_REMOVED;
	q = r = NULL;
	pi = rpmtsiInit(ts);
	/* Scan for zeroes and add them to the queue */
	while ((p = rpmtsiNext(pi, oType)) != NULL) {
	    if (rpmteTSI(p)->tsi_count != 0)
		continue;
	    rpmteTSI(p)->tsi_suc = NULL;
	    addQ(p, &q, &r, prefcolor);
	}
	pi = rpmtsiFree(pi);

	/* Add one member of each leaf SCC */
	for (int i = 2; SCCs[i].members != NULL; i++) {
	    if (SCCs[i].count == 0 && rpmteType(SCCs[i].members[0]) == oType) {
		p = SCCs[i].members[0];
		rpmteTSI(p)->tsi_suc = NULL;
		addQ(p, &q, &r, prefcolor);
	    }
	}

	/* Process queue */
	for (; q != NULL; q = rpmteTSI(q)->tsi_suc) {
	    /* Mark the package as unqueued. */
	    rpmteTSI(q)->tsi_reqx = 0;
	    if (rpmteTSI(q)->tsi_SccIdx > 1) {
		collectSCC(prefcolor, q, newOrder, &newOrderCount, SCCs, &r);
	    } else {
		collectTE(prefcolor, q, newOrder, &newOrderCount, SCCs, &r,
			  NULL, NULL);
	    }
	}
    }

    /* Clean up tsort data */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteFreeTSI(p);
    pi = rpmtsiFree(pi);

    assert(newOrderCount == ts->orderCount);

    ts->order = _free(ts->order);
    ts->order = newOrder;
    ts->orderAlloced = ts->orderCount;
    rc = 0;

    freeBadDeps();

    for (int i = 2; SCCs[i].members != NULL; i++) {
	free(SCCs[i].members);
    }
    free(SCCs);
    rpmalFree(erasedPackages);

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_ORDER), 0);

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
	rc = checkPackageDeps(ts, rpmteNEVRA(p),
			rpmteDS(p, RPMTAG_REQUIRENAME),
			rpmteDS(p, RPMTAG_CONFLICTNAME),
			NULL,
			tscolor, 1);
	if (rc)
	    goto exit;

	rc = 0;
	provides = rpmteDS(p, RPMTAG_PROVIDENAME);
	provides = rpmdsInit(provides);
	if (provides != NULL)
	while (rpmdsNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = rpmdsN(provides)) == NULL)
		continue;	/* XXX can't happen */

	    /* Adding: check provides key against conflicts matches. */
	    if (!checkDependentConflicts(ts, Name))
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
	rpmds provides;
	rpmfi fi;

	/* FIX: rpmts{A,O} can return null. */
	rpmlog(RPMLOG_DEBUG, "========== --- %s %s/%s 0x%x\n",
		rpmteNEVR(p), rpmteA(p), rpmteO(p), rpmteColor(p));

	rc = 0;
	provides = rpmteDS(p, RPMTAG_PROVIDENAME);
	provides = rpmdsInit(provides);
	if (provides != NULL)
	while (rpmdsNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = rpmdsN(provides)) == NULL)
		continue;	/* XXX can't happen */

	    /* Erasing: check provides against requiredby matches. */
	    if (!checkDependentPackages(ts, Name))
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
	    if (!checkDependentPackages(ts, fn))
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

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    if (closeatexit)
	xx = rpmtsCloseDB(ts);
    else if (_cacheDependsRC)
	xx = rpmdbCloseDBI(rpmtsGetRdb(ts), RPMDBI_DEPENDS);
    return rc;
}
