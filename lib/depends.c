/*@-boundsread@*/
/** \ingroup rpmts
 * \file lib/depends.c
 */

#include "system.h"

#include <rpmlib.h>

#include <rpmmacro.h>		/* XXX rpmExpand("%{_dependency_whiteout}" */

#include "rpmdb.h"		/* XXX response cache needs dbiOpen et al. */
#include "rpmps.h"

#include "rpmal.h"
#include "rpmds.h"
#include "rpmfi.h"

#define	_RPMTE_INTERNAL
#include "rpmte.h"

#define	_RPMTS_INTERNAL
#include "rpmts.h"

#include "debug.h"

/*@access tsortInfo @*/
/*@access rpmts @*/

/*@access dbiIndex @*/		/* XXX for dbi->dbi_txnid */

/*@access alKey @*/	/* XXX for reordering and RPMAL_NOMATCH assign */

/**
 */
typedef /*@abstract@*/ struct orderListIndex_s *	orderListIndex;
/*@access orderListIndex@*/

/**
 */
struct orderListIndex_s {
/*@dependent@*/
    alKey pkgKey;
    int orIndex;
};

/*@unchecked@*/
int _cacheDependsRC = 1;

/*@unchecked@*/
static int _tso_debug = 0;

/*@observer@*/ /*@unchecked@*/
const char *rpmNAME = PACKAGE;

/*@observer@*/ /*@unchecked@*/
const char *rpmEVR = VERSION;

/*@unchecked@*/
int rpmFLAGS = RPMSENSE_EQUAL;

/**
 * Compare removed package instances (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
static int intcmp(const void * a, const void * b)	/*@*/
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
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ alKey depends)
	/*@modifies ts, h @*/
{
    rpmte p;

    /* Filter out duplicate erasures. */
    if (ts->numRemovedPackages > 0 && ts->removedPackages != NULL) {
/*@-boundswrite@*/
	if (bsearch(&dboffset, ts->removedPackages, ts->numRemovedPackages,
			sizeof(*ts->removedPackages), intcmp) != NULL)
	    return 0;
/*@=boundswrite@*/
    }

    if (ts->numRemovedPackages == ts->allocedRemovedPackages) {
	ts->allocedRemovedPackages += ts->delta;
	ts->removedPackages = xrealloc(ts->removedPackages,
		sizeof(ts->removedPackages) * ts->allocedRemovedPackages);
    }

    if (ts->removedPackages != NULL) {	/* XXX can't happen. */
/*@-boundswrite@*/
	ts->removedPackages[ts->numRemovedPackages] = dboffset;
	ts->numRemovedPackages++;
/*@=boundswrite@*/
	if (ts->numRemovedPackages > 1)
	    qsort(ts->removedPackages, ts->numRemovedPackages,
			sizeof(*ts->removedPackages), intcmp);
    }

    if (ts->orderCount >= ts->orderAlloced) {
	ts->orderAlloced += (ts->orderCount - ts->orderAlloced) + ts->delta;
/*@-type +voidabstract @*/
	ts->order = xrealloc(ts->order, sizeof(*ts->order) * ts->orderAlloced);
/*@=type =voidabstract @*/
    }

    p = rpmteNew(ts, h, TR_REMOVED, NULL, NULL, dboffset, depends);
/*@-boundswrite@*/
    ts->order[ts->orderCount] = p;
    ts->orderCount++;
/*@=boundswrite@*/

    return 0;
}

int rpmtsAddInstallElement(rpmts ts, Header h,
			fnpyKey key, int upgrade, rpmRelocation * relocs)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    int isSource;
    int duplicate = 0;
    rpmtsi pi; rpmte p;
    rpmds add;
    rpmds obsoletes;
    alKey pkgKey;	/* addedPackages key */
    int xx;
    int ec = 0;
    int rc;
    int oc;

    /*
     * Check for previously added versions with the same name.
     * FIXME: only catches previously added, older packages.
     */
    add = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL|RPMSENSE_LESS));
    pkgKey = RPMAL_NOMATCH;
    for (pi = rpmtsiInit(ts), oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
	rpmds this;

	/* XXX Only added packages need be checked for dupes. */
	if (rpmteType(p) == TR_REMOVED)
	    continue;

	if ((this = rpmteDS(p, RPMTAG_NAME)) == NULL)
	    continue;	/* XXX can't happen */

	rc = rpmdsCompare(add, this);
	if (rc != 0) {
	    const char * pkgNEVR = rpmdsDNEVR(this);
	    const char * addNEVR = rpmdsDNEVR(add);
	    rpmMessage(RPMMESS_WARNING,
		_("package %s was already added, replacing with %s\n"),
		(pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		(addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    duplicate = 1;
	    pkgKey = rpmteAddedKey(p);
	    break;
	}
    }
    pi = rpmtsiFree(pi);
    add = rpmdsFree(add);

    isSource = headerIsEntry(h, RPMTAG_SOURCEPACKAGE);

    if (p != NULL && duplicate && oc < ts->orderCount) {
    /* XXX FIXME removed transaction element side effects need to be weeded */
/*@-type -unqualifiedtrans@*/
/*@-boundswrite@*/
	ts->order[oc] = rpmteFree(ts->order[oc]);
/*@=boundswrite@*/
/*@=type =unqualifiedtrans@*/
    }

    if (oc >= ts->orderAlloced) {
	ts->orderAlloced += (oc - ts->orderAlloced) + ts->delta;
/*@-type +voidabstract @*/
	ts->order = xrealloc(ts->order, ts->orderAlloced * sizeof(*ts->order));
/*@=type =voidabstract @*/
    }

    p = rpmteNew(ts, h, TR_ADDED, key, relocs, -1, pkgKey);
/*@-boundswrite@*/
    ts->order[oc] = p;
/*@=boundswrite@*/
    if (!duplicate)
	ts->orderCount++;
    
    pkgKey = rpmalAdd(&ts->addedPackages, pkgKey, rpmteKey(p),
			rpmteDS(p, RPMTAG_PROVIDENAME),
			rpmteFI(p, RPMTAG_BASENAMES));
    if (pkgKey == RPMAL_NOMATCH) {
/*@-boundswrite@*/
	ts->order[oc] = rpmteFree(ts->order[oc]);
/*@=boundswrite@*/
	ec = 1;
	goto exit;
    }
    (void) rpmteSetAddedKey(p, pkgKey);

#ifdef	NOYET
  /* XXX this needs a search over ts->order, not ts->addedPackages */
  { uint_32 *pp = NULL;

    /* XXX This should be added always so that packages look alike.
     * XXX However, there is logic in files.c/depends.c that checks for
     * XXX existence (rather than value) that will need to change as well.
     */
    if (hge(h, RPMTAG_MULTILIBS, NULL, (void **) &pp, NULL))
	multiLibMask = *pp;

    if (multiLibMask) {
	for (i = 0; i < pkgNum - 1; i++) {
	    if (!strcmp (rpmteN(p), al->list[i].name)
		&& hge(al->list[i].h, RPMTAG_MULTILIBS, NULL,
				  (void **) &pp, NULL)
		&& !rpmVersionCompare(p->h, al->list[i].h)
		&& *pp && !(*pp & multiLibMask))
		    (void) rpmteSetMultiLib(p, multiLibMask);
	}
    }
  }
#endif

    if (!duplicate) {
	ts->numAddedPackages++;
    }

    if (!upgrade)
	goto exit;

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (isSource)
	goto exit;

    /* Do lazy (readonly?) open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL) {
	if ((ec = rpmtsOpenDB(ts, ts->dbmode)) != 0)
	    goto exit;
    }

    {	rpmdbMatchIterator mi;
	Header h2;

	mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, rpmteN(p), 0);
	while((h2 = rpmdbNextIterator(mi)) != NULL) {
	    if (rpmVersionCompare(h, h2))
		xx = removePackage(ts, h2, rpmdbGetIteratorOffset(mi), pkgKey);
	    else {
		uint_32 *pp, multiLibMask = 0, oldmultiLibMask = 0;

		if (hge(h2, RPMTAG_MULTILIBS, NULL, (void **) &pp, NULL))
		    oldmultiLibMask = *pp;
		if (hge(h, RPMTAG_MULTILIBS, NULL, (void **) &pp, NULL))
		    multiLibMask = *pp;
		if (oldmultiLibMask && multiLibMask
		 && !(oldmultiLibMask & multiLibMask))
		{
		    (void) rpmteSetMultiLib(p, multiLibMask);
		}
	    }
	}
	mi = rpmdbFreeIterator(mi);
    }

    obsoletes = rpmdsLink(rpmteDS(p, RPMTAG_OBSOLETENAME), "Obsoletes");
    obsoletes = rpmdsInit(obsoletes);
    if (obsoletes != NULL)
    while (rpmdsNext(obsoletes) >= 0) {
	const char * Name;

	if ((Name = rpmdsN(obsoletes)) == NULL)
	    continue;	/* XXX can't happen */

	/* XXX avoid self-obsoleting packages. */
	if (!strcmp(rpmteN(p), Name))
		continue;

	{   rpmdbMatchIterator mi;
	    Header h2;

	    mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, Name, 0);

	    xx = rpmdbPruneIterator(mi,
		ts->removedPackages, ts->numRemovedPackages, 1);

	    while((h2 = rpmdbNextIterator(mi)) != NULL) {
		/*
		 * Rpm prior to 3.0.3 does not have versioned obsoletes.
		 * If no obsoletes version info is available, match all names.
		 */
		if (rpmdsEVR(obsoletes) == NULL
		 || headerMatchesDepFlags(h2, obsoletes))
		    xx = removePackage(ts, h2, rpmdbGetIteratorOffset(mi), pkgKey);
	    }
	    mi = rpmdbFreeIterator(mi);
	}
    }
    obsoletes = rpmdsFree(obsoletes);

    ec = 0;

exit:
    pi = rpmtsiFree(pi);
    return ec;
}

#ifdef	DYING
void rpmtsAvailablePackage(rpmts ts, Header h, fnpyKey key)
{
    int scareMem = 0;
    rpmds provides = rpmdsNew(h, RPMTAG_PROVIDENAME, scareMem);
    rpmfi fi = rpmfiNew(ts, NULL, h, RPMTAG_BASENAMES, scareMem);

    /* XXX FIXME: return code RPMAL_NOMATCH is error */
    (void) rpmalAdd(&ts->availablePackages, RPMAL_NOMATCH, key,
		provides, fi);
    fi = rpmfiFree(fi, 1);
    provides = rpmdsFree(provides);
}
#endif

int rpmtsAddEraseElement(rpmts ts, Header h, int dboffset)
{
    return removePackage(ts, h, dboffset, RPMAL_NOMATCH);
}

/**
 * Check dep for an unsatisfied dependency.
 * @todo Eliminate rpmrc provides.
 * @param ts		transaction set
 * @param dep		dependency
 * @return		0 if satisfied, 1 if not satisfied, 2 if error
 */
static int unsatisfiedDepend(rpmts ts, rpmds dep)
	/*@globals _cacheDependsRC, fileSystem @*/
	/*@modifies ts, _cacheDependsRC, fileSystem @*/
{
    DBT * key = alloca(sizeof(*key));
    DBT * data = alloca(sizeof(*data));
    rpmdbMatchIterator mi;
    const char * Name;
    Header h;
    int rc;
    int xx;

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
/*@-branchstate@*/
	    if ((DNEVR = rpmdsDNEVR(dep)) != NULL) {
		DBC * dbcursor = NULL;
		void * datap = NULL;
		size_t datalen = 0;
		size_t DNEVRlen = strlen(DNEVR);

		xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, 0);

		memset(key, 0, sizeof(*key));
/*@i@*/		key->data = (void *) DNEVR;
		key->size = DNEVRlen;
		memset(data, 0, sizeof(*data));
		data->data = datap;
		data->size = datalen;
		xx = dbiGet(dbi, dbcursor, key, data, DB_SET);
		DNEVR = key->data;
		DNEVRlen = key->size;
		datap = data->data;
		datalen = data->size;

/*@-boundswrite@*/
		if (xx == 0 && datap && datalen == 4)
		    memcpy(&rc, datap, datalen);
/*@=boundswrite@*/
		xx = dbiCclose(dbi, dbcursor, 0);
	    }
/*@=branchstate@*/

	    if (rc >= 0) {
		rpmdsNotify(dep, _("(cached)"), rc);
		return rc;
	    }
	}
    }

    rc = 0;	/* assume dependency is satisfied */

#if defined(DYING) || defined(__LCLINT__)
  { static /*@observer@*/ const char noProvidesString[] = "nada";
    static /*@observer@*/ const char * rcProvidesString = noProvidesString;
    int_32 Flags = rpmdsFlags(dep);
    const char * start;
    int i;

    if (rcProvidesString == noProvidesString)
	rcProvidesString = rpmGetVar(RPMVAR_PROVIDES);

    if (rcProvidesString != NULL && !(Flags & RPMSENSE_SENSEMASK)) {

	i = strlen(Name);
	/*@-observertrans -mayaliasunique@*/
	while ((start = strstr(rcProvidesString, Name))) {
	/*@=observertrans =mayaliasunique@*/
	    if (xisspace(start[i]) || start[i] == '\0' || start[i] == ',') {
		rpmdsNotify(dep, _("(rpmrc provides)"), rc);
		goto exit;
	    }
	    rcProvidesString = start + 1;
	}
    }
  }
#endif

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
    if (rpmalSatisfiesDepend(ts->addedPackages, dep, NULL) != NULL)
	goto exit;

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
	    if (rangeMatchesDepFlags(h, dep)) {
		rpmdsNotify(dep, _("(db provides)"), rc);
		mi = rpmdbFreeIterator(mi);
		goto exit;
	    }
	}
	mi = rpmdbFreeIterator(mi);

#if defined(DYING) || defined(__LCLINT__)
	mi = rpmtsInitIterator(ts, RPMTAG_NAME, Name, 0);
	(void) rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (rangeMatchesDepFlags(h, dep)) {
		rpmdsNotify(dep, _("(db package)"), rc);
		mi = rpmdbFreeIterator(mi);
		goto exit;
	    }
	}
	mi = rpmdbFreeIterator(mi);
#endif

    }

    /*
     * Search for an unsatisfied dependency.
     */
    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOSUGGEST) && ts->solve != NULL)
	xx = (*ts->solve) (ts, dep);

unsatisfied:
    rc = 1;	/* dependency is unsatisfied */
    rpmdsNotify(dep, NULL, rc);

exit:
    /*
     * If dbiOpen/dbiPut fails (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(rpmtsGetRdb(ts), RPMDBI_DEPENDS, 0);
	if (dbi == NULL) {
	    _cacheDependsRC = 0;
	} else {
	    const char * DNEVR;
	    xx = 0;
	    /*@-branchstate@*/
	    if ((DNEVR = rpmdsDNEVR(dep)) != NULL) {
		DBC * dbcursor = NULL;
		size_t DNEVRlen = strlen(DNEVR);

		xx = dbiCopen(dbi, dbi->dbi_txnid, &dbcursor, DB_WRITECURSOR);

		memset(key, 0, sizeof(*key));
/*@i@*/		key->data = (void *) DNEVR;
		key->size = DNEVRlen;
		memset(data, 0, sizeof(*data));
		data->data = &rc;
		data->size = sizeof(rc);

		/*@-compmempass@*/
		xx = dbiPut(dbi, dbcursor, key, data, 0);
		/*@=compmempass@*/
		xx = dbiCclose(dbi, dbcursor, DB_WRITECURSOR);
	    }
	    /*@=branchstate@*/
	    if (xx)
		_cacheDependsRC = 0;
	}
    }
    return rc;
}

/**
 * Check added requires/conflicts against against installed+added packages.
 * @param ts		transaction set
 * @param pkgNEVR	package name-version-release
 * @param requires	Requires: dependencies (or NULL)
 * @param conflicts	Conflicts: dependencies (or NULL)
 * @param depName	dependency name to filter (or NULL)
 * @param multiLib	skip multilib colored dependencies?
 * @param adding	dependency is from added package set?
 * @return		0 no problems found
 */
static int checkPackageDeps(rpmts ts, const char * pkgNEVR,
		/*@null@*/ rpmds requires, /*@null@*/ rpmds conflicts,
		/*@null@*/ const char * depName, uint_32 multiLib, int adding)
	/*@globals fileSystem @*/
	/*@modifies ts, requires, conflicts, fileSystem */
{
    const char * Name;
    int_32 Flags;
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

	Flags = rpmdsFlags(requires);

	/* If this requirement comes from the core package only, not libraries,
	   then if we're installing the libraries only, don't count it in. */
	if (multiLib && !isDependsMULTILIB(Flags))
	    continue;

	rc = unsatisfiedDepend(ts, requires);

	switch (rc) {
	case 0:		/* requirements are satisfied. */
	    /*@switchbreak@*/ break;
	case 1:		/* requirements are not satisfied. */
	{   fnpyKey * suggestedKeys = NULL;

	    /*@-branchstate@*/
	    if (ts->availablePackages != NULL) {
		suggestedKeys = rpmalAllSatisfiesDepend(ts->availablePackages,
				requires, NULL);
	    }
	    /*@=branchstate@*/

	    rpmdsProblem(ts->probs, pkgNEVR, requires, suggestedKeys, adding);

	}
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    /*@switchbreak@*/ break;
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

	Flags = rpmdsFlags(conflicts);

	/* If this requirement comes from the core package only, not libraries,
	   then if we're installing the libraries only, don't count it in. */
	if (multiLib && !isDependsMULTILIB(Flags))
	    continue;

	rc = unsatisfiedDepend(ts, conflicts);

	/* 1 == unsatisfied, 0 == satsisfied */
	switch (rc) {
	case 0:		/* conflicts exist. */
	    rpmdsProblem(ts->probs, pkgNEVR, conflicts, NULL, adding);
	    /*@switchbreak@*/ break;
	case 1:		/* conflicts don't exist. */
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    /*@switchbreak@*/ break;
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
		/*@only@*/ /*@null@*/ rpmdbMatchIterator mi, int adding)
	/*@globals fileSystem @*/
	/*@modifies ts, mi, fileSystem @*/
{
    int scareMem = 1;
    Header h;
    int ec = 0;

    (void) rpmdbPruneIterator(mi,
		ts->removedPackages, ts->numRemovedPackages, 1);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	const char * pkgNEVR;
	rpmds requires, conflicts;
	int rc;

	pkgNEVR = hGetNEVR(h, NULL);
	requires = rpmdsNew(h, RPMTAG_REQUIRENAME, scareMem);
	conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, scareMem);
	rc = checkPackageDeps(ts, pkgNEVR, requires, conflicts, dep, 0, adding);
	conflicts = rpmdsFree(conflicts);
	requires = rpmdsFree(requires);
	pkgNEVR = _free(pkgNEVR);

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
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/
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
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/
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
/*@observer@*/ /*@owned@*/ /*@null@*/
    const char * pname;
/*@observer@*/ /*@dependent@*/ /*@null@*/
    const char * qname;
};

#ifdef REFERENCE
static struct badDeps_s {
/*@observer@*/ /*@null@*/ const char * pname;
/*@observer@*/ /*@null@*/ const char * qname;
} badDeps[] = {
    { "libtermcap", "bash" },
    { "modutils", "vixie-cron" },
    { "ypbind", "yp-tools" },
    { "ghostscript-fonts", "ghostscript" },
    /* 7.2 only */
    { "libgnomeprint15", "gnome-print" },
    { "nautilus", "nautilus-mozilla" },
    /* 7.1 only */
    { "arts", "kdelibs-sound" },
    /* 7.0 only */
    { "pango-gtkbeta-devel", "pango-gtkbeta" },
    { "XFree86", "Mesa" },
    { "compat-glibc", "db2" },
    { "compat-glibc", "db1" },
    { "pam", "initscripts" },
    { "initscripts", "sysklogd" },
    /* 6.2 */
    { "egcs-c++", "libstdc++" },
    /* 6.1 */
    { "pilot-link-devel", "pilot-link" },
    /* 5.2 */
    { "pam", "pamconfig" },
    { NULL, NULL }
};
#else
/*@unchecked@*/
static int badDepsInitialized = 0;

/*@unchecked@*/ /*@only@*/ /*@null@*/
static struct badDeps_s * badDeps = NULL;
#endif

/**
 */
/*@-modobserver -observertrans @*/
static void freeBadDeps(void)
	/*@globals badDeps, badDepsInitialized @*/
	/*@modifies badDeps, badDepsInitialized @*/
{
    if (badDeps) {
	struct badDeps_s * bdp;
	for (bdp = badDeps; bdp->pname != NULL && bdp->qname != NULL; bdp++)
	    bdp->pname = _free(bdp->pname);
	badDeps = _free(badDeps);
    }
    badDepsInitialized = 0;
}
/*@=modobserver =observertrans @*/

/**
 * Check for dependency relations to be ignored.
 *
 * @param p	successor element (i.e. with Requires: )
 * @param q	predecessor element (i.e. with Provides: )
 * @return	1 if dependency is to be ignored.
 */
static int ignoreDep(const rpmte p, const rpmte q)
	/*@globals badDeps, badDepsInitialized @*/
	/*@modifies badDeps, badDepsInitialized @*/
{
    struct badDeps_s * bdp;

/*@-globs -mods@*/
    if (!badDepsInitialized) {
	char * s = rpmExpand("%{?_dependency_whiteout}", NULL);
	const char ** av = NULL;
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
		/*@-usereleased@*/
		bdp->qname = qname;
		/*@=usereleased@*/
		rpmMessage(RPMMESS_DEBUG,
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
/*@=globs =mods@*/

    /*@-compdef@*/
    if (badDeps != NULL)
    for (bdp = badDeps; bdp->pname != NULL && bdp->qname != NULL; bdp++) {
	if (!strcmp(rpmteN(p), bdp->pname) && !strcmp(rpmteN(q), bdp->qname))
	    return 1;
    }
    return 0;
    /*@=compdef@*/
}

/**
 * Recursively mark all nodes with their predecessors.
 * @param tsi		successor chain
 * @param q		predecessor
 */
static void markLoop(/*@special@*/ tsortInfo tsi, rpmte q)
	/*@globals internalState @*/
	/*@uses tsi @*/
	/*@modifies internalState @*/
{
    rpmte p;

    /*@-branchstate@*/ /* FIX: q is kept */
    while (tsi != NULL && (p = tsi->tsi_suc) != NULL) {
	tsi = tsi->tsi_next;
	if (rpmteTSI(p)->tsi_chain != NULL)
	    continue;
	/*@-assignexpose -temptrans@*/
	rpmteTSI(p)->tsi_chain = q;
	/*@=assignexpose =temptrans@*/
	if (rpmteTSI(p)->tsi_next != NULL)
	    markLoop(rpmteTSI(p)->tsi_next, p);
    }
    /*@=branchstate@*/
}

static inline /*@observer@*/ const char * const identifyDepend(int_32 f)
	/*@*/
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
 * @return		(possibly NULL) formatted "q <- p" releation (malloc'ed)
 */
/*@-boundswrite@*/
/*@-mustmod@*/ /* FIX: hack modifies, but -type disables */
static /*@owned@*/ /*@null@*/ const char *
zapRelation(rpmte q, rpmte p,
		/*@null@*/ rpmds requires,
		int zap, /*@in@*/ /*@out@*/ int * nzaps)
	/*@modifies q, p, requires, *nzaps @*/
{
    tsortInfo tsi_prev;
    tsortInfo tsi;
    const char *dp = NULL;

    for (tsi_prev = rpmteTSI(q), tsi = rpmteTSI(q)->tsi_next;
	 tsi != NULL;
	/* XXX Note: the loop traverses "not found", break on "found". */
	/*@-nullderef@*/
	 tsi_prev = tsi, tsi = tsi->tsi_next)
	/*@=nullderef@*/
    {
	int_32 Flags;

	/*@-abstract@*/
	if (tsi->tsi_suc != p)
	    continue;
	/*@=abstract@*/

	if (requires == NULL) continue;		/* XXX can't happen */

	(void) rpmdsSetIx(requires, tsi->tsi_reqx);

	Flags = rpmdsFlags(requires);

	dp = rpmdsNewDNEVR( identifyDepend(Flags), requires);

	/*
	 * Attempt to unravel a dependency loop by eliminating Requires's.
	 */
	/*@-branchstate@*/
	if (zap && !(Flags & RPMSENSE_PREREQ)) {
	    rpmMessage(RPMMESS_DEBUG,
			_("removing %s \"%s\" from tsort relations.\n"),
			(rpmteNEVR(p) ?  rpmteNEVR(p) : "???"), dp);
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
	/*@=branchstate@*/
	/* XXX Note: the loop traverses "not found", get out now! */
	break;
    }
    return dp;
}
/*@=mustmod@*/
/*@=boundswrite@*/

static void prtTSI(const char * msg, tsortInfo tsi)
	/*@globals fileSystem@*/
	/*@modifies fileSystem@*/
{
/*@-nullpass@*/
if (_tso_debug) {
    if (msg) fprintf(stderr, "%s", msg);
/*@i@*/ fprintf(stderr, " tsi %p suc %p next %p chain %p reqx %d qcnt %d\n", tsi, tsi->tsi_suc, tsi->tsi_next, tsi->tsi_chain, tsi->tsi_reqx, tsi->tsi_qcnt);
}
/*@=nullpass@*/
}

/**
 * Record next "q <- p" relation (i.e. "p" requires "q").
 * @param ts		transaction set
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param selected	boolean package selected array
 * @param requires	relation
 * @return		0 always
 */
/*@-mustmod@*/
static inline int addRelation(rpmts ts,
		/*@dependent@*/ rpmte p,
		unsigned char * selected,
		rpmds requires)
	/*@globals fileSystem @*/
	/*@modifies ts, p, *selected, fileSystem @*/
{
    rpmtsi qi; rpmte q;
    tsortInfo tsi;
    const char * Name;
    fnpyKey key;
    alKey pkgKey;
    int i = 0;

    if ((Name = rpmdsN(requires)) == NULL)
	return 0;	/* XXX can't happen */

    /* Avoid rpmlib feature dependencies. */
    if (!strncmp(Name, "rpmlib(", sizeof("rpmlib(")-1))
	return 0;

    pkgKey = RPMAL_NOMATCH;
    key = rpmalSatisfiesDepend(ts->addedPackages, requires, &pkgKey);

if (_tso_debug)
fprintf(stderr, "addRelation: pkgKey %ld\n", (long)pkgKey);

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
    if (ignoreDep(p, q))
	return 0;

/*@-nullpass -nullderef -formattype@*/
if (_tso_debug)
fprintf(stderr, "addRelation: q %p(%s) from %p[%d:%d]\n", q, rpmteN(q), ts->order, i, ts->orderCount);
/*@=nullpass =nullderef =formattype@*/

    /* Avoid redundant relations. */
    /* XXX TODO: add control bit. */
    if (selected[i] != 0)
	return 0;
/*@-boundswrite@*/
    selected[i] = 1;
/*@=boundswrite@*/
/*@-nullpass@*/
if (_tso_debug)
fprintf(stderr, "addRelation: selected[%d] = 1\n", i);
/*@=nullpass@*/

    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
    rpmteTSI(p)->tsi_count++;			/* bump p predecessor count */

    if (rpmteDepth(p) <= rpmteDepth(q))	/* Save max. depth in dependency tree */
	(void) rpmteSetDepth(p, (rpmteDepth(q) + 1));

/*@-nullpass@*/
if (_tso_debug)
/*@i@*/ fprintf(stderr, "addRelation: p %p(%s) depth %d", p, rpmteN(p), rpmteDepth(p));
prtTSI(NULL, rpmteTSI(p));
/*@=nullpass@*/

    tsi = xcalloc(1, sizeof(*tsi));
    tsi->tsi_suc = p;

    tsi->tsi_reqx = rpmdsIx(requires);

    tsi->tsi_next = rpmteTSI(q)->tsi_next;
/*@-nullpass -compmempass@*/
prtTSI("addRelation: new", tsi);
if (_tso_debug)
/*@i@*/ fprintf(stderr, "addRelation: BEFORE q %p(%s)", q, rpmteN(q));
prtTSI(NULL, rpmteTSI(q));
/*@=nullpass =compmempass@*/
/*@-mods@*/
    rpmteTSI(q)->tsi_next = tsi;
    rpmteTSI(q)->tsi_qcnt++;			/* bump q successor count */
/*@=mods@*/
/*@-nullpass -compmempass@*/
if (_tso_debug)
/*@i@*/ fprintf(stderr, "addRelation:  AFTER q %p(%s)", q, rpmteN(q));
prtTSI(NULL, rpmteTSI(q));
/*@=nullpass =compmempass@*/
    return 0;
}
/*@=mustmod@*/

/**
 * Compare ordered list entries by index (qsort/bsearch).
 * @param one		1st ordered list entry
 * @param two		2nd ordered list entry
 * @return		result of comparison
 */
static int orderListIndexCmp(const void * one, const void * two)	/*@*/
{
    /*@-castexpose@*/
    long a = (long) ((const orderListIndex)one)->pkgKey;
    long b = (long) ((const orderListIndex)two)->pkgKey;
    /*@=castexpose@*/
    return (a - b);
}

/**
 * Add element to list sorting by tsi_qcnt.
 * @param p		new element
 * @retval qp		address of first element
 * @retval rp		address of last element
 */
/*@-boundswrite@*/
/*@-mustmod@*/
static void addQ(/*@dependent@*/ rpmte p,
		/*@in@*/ /*@out@*/ rpmte * qp,
		/*@in@*/ /*@out@*/ rpmte * rp)
	/*@modifies p, *qp, *rp @*/
{
    rpmte q, qprev;

    /* Mark the package as queued. */
    rpmteTSI(p)->tsi_reqx = 1;

    if ((*rp) == NULL) {	/* 1st element */
	/*@-dependenttrans@*/ /* FIX: double indirection */
	(*rp) = (*qp) = p;
	/*@=dependenttrans@*/
	return;
    }

    /* Find location in queue using metric tsi_qcnt. */
    for (qprev = NULL, q = (*qp);
	 q != NULL;
	 qprev = q, q = rpmteTSI(q)->tsi_suc)
    {
	if (rpmteTSI(q)->tsi_qcnt <= rpmteTSI(p)->tsi_qcnt)
	    break;
    }

    if (qprev == NULL) {	/* insert at beginning of list */
	rpmteTSI(p)->tsi_suc = q;
	/*@-dependenttrans@*/
	(*qp) = p;		/* new head */
	/*@=dependenttrans@*/
    } else if (q == NULL) {	/* insert at end of list */
	rpmteTSI(qprev)->tsi_suc = p;
	/*@-dependenttrans@*/
	(*rp) = p;		/* new tail */
	/*@=dependenttrans@*/
    } else {			/* insert between qprev and q */
	rpmteTSI(p)->tsi_suc = q;
	rpmteTSI(qprev)->tsi_suc = p;
    }
}
/*@=mustmod@*/
/*@=boundswrite@*/

/*@-bounds@*/
int rpmtsOrder(rpmts ts)
{
    rpmds requires;
    int_32 Flags;

#ifdef	DYING
    int chainsaw = rpmtsFlags(ts) & RPMTRANS_FLAG_CHAINSAW;
#else
    int chainsaw = 1;
#endif
    rpmtsi pi; rpmte p;
    rpmtsi qi; rpmte q;
    rpmtsi ri; rpmte r;
    tsortInfo tsi;
    tsortInfo tsi_next;
    alKey * ordering;
    int orderingCount = 0;
    unsigned char * selected = alloca(sizeof(*selected) * (ts->orderCount + 1));
    int loopcheck;
    rpmte * newOrder;
    int newOrderCount = 0;
    orderListIndex orderList;
    int numOrderList;
    int nrescans = 10;
    int _printed = 0;
    char deptypechar;
#ifdef	DYING
    int oType = TR_ADDED;
#else
    int oType = 0;
#endif
    int treex;
    int depth;
    int qlen;
    int i, j;

    rpmalMakeIndex(ts->addedPackages);

/*@-modfilesystem -nullpass -formattype@*/
if (_tso_debug)
fprintf(stderr, "*** rpmtsOrder(%p) order %p[%d]\n", ts, ts->order, ts->orderCount);
/*@=modfilesystem =nullpass =formattype@*/

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

    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL)
	rpmteNewTSI(p);
    pi = rpmtsiFree(pi);

    /* Record all relations. */
    rpmMessage(RPMMESS_DEBUG, _("========== recording tsort relations\n"));
    pi = rpmtsiInit(ts);
    /* XXX Only added packages are ordered (for now). */
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
		if (isInstallPreReq(Flags)
		 || !( isErasePreReq(Flags)
		    || isLegacyPreReq(Flags) )
		    )
		    /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
	    case TR_ADDED:
		/* Skip if not %pre/%post requires or legacy prereq. */
		if (isErasePreReq(Flags)
		 || !( isInstallPreReq(Flags)
		    || isLegacyPreReq(Flags) )
		    )
		    /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
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
		 ||  ( isErasePreReq(Flags)
		    || isLegacyPreReq(Flags) )
		    )
		    /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
	    case TR_ADDED:
		/* Skip if %pre/%post requires or legacy prereq. */
		if (isErasePreReq(Flags)
		 ||  ( isInstallPreReq(Flags)
		    || isLegacyPreReq(Flags) )
		    )
		    /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
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

	if (npreds == 0)
	    (void) rpmteSetTree(p, treex++);
	else
	    (void) rpmteSetTree(p, -1);
#ifdef	UNNECESSARY
	(void) rpmteSetParent(p, NULL);
#endif

/*@-modfilesystem -nullpass @*/
if (_tso_debug)
/*@i@*/ fprintf(stderr, "\t+++ %p[%d] %s npreds %d\n", p, rpmtsiOc(pi), rpmteNEVR(p), rpmteNpreds(p));
/*@=modfilesystem =nullpass @*/

    }
    pi = rpmtsiFree(pi);

    /* T4. Scan for zeroes. */
    rpmMessage(RPMMESS_DEBUG, _("========== tsorting packages (order, #predecessors, #succesors, tree, depth)\n"));

rescan:
    if (pi != NULL) pi = rpmtsiFree(pi);
    q = r = NULL;
    qlen = 0;
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, oType)) != NULL) {

	/* Prefer packages in chainsaw or presentation order. */
	if (!chainsaw)
	    rpmteTSI(p)->tsi_qcnt = (ts->orderCount - rpmtsiOc(pi));

	if (rpmteTSI(p)->tsi_count != 0)
	    continue;
	rpmteTSI(p)->tsi_suc = NULL;
	addQ(p, &q, &r);
	qlen++;
/*@-modfilesystem -nullpass @*/
if (_tso_debug)
/*@i@*/ fprintf(stderr, "\t+++ addQ ++ qlen %d p %p(%s)", qlen, p, rpmteNEVR(p));
prtTSI(" p", rpmteTSI(p));
/*@=modfilesystem =nullpass @*/
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
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    if (!(oType & TR_REMOVED))
		continue;
	    /*@switchbreak@*/ break;
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}
	deptypechar = (rpmteType(q) == TR_REMOVED ? '-' : '+');

	rpmMessage(RPMMESS_DEBUG, "%5d%5d%5d%5d%5d %*s%c%s\n",
			orderingCount, rpmteNpreds(q),
			rpmteTSI(q)->tsi_qcnt, rpmteTree(q), rpmteDepth(q),
			(2 * rpmteDepth(q)), "",
			deptypechar,
			(rpmteNEVR(q) ? rpmteNEVR(q) : "???"));

	treex = rpmteTree(q);
	depth = rpmteDepth(q);
	(void) rpmteSetDegree(q, 0);

	ordering[orderingCount] = rpmteAddedKey(q);
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
		addQ(p, &rpmteTSI(q)->tsi_suc, &r);
		qlen++;
/*@-modfilesystem -nullpass @*/
if (_tso_debug)
/*@i@*/ fprintf(stderr, "\t+++ addQ ++ qlen %d p %p(%s)", qlen, p, rpmteNEVR(p));
prtTSI(" p", rpmteTSI(p));
/*@=modfilesystem =nullpass @*/
	    }
	    tsi = _free(tsi);
	}
	if (!_printed && loopcheck == qlen && rpmteTSI(q)->tsi_suc != NULL) {
	    _printed++;
	    rpmMessage(RPMMESS_DEBUG,
		_("========== successors only (presentation order)\n"));

	    /* Relink the queue in presentation order. */
	    tsi = rpmteTSI(q);
	    pi = rpmtsiInit(ts);
	    while ((p = rpmtsiNext(pi, oType)) != NULL) {
		/* Is this element in the queue? */
		if (rpmteTSI(p)->tsi_reqx == 0)
		    /*@innercontinue@*/ continue;
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
		    /*@innerbreak@*/ break;
		rpmteTSI(q)->tsi_reqx = 1;
	    }

	    /* T13. Print predecessor chain from start of loop. */
	    while ((p = q) != NULL && (q = rpmteTSI(p)->tsi_chain) != NULL) {
		const char * dp;
		char buf[4096];

		/* Unchain predecessor loop. */
		rpmteTSI(p)->tsi_chain = NULL;

		if (!printed) {
		    rpmMessage(RPMMESS_DEBUG, _("LOOP:\n"));
		    printed = 1;
		}

		/* Find (and destroy if co-requisite) "q <- p" relation. */
		requires = rpmteDS(p, RPMTAG_REQUIRENAME);
		requires = rpmdsInit(requires);
		if (requires == NULL)
		    /*@innercontinue@*/ continue;	/* XXX can't happen */
		dp = zapRelation(q, p, requires, 1, &nzaps);

		/* Print next member of loop. */
		buf[0] = '\0';
		if (rpmteNEVR(p) != NULL)
		    (void) stpcpy(buf, rpmteNEVR(p));
		rpmMessage(RPMMESS_DEBUG, "    %-40s %s\n", buf,
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
	    rpmMessage(RPMMESS_DEBUG, _("========== continuing tsort ...\n"));
	    goto rescan;
	}

	/* Return no. of packages that could not be ordered. */
	rpmMessage(RPMMESS_ERROR, _("rpmtsOrder failed, %d elements remain\n"),
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
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	    orderList[j].pkgKey = RPMAL_NOMATCH;
	    /*@switchbreak@*/ break;
	}
	orderList[j].orIndex = rpmtsiOc(pi);
	j++;
    }
    pi = rpmtsiFree(pi);

    qsort(orderList, numOrderList, sizeof(*orderList), orderListIndexCmp);

/*@-type@*/
    newOrder = xcalloc(ts->orderCount, sizeof(*newOrder));
/*@=type@*/
    /*@-branchstate@*/
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
	if ((q = ts->order[j]) == NULL)
	    continue;

	newOrder[newOrderCount++] = q;
	ts->order[j] = NULL;
	if (!chainsaw)
	for (j = needle->orIndex + 1; j < ts->orderCount; j++) {
	    if ((q = ts->order[j]) == NULL)
		/*@innerbreak@*/ break;
	    if (rpmteType(q) == TR_REMOVED
	     && rpmteDependsOnKey(q) == needle->pkgKey)
	    {
		newOrder[newOrderCount++] = q;
		ts->order[j] = NULL;
	    } else
		/*@innerbreak@*/ break;
	}
    }
    /*@=branchstate@*/

    for (j = 0; j < ts->orderCount; j++) {
	if ((p = ts->order[j]) == NULL)
	    continue;
	newOrder[newOrderCount++] = p;
	ts->order[j] = NULL;
    }
assert(newOrderCount == ts->orderCount);

/*@+voidabstract@*/
    ts->order = _free(ts->order);
/*@=voidabstract@*/
    ts->order = newOrder;
    ts->orderAlloced = ts->orderCount;
    orderList = _free(orderList);

#ifdef	DYING
    /* Clean up after dependency checks */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	rpmteCleanDS(p);
    }
    pi = rpmtsiFree(pi);

    ts->addedPackages = rpmalFree(ts->addedPackages);
    ts->numAddedPackages = 0;
#else
    rpmtsClean(ts);
#endif
    freeBadDeps();

    return 0;
}
/*@=bounds@*/

/**
 * Close a single database index.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @return              0 on success
 */
/*@-mustmod -type@*/ /* FIX: this belongs in rpmdb.c */
static int rpmdbCloseDBI(/*@null@*/ rpmdb db, int rpmtag)
	/*@globals fileSystem @*/
	/*@modifies db, fileSystem @*/
{
    int dbix;
    int rc = 0;

    if (db == NULL || db->_dbi == NULL || dbiTags == NULL)
	return 0;

    for (dbix = 0; dbix < dbiTagsMax; dbix++) {
	if (dbiTags[dbix] != rpmtag)
	    continue;
/*@-boundswrite@*/
	if (db->_dbi[dbix] != NULL) {
	    int xx;
	    /*@-unqualifiedtrans@*/		/* FIX: double indirection. */
	    xx = dbiClose(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	    /*@=unqualifiedtrans@*/
	}
/*@=boundswrite@*/
	break;
    }
    return rc;
}
/*@=mustmod =type@*/

int rpmtsCheck(rpmts ts)
{
    rpmdbMatchIterator mi = NULL;
    rpmtsi pi = NULL; rpmte p;
    int closeatexit = 0;
    int xx;
    int rc;

    /* Do lazy, readonly, open of rpm database. */
    if (rpmtsGetRdb(ts) == NULL) {
	if ((rc = rpmtsOpenDB(ts, ts->dbmode)) != 0)
	    goto exit;
	closeatexit = 1;
    }

    ts->probs = rpmpsFree(ts->probs);
    ts->probs = rpmpsCreate();

    rpmalMakeIndex(ts->addedPackages);
    rpmalMakeIndex(ts->availablePackages);

    /*
     * Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, TR_ADDED)) != NULL) {
	rpmds provides;

        rpmMessage(RPMMESS_DEBUG,  "========== +++ %s\n" , rpmteNEVR(p));
	rc = checkPackageDeps(ts, rpmteNEVR(p),
			rpmteDS(p, RPMTAG_REQUIRENAME),
			rpmteDS(p, RPMTAG_CONFLICTNAME),
			NULL,
			rpmteMultiLib(p), 1);
	if (rc)
	    goto exit;

#if defined(DYING) || defined(__LCLINT__)
	/* XXX all packages now have Provides: name = version-release */
	/* Adding: check name against conflicts matches. */
	rc = checkDependentConflicts(ts, rpmteN(p));
	if (rc)
	    goto exit;
#endif

	rc = 0;
	provides = rpmteDS(p, RPMTAG_PROVIDENAME);
	provides = rpmdsInit(provides);
	if (provides == NULL || rpmdsN(provides) == NULL)
	    continue;
	while (rpmdsNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = rpmdsN(provides)) == NULL)
		/*@innercontinue@*/ continue;	/* XXX can't happen */

	    /* Adding: check provides key against conflicts matches. */
	    if (!checkDependentConflicts(ts, Name))
		/*@innercontinue@*/ continue;
	    rc = 1;
	    /*@innerbreak@*/ break;
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

	rpmMessage(RPMMESS_DEBUG,  "========== --- %s\n" , rpmteNEVR(p));

#if defined(DYING) || defined(__LCLINT__)
	/* XXX all packages now have Provides: name = version-release */
	/* Erasing: check name against requiredby matches. */
	rc = checkDependentPackages(ts, rpmteN(p));
	if (rc)
		goto exit;
#endif

	rc = 0;
	provides = rpmteDS(p, RPMTAG_PROVIDENAME);
	provides = rpmdsInit(provides);
	if (provides != NULL)
	while (rpmdsNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = rpmdsN(provides)) == NULL)
		/*@innercontinue@*/ continue;	/* XXX can't happen */

	    /* Erasing: check provides against requiredby matches. */
	    if (!checkDependentPackages(ts, Name))
		/*@innercontinue@*/ continue;
	    rc = 1;
	    /*@innerbreak@*/ break;
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
		/*@innercontinue@*/ continue;
	    rc = 1;
	    /*@innerbreak@*/ break;
	}
	if (rc)
	    goto exit;
    }
    pi = rpmtsiFree(pi);

    rc = 0;

exit:
    mi = rpmdbFreeIterator(mi);
    pi = rpmtsiFree(pi);
    /*@-branchstate@*/
    if (closeatexit)
	xx = rpmtsCloseDB(ts);
    else if (_cacheDependsRC)
	xx = rpmdbCloseDBI(rpmtsGetRdb(ts), RPMDBI_DEPENDS);
    /*@=branchstate@*/
    return rc;
}
/*@=boundsread@*/
