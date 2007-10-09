/** \ingroup rpmts
 * \file lib/depends.c
 */

#include "system.h"

#include <rpmcli.h>		/* XXX rpmcliPackagesTotal */

#include <rpmmacro.h>		/* XXX rpmExpand("%{_dependency_whiteout}" */

#include "rpmdb.h"
#include "rpmdb_internal.h"	/* XXX response cache needs dbiOpen et al. */

#include "rpmds.h"
#include "rpmfi.h"

#define	_RPMTE_INTERNAL
#include "rpmte.h"

#define	_RPMTS_INTERNAL
#include "rpmts.h"

#include "debug.h"


/**
 */
typedef struct orderListIndex_s *	orderListIndex;

/**
 */
struct orderListIndex_s {
    rpmalKey pkgKey;
    int orIndex;
};

int _cacheDependsRC = 1;

const char *rpmNAME = PACKAGE;

const char *rpmEVR = VERSION;

int rpmFLAGS = RPMSENSE_EQUAL;

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
 * @param dboffset	rpm database instance
 * @param depends	installed package of pair (or RPMAL_NOMATCH on erase)
 * @return		0 on success
 */
static int removePackage(rpmts ts, Header h, int dboffset,
		rpmalKey depends)
{
    rpmte p;

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

    p = rpmteNew(ts, h, TR_REMOVED, NULL, NULL, dboffset, depends);
    ts->order[ts->orderCount] = p;
    ts->orderCount++;

    return 0;
}

int rpmtsAddInstallElement(rpmts ts, Header h,
			fnpyKey key, int upgrade, rpmRelocation * relocs)
{
    uint_32 tscolor = rpmtsColor(ts);
    uint_32 dscolor;
    uint_32 hcolor;
    rpmdbMatchIterator mi;
    Header oh;
    uint_32 ohcolor;
    int isSource;
    int duplicate = 0;
    rpmtsi pi = NULL; rpmte p;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char * arch;
    const char * os;
    rpmds oldChk, newChk;
    rpmds obsoletes;
    rpmalKey pkgKey;	/* addedPackages key */
    int xx;
    int ec = 0;
    int rc;
    int oc;

    /*
     * Check for previously added versions with the same name and arch/os.
     * FIXME: only catches previously added, older packages.
     */
    arch = NULL;
    xx = hge(h, RPMTAG_ARCH, NULL, (void **)&arch, NULL);
    os = NULL;
    xx = hge(h, RPMTAG_OS, NULL, (void **)&os, NULL);
    hcolor = hGetColor(h);
    pkgKey = RPMAL_NOMATCH;

    /* Check for supported payload format if it's a package */
    if (key && headerCheckPayloadFormat(h) != RPMRC_OK) {
	ec = 1;
	goto exit;
    }

    /* XXX Always add source headers. */
    isSource = headerIsEntry(h, RPMTAG_SOURCEPACKAGE);
    if (isSource) {
	oc = ts->orderCount;
	goto addheader;
    }

    oldChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_LESS));
    newChk = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL|RPMSENSE_GREATER));
    /* XXX can't use rpmtsiNext() filter or oc will have wrong value. */
    for (pi = rpmtsiInit(ts), oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
	rpmds this;

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
 	 * On upgrade, if newer NEVR was previously added, 
 	 * then skip adding older. 
 	 */
	rc = rpmdsCompare(newChk, this);
	if (upgrade && rc != 0) {
	    const char * pkgNEVR = rpmdsDNEVR(this);
	    const char * addNEVR = rpmdsDNEVR(oldChk);
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
	    const char * pkgNEVR = rpmdsDNEVR(this);
	    const char * addNEVR = rpmdsDNEVR(newChk);
	    if (rpmIsVerbose())
		rpmlog(RPMLOG_WARNING,
		    _("package %s was already added, replacing with %s\n"),
		    (pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		    (addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    duplicate = 1;
	    pkgKey = rpmteAddedKey(p);
	    break;
	}
    }
    pi = rpmtsiFree(pi);
    oldChk = rpmdsFree(oldChk);
    newChk = rpmdsFree(newChk);

    /* If newer NEVR was already added, exit now. */
    if (ec)
	goto exit;

addheader:
    if (oc >= ts->orderAlloced) {
	ts->orderAlloced += (oc - ts->orderAlloced) + ts->delta;
	ts->order = xrealloc(ts->order, ts->orderAlloced * sizeof(*ts->order));
    }

    p = rpmteNew(ts, h, TR_ADDED, key, relocs, -1, pkgKey);

    if (duplicate && oc < ts->orderCount) {
	ts->order[oc] = rpmteFree(ts->order[oc]);
    }

    ts->order[oc] = p;
    if (!duplicate) {
	ts->orderCount++;
	rpmcliPackagesTotal++;
    }
    
    pkgKey = rpmalAdd(&ts->addedPackages, pkgKey, rpmteKey(p),
			rpmteDS(p, RPMTAG_PROVIDENAME),
			rpmteFI(p, RPMTAG_BASENAMES), tscolor);
    if (pkgKey == RPMAL_NOMATCH) {
	ts->order[oc] = rpmteFree(ts->order[oc]);
	ec = 1;
	goto exit;
    }
    (void) rpmteSetAddedKey(p, pkgKey);

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
    if (rpmtsGetRdb(ts) == NULL && ts->dbmode != -1) {
	if ((ec = rpmtsOpenDB(ts, ts->dbmode)) != 0)
	    goto exit;
    }

    /* On upgrade, erase older packages of same color (if any). */

    mi = rpmtsInitIterator(ts, RPMTAG_NAME, rpmteN(p), 0);
    while((oh = rpmdbNextIterator(mi)) != NULL) {

	/* Ignore colored packages not in our rainbow. */
	ohcolor = hGetColor(oh);
	if (tscolor && hcolor && ohcolor && !(hcolor & ohcolor))
	    continue;

	/* Skip packages that contain identical NEVR. */
	if (rpmVersionCompare(h, oh) == 0)
	    continue;

	xx = removePackage(ts, oh, rpmdbGetIteratorOffset(mi), pkgKey);
    }
    mi = rpmdbFreeIterator(mi);

    obsoletes = rpmdsLink(rpmteDS(p, RPMTAG_OBSOLETENAME), "Obsoletes");
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
	    ohcolor = hGetColor(oh);
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
		const char * ohNEVRA = hGetNEVRA(oh, NULL);
#ifdef	DYING	/* XXX see http://bugzilla.redhat.com #134497 */
		if (rpmVersionCompare(h, oh))
#endif
		    xx = removePackage(ts, oh, rpmdbGetIteratorOffset(mi), pkgKey);
		rpmlog(RPMLOG_DEBUG, _("  Obsoletes: %s\t\terases %s\n"),
			rpmdsDNEVR(obsoletes)+2, ohNEVRA);
		ohNEVRA = _free(ohNEVRA);
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }
    obsoletes = rpmdsFree(obsoletes);

    ec = 0;

exit:
    pi = rpmtsiFree(pi);
    return ec;
}

int rpmtsAddEraseElement(rpmts ts, Header h, int dboffset)
{
    return removePackage(ts, h, dboffset, RPMAL_NOMATCH);
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
    DBT * key = alloca(sizeof(*key));
    DBT * data = alloca(sizeof(*data));
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

		memset(key, 0, sizeof(*key));
		key->data = (void *) DNEVR;
		key->size = DNEVRlen;
		memset(data, 0, sizeof(*data));
		data->data = datap;
		data->size = datalen;
/* FIX: data->data may be NULL */
		xx = dbiGet(dbi, dbcursor, key, data, DB_SET);
		DNEVR = key->data;
		DNEVRlen = key->size;
		datap = data->data;
		datalen = data->size;

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
	if (rpmCheckRpmlibProvides(dep)) {
	    rpmdsNotify(dep, _("(rpmlib provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    /* Search added packages for the dependency. */
    if (rpmalSatisfiesDepend(ts->addedPackages, dep, NULL) != NULL) {
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

		memset(key, 0, sizeof(*key));
		key->data = (void *) DNEVR;
		key->size = DNEVRlen;
		memset(data, 0, sizeof(*data));
		data->data = &rc;
		data->size = sizeof(rc);

		xx = dbiPut(dbi, dbcursor, key, data, 0);
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
		const char * depName, uint_32 tscolor, int adding)
{
    uint_32 dscolor;
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
	{   fnpyKey * suggestedKeys = NULL;

	    if (ts->availablePackages != NULL) {
		suggestedKeys = rpmalAllSatisfiesDepend(ts->availablePackages,
				requires, NULL);
	    }

	    rpmdsProblem(ts->probs, pkgNEVRA, requires, suggestedKeys, adding);

	}
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
    int scareMem = 1;
    Header h;
    int ec = 0;

    (void) rpmdbPruneIterator(mi,
		ts->removedPackages, ts->numRemovedPackages, 1);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	const char * pkgNEVRA;
	rpmds requires, conflicts;
	int rc;

	pkgNEVRA = hGetNEVRA(h, NULL);
	requires = rpmdsNew(h, RPMTAG_REQUIRENAME, scareMem);
	(void) rpmdsSetNoPromote(requires, _rpmds_nopromote);
	conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, scareMem);
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
    const char * pname;
    const char * qname;
};

#ifdef REFERENCE
static struct badDeps_s {
const char * pname;
const char * qname;
} badDeps[] = {
    { NULL, NULL }
};
#else
static int badDepsInitialized = 0;

static struct badDeps_s * badDeps = NULL;
#endif

/**
 */
static void freeBadDeps(void)
{
    if (badDeps) {
	struct badDeps_s * bdp;
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

/**
 * Recursively mark all nodes with their predecessors.
 * @param tsi		successor chain
 * @param q		predecessor
 */
static void markLoop(tsortInfo tsi, rpmte q)
{
    rpmte p;

    /* FIX: q is kept */
    while (tsi != NULL && (p = tsi->tsi_suc) != NULL) {
	tsi = tsi->tsi_next;
	if (rpmteTSI(p)->tsi_chain != NULL)
	    continue;
	rpmteTSI(p)->tsi_chain = q;
	if (rpmteTSI(p)->tsi_next != NULL)
	    markLoop(rpmteTSI(p)->tsi_next, p);
    }
}

static inline const char * identifyDepend(int_32 f)
{
    if (isLegacyPreReq(f))
	return "PreReq:";
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
 * Find (and eliminate co-requisites) "q <- p" relation in dependency loop.
 * Search all successors of q for instance of p. Format the specific relation,
 * (e.g. p contains "Requires: q"). Unlink and free co-requisite (i.e.
 * pure Requires: dependencies) successor node(s).
 * @param q		sucessor (i.e. package required by p)
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param requires	relation
 * @param zap		max. no. of co-requisites to remove (-1 is all)?
 * @retval nzaps	address of no. of relations removed
 * @param msglvl	message level at which to spew
 * @return		(possibly NULL) formatted "q <- p" releation (malloc'ed)
 */
/* FIX: hack modifies, but -type disables */
static const char *
zapRelation(rpmte q, rpmte p,
		rpmds requires,
		int zap, int * nzaps, int msglvl)
{
    tsortInfo tsi_prev;
    tsortInfo tsi;
    const char *dp = NULL;

    for (tsi_prev = rpmteTSI(q), tsi = rpmteTSI(q)->tsi_next;
	 tsi != NULL;
	/* XXX Note: the loop traverses "not found", break on "found". */
	 tsi_prev = tsi, tsi = tsi->tsi_next)
    {
	int_32 Flags;

	if (tsi->tsi_suc != p)
	    continue;

	if (requires == NULL) continue;		/* XXX can't happen */

	(void) rpmdsSetIx(requires, tsi->tsi_reqx);

	Flags = rpmdsFlags(requires);

	dp = rpmdsNewDNEVR( identifyDepend(Flags), requires);

	/*
	 * Attempt to unravel a dependency loop by eliminating Requires's.
	 */
	if (zap && !(Flags & RPMSENSE_PREREQ)) {
	    rpmlog(msglvl,
			_("removing %s \"%s\" from tsort relations.\n"),
			(rpmteNEVRA(p) ?  rpmteNEVRA(p) : "???"), dp);
	    rpmteTSI(p)->tsi_count--;
	    if (tsi_prev) tsi_prev->tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    tsi->tsi_suc = NULL;
	    tsi = _free(tsi);
	    if (nzaps)
		(*nzaps)++;
	    if (zap)
		zap--;
	}
	/* XXX Note: the loop traverses "not found", get out now! */
	break;
    }
    return dp;
}

/**
 * Record next "q <- p" relation (i.e. "p" requires "q").
 * @param ts		transaction set
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param selected	boolean package selected array
 * @param requires	relation
 * @return		0 always
 */
static inline int addRelation(rpmts ts,
		rpmte p,
		unsigned char * selected,
		rpmds requires)
{
    rpmtsi qi; rpmte q;
    tsortInfo tsi;
    const char * Name;
    fnpyKey key;
    rpmalKey pkgKey;
    int i = 0;

    if ((Name = rpmdsN(requires)) == NULL)
	return 0;

    /* Avoid rpmlib feature dependencies. */
    if (!strncmp(Name, "rpmlib(", sizeof("rpmlib(")-1))
	return 0;

    /* Avoid package config dependencies. */
    if (!strncmp(Name, "config(", sizeof("config(")-1))
	return 0;

    pkgKey = RPMAL_NOMATCH;
    key = rpmalSatisfiesDepend(ts->addedPackages, requires, &pkgKey);

    /* Ordering depends only on added package relations. */
    if (pkgKey == RPMAL_NOMATCH)
	return 0;

/* XXX Set q to the added package that has pkgKey == q->u.addedKey */
/* XXX FIXME: bsearch is possible/needed here */
    for (qi = rpmtsiInit(ts), i = 0; (q = rpmtsiNext(qi, 0)) != NULL; i++) {

	/* XXX Only added packages need be checked for matches. */
	if (rpmteType(q) == TR_REMOVED)
	    continue;

	if (pkgKey == rpmteAddedKey(q))
	    break;
    }
    qi = rpmtsiFree(qi);
    if (q == NULL || i == ts->orderCount)
	return 0;

    /* Avoid certain dependency relations. */
    if (ignoreDep(ts, p, q))
	return 0;

    /* Avoid redundant relations. */
    /* XXX TODO: add control bit. */
    if (selected[i] != 0)
	return 0;
    selected[i] = 1;

    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
    rpmteTSI(p)->tsi_count++;			/* bump p predecessor count */

    if (rpmteDepth(p) <= rpmteDepth(q))	/* Save max. depth in dependency tree */
	(void) rpmteSetDepth(p, (rpmteDepth(q) + 1));
    if (rpmteDepth(p) > ts->maxDepth)
	ts->maxDepth = rpmteDepth(p);

    tsi = xcalloc(1, sizeof(*tsi));
    tsi->tsi_suc = p;

    tsi->tsi_reqx = rpmdsIx(requires);

    tsi->tsi_next = rpmteTSI(q)->tsi_next;
    rpmteTSI(q)->tsi_next = tsi;
    rpmteTSI(q)->tsi_qcnt++;			/* bump q successor count */
    return 0;
}

/**
 * Compare ordered list entries by index (qsort/bsearch).
 * @param one		1st ordered list entry
 * @param two		2nd ordered list entry
 * @return		result of comparison
 */
static int orderListIndexCmp(const void * one, const void * two)	
{
    long a = (long) ((const orderListIndex)one)->pkgKey;
    long b = (long) ((const orderListIndex)two)->pkgKey;
    return (a - b);
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
		uint_32 prefcolor)
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

int rpmtsOrder(rpmts ts)
{
    rpmds requires;
    int_32 Flags;
    uint_32 prefcolor = rpmtsPrefColor(ts);
    rpmtsi pi; rpmte p;
    rpmtsi qi; rpmte q;
    rpmtsi ri; rpmte r;
    tsortInfo tsi;
    tsortInfo tsi_next;
    rpmalKey * ordering;
    int orderingCount = 0;
    unsigned char * selected = alloca(sizeof(*selected) * (ts->orderCount + 1));
    int loopcheck;
    rpmte * newOrder;
    int newOrderCount = 0;
    orderListIndex orderList;
    int numOrderList;
    int npeer = 128;	/* XXX more than deep enough for now. */
    int * peer = memset(alloca(npeer*sizeof(*peer)), 0, (npeer*sizeof(*peer)));
    int nrescans = 10;
    int _printed = 0;
    char deptypechar;
    size_t tsbytes;
    int oType = 0;
    int treex;
    int depth;
    int breadth;
    int qlen;
    int i, j;

    /*
     * XXX FIXME: this gets needlesly called twice on normal usage patterns,
     * should track the need for generating the index somewhere
     */
    rpmalMakeIndex(ts->addedPackages);

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_ORDER), 0);

    /* T1. Initialize. */
    if (oType == 0)
	numOrderList = ts->orderCount;
    else {
	numOrderList = 0;
	if (oType & TR_ADDED)
	    numOrderList += ts->numAddedPackages;
	if (oType & TR_REMOVED)
	    numOrderList += ts->numRemovedPackages;
     }
    ordering = alloca(sizeof(*ordering) * (numOrderList + 1));
    loopcheck = numOrderList;
    tsbytes = 0;

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL)
	rpmteNewTSI(p);
    pi = rpmtsiFree(pi);

    /* Record all relations. */
    rpmlog(RPMLOG_DEBUG, _("========== recording tsort relations\n"));
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {

	if ((requires = rpmteDS(p, RPMTAG_REQUIRENAME)) == NULL)
	    continue;

	memset(selected, 0, sizeof(*selected) * ts->orderCount);

	/* Avoid narcisstic relations. */
	selected[rpmtsiOc(pi)] = 1;

	/* T2. Next "q <- p" relation. */

	/* First, do pre-requisites. */
	requires = rpmdsInit(requires);
	if (requires != NULL)
	while (rpmdsNext(requires) >= 0) {

	    Flags = rpmdsFlags(requires);

	    switch (rpmteType(p)) {
	    case TR_REMOVED:
		/* Skip if not %preun/%postun requires or legacy prereq. */
		if (!( isErasePreReq(Flags) || isLegacyPreReq(Flags) ) )
		    continue;
		break;
	    case TR_ADDED:
		/* Skip if not %pre/%post requires or legacy prereq. */
		if (!( isInstallPreReq(Flags) || isLegacyPreReq(Flags) ) )
		    continue;
		break;
	    }

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, requires);

	}

	/* Then do co-requisites. */
	requires = rpmdsInit(requires);
	if (requires != NULL)
	while (rpmdsNext(requires) >= 0) {

	    Flags = rpmdsFlags(requires);

	    switch (rpmteType(p)) {
	    case TR_REMOVED:
		/* Skip if %preun/%postun requires or legacy prereq. */
		if (isInstallPreReq(Flags)
		 ||  ( isErasePreReq(Flags) || isLegacyPreReq(Flags) ) )
		    continue;
		break;
	    case TR_ADDED:
		/* Skip if %pre/%post requires or legacy prereq. */
		if (isErasePreReq(Flags)
		 ||  ( isInstallPreReq(Flags) || isLegacyPreReq(Flags) ) )
		    continue;
		break;
	    }

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, requires);

	}
    }
    pi = rpmtsiFree(pi);

    /* Save predecessor count and mark tree roots. */
    treex = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {
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

    /* T4. Scan for zeroes. */
    rpmlog(RPMLOG_DEBUG, _("========== tsorting packages (order, #predecessors, #succesors, tree, depth, breadth)\n"));

rescan:
    if (pi != NULL) pi = rpmtsiFree(pi);
    q = r = NULL;
    qlen = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {
	if (rpmteTSI(p)->tsi_count != 0)
	    continue;
	rpmteTSI(p)->tsi_suc = NULL;
	addQ(p, &q, &r, prefcolor);
	qlen++;
    }
    pi = rpmtsiFree(pi);

    /* T5. Output front of queue (T7. Remove from queue.) */
    for (; q != NULL; q = rpmteTSI(q)->tsi_suc) {

	/* Mark the package as unqueued. */
	rpmteTSI(q)->tsi_reqx = 0;

	if (oType != 0)
	switch (rpmteType(q)) {
	case TR_ADDED:
	    if (!(oType & TR_ADDED))
		continue;
	    break;
	case TR_REMOVED:
	    if (!(oType & TR_REMOVED))
		continue;
	    break;
	default:
	    continue;
	    break;
	}
	deptypechar = (rpmteType(q) == TR_REMOVED ? '-' : '+');

	treex = rpmteTree(q);
	depth = rpmteDepth(q);
	breadth = ((depth < npeer) ? peer[depth]++ : 0);
	(void) rpmteSetBreadth(q, breadth);

	rpmlog(RPMLOG_DEBUG, "%5d%5d%5d%5d%5d%5d %*s%c%s\n",
			orderingCount, rpmteNpreds(q),
			rpmteTSI(q)->tsi_qcnt,
			treex, depth, breadth,
			(2 * depth), "",
			deptypechar,
			(rpmteNEVRA(q) ? rpmteNEVRA(q) : "???"));

	(void) rpmteSetDegree(q, 0);
	tsbytes += rpmtePkgFileSize(q);

	switch (rpmteType(q)) {
	case TR_ADDED:
            ordering[orderingCount] = rpmteAddedKey(q);
            break;
	case TR_REMOVED:
            ordering[orderingCount] = RPMAL_NOMATCH;
            break;
        }
	orderingCount++;
	qlen--;
	loopcheck--;

	/* T6. Erase relations. */
	tsi_next = rpmteTSI(q)->tsi_next;
	rpmteTSI(q)->tsi_next = NULL;
	while ((tsi = tsi_next) != NULL) {
	    tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    p = tsi->tsi_suc;
	    if (p && (--rpmteTSI(p)->tsi_count) <= 0) {

		(void) rpmteSetTree(p, treex);
		(void) rpmteSetDepth(p, depth+1);
		(void) rpmteSetParent(p, q);
		(void) rpmteSetDegree(q, rpmteDegree(q)+1);

		/* XXX TODO: add control bit. */
		rpmteTSI(p)->tsi_suc = NULL;
		addQ(p, &rpmteTSI(q)->tsi_suc, &r, prefcolor);
		qlen++;
	    }
	    tsi = _free(tsi);
	}
	if (!_printed && loopcheck == qlen && rpmteTSI(q)->tsi_suc != NULL) {
	    _printed++;
	    (void) rpmtsUnorderedSuccessors(ts, orderingCount);
	    rpmlog(RPMLOG_DEBUG,
		_("========== successors only (%d bytes)\n"), (int)tsbytes);

	    /* Relink the queue in presentation order. */
	    tsi = rpmteTSI(q);
	    pi = rpmtsiInit(ts);
	    while ((p = rpmtsiNext(pi, oType)) != NULL) {
		/* Is this element in the queue? */
		if (rpmteTSI(p)->tsi_reqx == 0)
		    continue;
		tsi->tsi_suc = p;
		tsi = rpmteTSI(p);
	    }
	    pi = rpmtsiFree(pi);
	    tsi->tsi_suc = NULL;
	}
    }

    /* T8. End of process. Check for loops. */
    if (loopcheck != 0) {
	int nzaps;

	/* T9. Initialize predecessor chain. */
	nzaps = 0;
	qi = rpmtsiInit(ts);
	while ((q = rpmtsiNext(qi, oType)) != NULL) {
	    rpmteTSI(q)->tsi_chain = NULL;
	    rpmteTSI(q)->tsi_reqx = 0;
	    /* Mark packages already sorted. */
	    if (rpmteTSI(q)->tsi_count == 0)
		rpmteTSI(q)->tsi_count = -1;
	}
	qi = rpmtsiFree(qi);

	/* T10. Mark all packages with their predecessors. */
	qi = rpmtsiInit(ts);
	while ((q = rpmtsiNext(qi, oType)) != NULL) {
	    if ((tsi = rpmteTSI(q)->tsi_next) == NULL)
		continue;
	    rpmteTSI(q)->tsi_next = NULL;
	    markLoop(tsi, q);
	    rpmteTSI(q)->tsi_next = tsi;
	}
	qi = rpmtsiFree(qi);

	/* T11. Print all dependency loops. */
	ri = rpmtsiInit(ts);
	while ((r = rpmtsiNext(ri, oType)) != NULL)
	{
	    int printed;

	    printed = 0;

	    /* T12. Mark predecessor chain, looking for start of loop. */
	    for (q = rpmteTSI(r)->tsi_chain; q != NULL;
		 q = rpmteTSI(q)->tsi_chain)
	    {
		if (rpmteTSI(q)->tsi_reqx)
		    break;
		rpmteTSI(q)->tsi_reqx = 1;
	    }

	    /* T13. Print predecessor chain from start of loop. */
	    while ((p = q) != NULL && (q = rpmteTSI(p)->tsi_chain) != NULL) {
		const char * dp;
		char buf[4096];
		int msglvl = (rpmtsFlags(ts) & RPMTRANS_FLAG_DEPLOOPS)
			? RPMLOG_WARNING : RPMLOG_DEBUG;
;

		/* Unchain predecessor loop. */
		rpmteTSI(p)->tsi_chain = NULL;

		if (!printed) {
		    rpmlog(msglvl, _("LOOP:\n"));
		    printed = 1;
		}

		/* Find (and destroy if co-requisite) "q <- p" relation. */
		requires = rpmteDS(p, RPMTAG_REQUIRENAME);
		requires = rpmdsInit(requires);
		if (requires == NULL)
		    continue;	/* XXX can't happen */
		dp = zapRelation(q, p, requires, 1, &nzaps, msglvl);

		/* Print next member of loop. */
		buf[0] = '\0';
		if (rpmteNEVRA(p) != NULL)
		    (void) stpcpy(buf, rpmteNEVRA(p));
		rpmlog(msglvl, "    %-40s %s\n", buf,
			(dp ? dp : "not found!?!"));

		dp = _free(dp);
	    }

	    /* Walk (and erase) linear part of predecessor chain as well. */
	    for (p = r, q = rpmteTSI(r)->tsi_chain; q != NULL;
		 p = q, q = rpmteTSI(q)->tsi_chain)
	    {
		/* Unchain linear part of predecessor loop. */
		rpmteTSI(p)->tsi_chain = NULL;
		rpmteTSI(p)->tsi_reqx = 0;
	    }
	}
	ri = rpmtsiFree(ri);

	/* If a relation was eliminated, then continue sorting. */
	/* XXX TODO: add control bit. */
	if (nzaps && nrescans-- > 0) {
	    rpmlog(RPMLOG_DEBUG, _("========== continuing tsort ...\n"));
	    goto rescan;
	}

	/* Return no. of packages that could not be ordered. */
	rpmlog(RPMLOG_ERR, _("rpmtsOrder failed, %d elements remain\n"),
			loopcheck);
	return loopcheck;
    }

    /* Clean up tsort remnants (if any). */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteFreeTSI(p);
    pi = rpmtsiFree(pi);

    /*
     * The order ends up as installed packages followed by removed packages,
     * with removes for upgrades immediately following the installation of
     * the new package. This would be easier if we could sort the
     * addedPackages array, but we store indexes into it in various places.
     */
    orderList = xcalloc(numOrderList, sizeof(*orderList));
    j = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {
	/* Prepare added package ordering permutation. */
	switch (rpmteType(p)) {
	case TR_ADDED:
	    orderList[j].pkgKey = rpmteAddedKey(p);
	    break;
	case TR_REMOVED:
	    orderList[j].pkgKey = RPMAL_NOMATCH;
	    break;
	}
	orderList[j].orIndex = rpmtsiOc(pi);
	j++;
    }
    pi = rpmtsiFree(pi);

    qsort(orderList, numOrderList, sizeof(*orderList), orderListIndexCmp);

    newOrder = xcalloc(ts->orderCount, sizeof(*newOrder));
    for (i = 0, newOrderCount = 0; i < orderingCount; i++)
    {
	struct orderListIndex_s key;
	orderListIndex needle;

	key.pkgKey = ordering[i];
	needle = bsearch(&key, orderList, numOrderList,
				sizeof(key), orderListIndexCmp);
	/* bsearch should never, ever fail */
	if (needle == NULL)
	    continue;

	j = needle->orIndex;
	if ((q = ts->order[j]) == NULL || needle->pkgKey == RPMAL_NOMATCH)
	    continue;

	newOrder[newOrderCount++] = q;
	ts->order[j] = NULL;
    }

    for (j = 0; j < ts->orderCount; j++) {
	if ((p = ts->order[j]) == NULL)
	    continue;
	newOrder[newOrderCount++] = p;
	ts->order[j] = NULL;
    }
assert(newOrderCount == ts->orderCount);

    ts->order = _free(ts->order);
    ts->order = newOrder;
    ts->orderAlloced = ts->orderCount;
    orderList = _free(orderList);

#ifdef	DYING	/* XXX now done at the CLI level just before rpmtsRun(). */
    rpmtsClean(ts);
#endif
    freeBadDeps();

    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_ORDER), 0);

    return 0;
}

int rpmtsCheck(rpmts ts)
{
    uint_32 tscolor = rpmtsColor(ts);
    rpmdbMatchIterator mi = NULL;
    rpmtsi pi = NULL; rpmte p;
    int closeatexit = 0;
    int xx;
    int rc;

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_CHECK), 0);

    /* Do lazy, readonly, open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL && ts->dbmode != -1) {
	if ((rc = rpmtsOpenDB(ts, ts->dbmode)) != 0)
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
	fi = rpmteFI(p, RPMTAG_BASENAMES);
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
