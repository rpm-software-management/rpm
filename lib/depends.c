/** \ingroup rpmdep
 * \file lib/depends.c
 */

#define	_DS_SCAREMEM	0

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>		/* XXX rpmGetPath */
#include <rpmpgp.h>		/* XXX pgpFreeDig */

#include "rpmds.h"

#define _NEED_TEITERATOR	1
#include "depends.h"

#include "rpmal.h"
#include "rpmdb.h"		/* XXX response cache needs dbiOpen et al. */

#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */
/*@access FD_t@*/		/* XXX compared with NULL */
/*@access rpmdb@*/		/* XXX compared with NULL */
/*@access rpmdbMatchIterator@*/	/* XXX compared with NULL */

/*@access tsortInfo@*/
/*@access rpmTransactionSet@*/

/*@access alKey @*/
/*@access rpmDependencyConflict@*/
/*@access problemsSet@*/

/**
 */
typedef /*@abstract@*/ struct orderListIndex_s *	orderListIndex;
/*@access orderListIndex@*/

/**
 */
struct orderListIndex_s {
    alKey pkgKey;
    int orIndex;
};

/*@unchecked@*/
int _cacheDependsRC = 1;

/*@unchecked@*/
int _te_debug = 0;

/*@unchecked@*/
int _ts_debug = 0;

/*@observer@*/ /*@unchecked@*/
const char *rpmNAME = PACKAGE;

/*@observer@*/ /*@unchecked@*/
const char *rpmEVR = VERSION;

/*@unchecked@*/
int rpmFLAGS = RPMSENSE_EQUAL;

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
	/*@-globs -mods@*/ /* FIX: rpmGlobalMacroContext */
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
    /*@-mods -onlytrans -type@*/ /* FIX: rpmdb excision */
    return rpmdbInitIterator(ts->rpmdb, rpmtag, keyp, keylen);
    /*@=mods =onlytrans =type@*/
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
	/*@-assignexpose@*/
	ts->rpmdb = rpmdbLink(db, "tsCreate");
	/*@=assignexpose@*/
	ts->dbmode = db->db_mode;
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
    ts->addedPackages = alCreate(ts->delta);

    ts->numAvailablePackages = 0;
    ts->availablePackages = alCreate(ts->delta);

    ts->orderAlloced = ts->delta;
    ts->orderCount = 0;
    ts->order = xcalloc(ts->orderAlloced, sizeof(*ts->order));

    ts->sig = NULL;
    ts->dig = NULL;

    ts->nrefs = 0;

    return rpmtsLink(ts, "tsCreate");
}

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
 * @param dboffset	rpm database instance
 * @param depends	installed package of pair (or RPMAL_NOMATCH on erase)
 * @return		0 on success
 */
static int removePackage(rpmTransactionSet ts, int dboffset,
		alKey depends)
	/*@modifies ts @*/
{
    transactionElement p;

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
	qsort(ts->removedPackages, ts->numRemovedPackages,
			sizeof(*ts->removedPackages), intcmp);
    }

    if (ts->orderCount == ts->orderAlloced) {
	ts->orderAlloced += ts->delta;
	ts->order = xrealloc(ts->order, sizeof(*ts->order) * ts->orderAlloced);
    }

    p = ts->order + ts->orderCount;
    memset(p, 0, sizeof(*p));
    p->type = TR_REMOVED;
    p->u.removed.dboffset = dboffset;
    /*@-assignexpose -temptrans@*/
    p->u.removed.dependsOnKey = depends;
    /*@=assignexpose =temptrans@*/
    ts->orderCount++;

    return 0;
}

char * hGetNVR(Header h, const char ** np)
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

int rpmtransAddPackage(rpmTransactionSet ts, Header h, FD_t fd,
			fnpyKey key, int upgrade, rpmRelocation * relocs)
{
    int scareMem = _DS_SCAREMEM;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char * name = NULL;
    const char * addNVR = hGetNVR(h, &name);
    const char * pkgNVR = NULL;
    int duplicate = 0;
    transactionElement p;
    rpmDepSet obsoletes;
    alKey pkgKey;	/* addedPackages key */
    int apx;			/* addedPackages index */
    int xx;
    int ec = 0;
    int rc;
    int i;

    /*
     * Check for previously added versions with the same name.
     */
    i = ts->orderCount;
    apx = 0;
    if ((p = ts->order) != NULL)
    for (i = 0; i < ts->orderCount; i++, p++) {
	const char * pname;
	Header ph;

	/* XXX Only added packages are checked for dupes (for now). */
	switch (p->type) {
	case TR_ADDED:
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}

	apx++;

	ph = alGetHeader(ts->addedPackages, p->u.addedKey, 0);
	if (ph == NULL)
	    break;

	pkgNVR = _free(pkgNVR);
	pname = NULL;
	pkgNVR = hGetNVR(ph, &pname);

	if (strcmp(pname, name)) {
            pkgNVR = _free(pkgNVR);
            ph = headerFree(ph, "alGetHeader nomatch");
            continue;
	}
	
	rc = rpmVersionCompare(ph, h);
	ph = headerFree(ph, "alGetHeader match");

	if (rc > 0) {
	    rpmMessage(RPMMESS_WARNING,
		_("newer package %s already added, skipping %s\n"),
		pkgNVR, addNVR);
	    goto exit;
	} else if (rc == 0) {
	    rpmMessage(RPMMESS_WARNING,
		_("package %s already added, ignoring\n"),
		pkgNVR);
	    goto exit;
	} else {
	    rpmMessage(RPMMESS_WARNING,
		_("older package %s already added, replacing with %s\n"),
		pkgNVR, addNVR);
	    duplicate = 1;
	}
	break;
    }

    /* XXX Note: i == ts->orderCount here almost always. */
    if (i == ts->orderAlloced) {
	ts->orderAlloced += ts->delta;
	ts->order = xrealloc(ts->order, ts->orderAlloced * sizeof(*ts->order));
    }
    /* XXX cast assumes that available keys are indices, not pointers */
    pkgKey = alAddPackage(ts->addedPackages, (alKey)apx, key, h);
    if (pkgKey == RPMAL_NOMATCH) {
	ec = 1;
	goto exit;
    }
    p = ts->order + i;
    memset(p, 0, sizeof(*p));
    
    p->u.addedKey = pkgKey;

    p->type = TR_ADDED;
    p->multiLib = 0;

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
	    if (!strcmp (p->name, al->list[i].name)
		&& hge(al->list[i].h, RPMTAG_MULTILIBS, NULL,
				  (void **) &pp, NULL)
		&& !rpmVersionCompare(p->h, al->list[i].h)
		&& *pp && !(*pp & multiLibMask))
		    p->multiLib = multiLibMask;
	}
    }
  }
#endif

    /*@-assignexpose -ownedtrans @*/
    p->key = key;
    /*@=assignexpose =ownedtrans @*/
    /*@-type@*/ /* FIX: cast? */
    p->fd = (fd != NULL ? fdLink(fd, "rpmtransAddPackage") : NULL);
    /*@=type@*/

    if (relocs) {
	rpmRelocation * r;

	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++)
	    {};
	p->relocs = xmalloc((i + 1) * sizeof(*p->relocs));

	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++) {
	    p->relocs[i].oldPath = r->oldPath ? xstrdup(r->oldPath) : NULL;
	    p->relocs[i].newPath = r->newPath ? xstrdup(r->newPath) : NULL;
	}
	p->relocs[i].oldPath = NULL;
	p->relocs[i].newPath = NULL;
    } else {
	p->relocs = NULL;
    }

    if (!duplicate) {
assert(apx == ts->numAddedPackages);
	ts->numAddedPackages++;
	ts->orderCount++;
    }

    if (!upgrade)
	goto exit;

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (headerIsEntry(h, RPMTAG_SOURCEPACKAGE))
	goto exit;

    /* Do lazy (readonly?) open of rpm database. */
    if (ts->rpmdb == NULL) {
	if ((ec = rpmtsOpenDB(ts, ts->dbmode)) != 0)
	    goto exit;
    }

    {	rpmdbMatchIterator mi;
	Header h2;

	mi = rpmtsInitIterator(ts, RPMTAG_NAME, name, 0);
	while((h2 = rpmdbNextIterator(mi)) != NULL) {
	    /*@-branchstate@*/
	    if (rpmVersionCompare(h, h2))
		xx = removePackage(ts, rpmdbGetIteratorOffset(mi), pkgKey);
	    else {
		uint_32 *pp, multiLibMask = 0, oldmultiLibMask = 0;

		if (hge(h2, RPMTAG_MULTILIBS, NULL, (void **) &pp, NULL))
		    oldmultiLibMask = *pp;
		if (hge(h, RPMTAG_MULTILIBS, NULL, (void **) &pp, NULL))
		    multiLibMask = *pp;
		if (oldmultiLibMask && multiLibMask
		 && !(oldmultiLibMask & multiLibMask))
		{
		    p->multiLib = multiLibMask;
		}
	    }
	    /*@=branchstate@*/
	}
	mi = rpmdbFreeIterator(mi);
    }

    obsoletes = dsiInit(dsNew(h, RPMTAG_OBSOLETENAME, scareMem));
    if (obsoletes != NULL)
    while (dsiNext(obsoletes) >= 0) {
	const char * Name;

	if ((Name = dsiGetN(obsoletes)) == NULL)
	    continue;	/* XXX can't happen */

	/* XXX avoid self-obsoleting packages. */
	if (!strcmp(name, Name))
		continue;

	{   rpmdbMatchIterator mi;
	    Header h2;

	    mi = rpmtsInitIterator(ts, RPMTAG_NAME, Name, 0);

	    xx = rpmdbPruneIterator(mi,
		ts->removedPackages, ts->numRemovedPackages, 1);

	    while((h2 = rpmdbNextIterator(mi)) != NULL) {
		/*
		 * Rpm prior to 3.0.3 does not have versioned obsoletes.
		 * If no obsoletes version info is available, match all names.
		 */
		/*@-branchstate@*/
		if (dsiGetEVR(obsoletes) == NULL
		 || headerMatchesDepFlags(h2, obsoletes))
		    xx = removePackage(ts, rpmdbGetIteratorOffset(mi), pkgKey);
		/*@=branchstate@*/
	    }
	    mi = rpmdbFreeIterator(mi);
	}
    }
    /*@=nullstate@*/ /* FIX: obsoletes->EVR may be NULL */
    obsoletes = dsFree(obsoletes);
    /*@=nullstate@*/

    ec = 0;

exit:
    pkgNVR = _free(pkgNVR);
    addNVR = _free(addNVR);
    return ec;
}

void rpmtransAvailablePackage(rpmTransactionSet ts, Header h, fnpyKey key)
{
    /* XXX FIXME: return code RPMAL_NOMATCH is error */
    (void) alAddPackage(ts->availablePackages, RPMAL_NOMATCH, key, h);
}

int rpmtransRemovePackage(rpmTransactionSet ts, int dboffset)
{
    return removePackage(ts, dboffset, RPMAL_NOMATCH);
}

/*@-nullstate@*/ /* FIX: better annotations */
void rpmtransClean(rpmTransactionSet ts)
{
    if (ts) {
	HFD_t hfd = headerFreeData;
	if (ts->sig != NULL)
	    ts->sig = hfd(ts->sig, ts->sigtype);
	if (ts->dig != NULL)
	    ts->dig = pgpFreeDig(ts->dig);
    }
}

rpmTransactionSet rpmtransFree(rpmTransactionSet ts)
{
    if (ts) {
	teIterator pi; transactionElement p;

	(void) rpmtsUnlink(ts, "tsCreate");

	/*@-usereleased@*/
	if (ts->nrefs > 0)
	    return NULL;

	pi = teInitIterator(ts);
	while ((p = teNext(pi, TR_ADDED)) != NULL) {
	    rpmRelocation * r;
	    if (p->relocs) {
		for (r = p->relocs; (r->oldPath || r->newPath); r++) {
		    r->oldPath = _free(r->oldPath);
		    r->newPath = _free(r->newPath);
		}
		p->relocs = _free(p->relocs);
	    }
	    /*@-type@*/ /* FIX: cast? */
	    if (p->fd != NULL)
	        p->fd = fdFree(p->fd, "alAddPackage (rpmtransFree)");
	    /*@=type@*/
	}
	pi = teFreeIterator(pi);

	ts->addedPackages = alFree(ts->addedPackages);
	ts->availablePackages = alFree(ts->availablePackages);
	ts->di = _free(ts->di);
	ts->removedPackages = _free(ts->removedPackages);
	ts->order = _free(ts->order);
	/*@-type@*/
	if (ts->scriptFd != NULL) {
	    ts->scriptFd =
		fdFree(ts->scriptFd, "rpmtransSetScriptFd (rpmtransFree");
	    ts->scriptFd = NULL;
	}
	/*@=type@*/
	ts->rootDir = _free(ts->rootDir);
	ts->currDir = _free(ts->currDir);

	rpmtransClean(ts);

	(void) rpmtsCloseDB(ts);

	/*@-refcounttrans@*/ ts = _free(ts); /*@=refcounttrans@*/
	/*@=usereleased@*/
    }
    return NULL;
}
/*@=nullstate@*/

/**
 * Check key for an unsatisfied dependency.
 * @todo Eliminate rpmrc provides.
 * @param al		available list
 * @param key		dependency
 * @return		0 if satisfied, 1 if not satisfied, 2 if error
 */
static int unsatisfiedDepend(rpmTransactionSet ts, rpmDepSet key)
	/*@globals _cacheDependsRC, fileSystem @*/
	/*@modifies ts, _cacheDependsRC, fileSystem @*/
{
    rpmdbMatchIterator mi;
    const char * Name;
    Header h;
    int rc;

    if ((Name = dsiGetN(key)) == NULL)
	return 0;	/* XXX can't happen */

    /*
     * Check if dbiOpen/dbiPut failed (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(ts->rpmdb, RPMDBI_DEPENDS, 0);
	if (dbi == NULL)
	    _cacheDependsRC = 0;
	else {
	    const char * DNEVR;

	    rc = -1;
	    if ((DNEVR = dsiGetDNEVR(key)) != NULL) {
		DBC * dbcursor = NULL;
		void * datap = NULL;
		size_t datalen = 0;
		size_t DNEVRlen = strlen(DNEVR);
		int xx;

		xx = dbiCopen(dbi, &dbcursor, 0);
		xx = dbiGet(dbi, dbcursor, (void **)&DNEVR, &DNEVRlen,
				&datap, &datalen, 0);
		if (xx == 0 && datap && datalen == 4)
		    memcpy(&rc, datap, datalen);
		xx = dbiCclose(dbi, dbcursor, 0);
	    }

	    if (rc >= 0) {
		dsiNotify(key, _("(cached)"), rc);
		return rc;
	    }
	}
    }

    rc = 0;	/* assume dependency is satisfied */

#if defined(DYING) || defined(__LCLINT__)
  { static /*@observer@*/ const char noProvidesString[] = "nada";
    static /*@observer@*/ const char * rcProvidesString = noProvidesString;
    int_32 Flags = dsiGetFlags(key);
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
		dsiNotify(key, _("(rpmrc provides)"), rc);
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
	if (rpmCheckRpmlibProvides(key)) {
	    dsiNotify(key, _("(rpmlib provides)"), rc);
	    goto exit;
	}
	goto unsatisfied;
    }

    /* Search added packages for the dependency. */
    if (alSatisfiesDepend(ts->addedPackages, key, NULL) != NULL)
	goto exit;

    /* XXX only the installer does not have the database open here. */
    if (ts->rpmdb != NULL) {
	if (Name[0] == '/') {
	    /* keyFlags better be 0! */

	    mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, Name, 0);

	    (void) rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);

	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		dsiNotify(key, _("(db files)"), rc);
		mi = rpmdbFreeIterator(mi);
		goto exit;
	    }
	    mi = rpmdbFreeIterator(mi);
	}

	mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, Name, 0);
	(void) rpmdbPruneIterator(mi,
			ts->removedPackages, ts->numRemovedPackages, 1);
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    if (rangeMatchesDepFlags(h, key)) {
		dsiNotify(key, _("(db provides)"), rc);
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
	    if (rangeMatchesDepFlags(h, key)) {
		dsiNotify(key, _("(db package)"), rc);
		mi = rpmdbFreeIterator(mi);
		goto exit;
	    }
	}
	mi = rpmdbFreeIterator(mi);
#endif

    }

unsatisfied:
    rc = 1;	/* dependency is unsatisfied */
    dsiNotify(key, NULL, rc);

exit:
    /*
     * If dbiOpen/dbiPut fails (e.g. permissions), we can't cache.
     */
    if (_cacheDependsRC) {
	dbiIndex dbi;
	dbi = dbiOpen(ts->rpmdb, RPMDBI_DEPENDS, 0);
	if (dbi == NULL) {
	    _cacheDependsRC = 0;
	} else {
	    const char * DNEVR;
	    int xx = 0;
	    if ((DNEVR = dsiGetDNEVR(key)) != NULL) {
		DBC * dbcursor = NULL;
		size_t DNEVRlen = strlen(DNEVR);
		xx = dbiCopen(dbi, &dbcursor, DBI_WRITECURSOR);
		xx = dbiPut(dbi, dbcursor, DNEVR, DNEVRlen,
			&rc, sizeof(rc), 0);
		xx = dbiCclose(dbi, dbcursor, DBI_WRITECURSOR);
	    }
	    if (xx)
		_cacheDependsRC = 0;
#if 0	/* XXX NOISY */
	    else
		rpmMessage(RPMMESS_DEBUG,
			_("%9s: (%s, %s) added to Depends cache.\n"),
			key->Type, (key->DNEVR != NULL ? key->DNEVR : "???"),
			(rc ? _("NO ") : _("YES")));
#endif
	}
    }
    return rc;
}

/**
 * Check header requires/conflicts against against installed+added packages.
 * @param ts		transaction set
 * @param psp		dependency problems
 * @param h		header to check
 * @param keyName	dependency name
 * @param multiLib	skip multilib colored dependencies?
 * @return		0 no problems found
 */
static int checkPackageDeps(rpmTransactionSet ts, problemsSet psp,
		Header h, const char * keyName, uint_32 multiLib)
	/*@globals fileSystem @*/
	/*@modifies ts, h, psp, fileSystem */
{
    const char * name, * version, * release;
    int scareMem = _DS_SCAREMEM;
    rpmDepSet requires;
    rpmDepSet conflicts;
    const char * Name;
    int_32 Flags;
    int rc, xx;
    int ourrc = 0;

    xx = headerNVR(h, &name, &version, &release);

    requires = dsiInit(dsNew(h, RPMTAG_REQUIRENAME, scareMem));
    if (requires != NULL)
    while (!ourrc && dsiNext(requires) >= 0) {

	if ((Name = dsiGetN(requires)) == NULL)
	    continue;	/* XXX can't happen */

	/* Filter out requires that came along for the ride. */
	if (keyName && strcmp(keyName, Name))
	    continue;

	Flags = dsiGetFlags(requires);

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
		suggestedKeys = alAllSatisfiesDepend(ts->availablePackages,
				requires, NULL);
	    }
	    /*@=branchstate@*/

	    dsProblem(psp, h, requires, suggestedKeys);

	}
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    /*@switchbreak@*/ break;
	}
    }
    requires = dsFree(requires);

    conflicts = dsiInit(dsNew(h, RPMTAG_CONFLICTNAME, scareMem));
    if (conflicts != NULL)
    while (!ourrc && dsiNext(conflicts) >= 0) {

	if ((Name = dsiGetN(conflicts)) == NULL)
	    continue;	/* XXX can't happen */

	/* Filter out conflicts that came along for the ride. */
	if (keyName != NULL && strcmp(keyName, Name))
	    continue;

	Flags = dsiGetFlags(conflicts);

	/* If this requirement comes from the core package only, not libraries,
	   then if we're installing the libraries only, don't count it in. */
	if (multiLib && !isDependsMULTILIB(Flags))
	    continue;

	rc = unsatisfiedDepend(ts, conflicts);

	/* 1 == unsatisfied, 0 == satsisfied */
	switch (rc) {
	case 0:		/* conflicts exist. */
	    /*@-mayaliasunique@*/ /* LCL: NULL may alias h ??? */
	    dsProblem(psp, h, conflicts, NULL);
	    /*@=mayaliasunique@*/
	    /*@switchbreak@*/ break;
	case 1:		/* conflicts don't exist. */
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    /*@switchbreak@*/ break;
	}
    }
    conflicts = dsFree(conflicts);

    return ourrc;
}

/**
 * Check dependency against installed packages.
 * Adding: check name/provides key against each conflict match,
 * Erasing: check name/provides/filename key against each requiredby match.
 * @param ts		transaction set
 * @param psp		dependency problems
 * @param key		dependency name
 * @param mi		rpm database iterator
 * @return		0 no problems found
 */
static int checkPackageSet(rpmTransactionSet ts, problemsSet psp,
		const char * key, /*@only@*/ /*@null@*/ rpmdbMatchIterator mi)
	/*@globals fileSystem @*/
	/*@modifies ts, mi, psp, fileSystem @*/
{
    Header h;
    int rc = 0;

    (void) rpmdbPruneIterator(mi,
		ts->removedPackages, ts->numRemovedPackages, 1);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	if (checkPackageDeps(ts, psp, h, key, 0)) {
	    rc = 1;
	    break;
	}
    }
    mi = rpmdbFreeIterator(mi);

    return rc;
}

/**
 * Erasing: check name/provides/filename key against requiredby matches.
 * @param ts		transaction set
 * @param psp		dependency problems
 * @param key		requires name
 * @return		0 no problems found
 */
static int checkDependentPackages(rpmTransactionSet ts,
			problemsSet psp, const char * key)
	/*@globals fileSystem @*/
	/*@modifies ts, psp, fileSystem @*/
{
    rpmdbMatchIterator mi;
    mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, key, 0);
    return checkPackageSet(ts, psp, key, mi);
}

/**
 * Adding: check name/provides key against conflicts matches.
 * @param ts		transaction set
 * @param psp		dependency problems
 * @param key		conflicts name
 * @return		0 no problems found
 */
static int checkDependentConflicts(rpmTransactionSet ts,
		problemsSet psp, const char * key)
	/*@globals fileSystem @*/
	/*@modifies ts, psp, fileSystem @*/
{
    int rc = 0;

    if (ts->rpmdb) {	/* XXX is this necessary? */
	rpmdbMatchIterator mi;
	mi = rpmtsInitIterator(ts, RPMTAG_CONFLICTNAME, key, 0);
	rc = checkPackageSet(ts, psp, key, mi);
    }

    return rc;
}

/*
 * XXX Hack to remove known Red Hat dependency loops, will be removed
 * as soon as rpm's legacy permits.
 */
#define	DEPENDENCY_WHITEOUT

#if defined(DEPENDENCY_WHITEOUT)
/*@observer@*/ /*@unchecked@*/
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

static int ignoreDep(const transactionElement p,
		const transactionElement q)
	/*@*/
{
    struct badDeps_s * bdp = badDeps;

    /*@-nullpass@*/ /* FIX: {p,q}->name may be NULL. */
    while (bdp->pname != NULL && bdp->qname != NULL) {
	if (!strcmp(p->name, bdp->pname) && !strcmp(q->name, bdp->qname))
	    return 1;
	bdp++;
    }
    /*@=nullpass@*/
    return 0;
}
#endif

/**
 * Recursively mark all nodes with their predecessors.
 * @param tsi		successor chain
 * @param q		predecessor
 */
static void markLoop(/*@special@*/ tsortInfo tsi, transactionElement q)
	/*@globals internalState @*/
	/*@uses tsi @*/
	/*@modifies internalState @*/
{
    transactionElement p;

    /*@-branchstate@*/ /* FIX: q is kept */
    while (tsi != NULL && (p = tsi->tsi_suc) != NULL) {
	tsi = tsi->tsi_next;
	if (p->tsi->tsi_chain != NULL)
	    continue;
	/*@-assignexpose -temptrans@*/
	p->tsi->tsi_chain = q;
	/*@=assignexpose =temptrans@*/
	if (p->tsi->tsi_next != NULL)
	    markLoop(p->tsi->tsi_next, p);
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
 * @param zap		max. no. of co-requisites to remove (-1 is all)?
 * @retval nzaps	address of no. of relations removed
 * @return		(possibly NULL) formatted "q <- p" releation (malloc'ed)
 */
/*@-mustmod@*/ /* FIX: hack modifies, but -type disables */
static /*@owned@*/ /*@null@*/ const char *
zapRelation(transactionElement q, transactionElement p,
		/*@null@*/ rpmDepSet requires,
		int zap, /*@in@*/ /*@out@*/ int * nzaps)
	/*@modifies q, p, *nzaps @*/
{
    tsortInfo tsi_prev;
    tsortInfo tsi;
    const char *dp = NULL;

    for (tsi_prev = q->tsi, tsi = q->tsi->tsi_next;
	 tsi != NULL;
	/* XXX Note: the loop traverses "not found", break on "found". */
	/*@-nullderef@*/
	 tsi_prev = tsi, tsi = tsi->tsi_next)
	/*@=nullderef@*/
    {
	int_32 Flags;

	if (tsi->tsi_suc != p)
	    continue;

	if (requires == NULL) continue;		/* XXX can't happen */

	/*@-type@*/ /* FIX: hack */
	requires->i = tsi->tsi_reqx;
	/*@=type@*/

	Flags = dsiGetFlags(requires);

	dp = dsDNEVR( identifyDepend(Flags), requires);

	/*
	 * Attempt to unravel a dependency loop by eliminating Requires's.
	 */
	/*@-branchstate@*/
	if (zap && !(Flags & RPMSENSE_PREREQ)) {
	    rpmMessage(RPMMESS_DEBUG,
			_("removing %s \"%s\" from tsort relations.\n"),
			(p->NEVR ?  p->NEVR : "???"), dp);
	    p->tsi->tsi_count--;
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

static void prtTSI(const char * msg, tsortInfo tsi)
	/*@globals fileSystem@*/
	/*@modifies fileSystem@*/
{
/*@-nullpass@*/
if (_te_debug) {
    if (msg) fprintf(stderr, "%s", msg);
    fprintf(stderr, " tsi %p suc %p next %p chain %p reqx %d qcnt %d\n", tsi, tsi->tsi_suc, tsi->tsi_next, tsi->tsi_chain, tsi->tsi_reqx, tsi->tsi_qcnt);
}
/*@=nullpass@*/
}

/**
 * Record next "q <- p" relation (i.e. "p" requires "q").
 * @param ts		transaction set
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param selected	boolean package selected array
 * @param j		relation index
 * @return		0 always
 */
static inline int addRelation(rpmTransactionSet ts,
		transactionElement p, unsigned char * selected,
		rpmDepSet requires)
	/*@globals fileSystem @*/
	/*@modifies ts, p, *selected, fileSystem @*/
{
    teIterator qi; transactionElement q;
    tsortInfo tsi;
    const char * Name;
    fnpyKey key;
    alKey pkgKey;
    int i = 0;

    if ((Name = dsiGetN(requires)) == NULL)
	return 0;	/* XXX can't happen */

    /* Avoid rpmlib feature dependencies. */
    if (!strncmp(Name, "rpmlib(", sizeof("rpmlib(")-1))
	return 0;

    pkgKey = RPMAL_NOMATCH;
    key = alSatisfiesDepend(ts->addedPackages, requires, &pkgKey);

if (_te_debug)
fprintf(stderr, "addRelation: pkgKey %ld\n", (long)pkgKey);

    /* Ordering depends only on added package relations. */
    if (pkgKey == RPMAL_NOMATCH)
	return 0;

/* XXX Set q to the added package that has pkgKey == q->u.addedKey */
/* XXX FIXME: bsearch is possible/needed here */
    qi = teInitIterator(ts);
    while ((q = teNext(qi, TR_ADDED)) != NULL) {
	if (pkgKey == q->u.addedKey)
	    break;
    }
    qi = teFreeIterator(qi);
    if (q == NULL)
	return 0;

#if defined(DEPENDENCY_WHITEOUT)
    /* Avoid certain dependency relations. */
    if (ignoreDep(p, q))
	return 0;
#endif

    i = q - ts->order;

/*@-nullpass -nullderef@*/
if (_te_debug)
fprintf(stderr, "addRelation: q %p(%s) from %p[%d:%d]\n", q, q->name, ts->order, i, ts->orderCount);
/*@=nullpass =nullderef@*/

    /* Avoid redundant relations. */
    /* XXX TODO: add control bit. */
    if (selected[i] != 0)
	return 0;
    selected[i] = 1;
/*@-nullpass@*/
if (_te_debug)
fprintf(stderr, "addRelation: selected[%d] = 1\n", i);
/*@=nullpass@*/

    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
    p->tsi->tsi_count++;			/* bump p predecessor count */
    if (p->depth <= q->depth)		/* Save max. depth in dependency tree */
	p->depth = q->depth + 1;
/*@-nullpass@*/
if (_te_debug)
fprintf(stderr, "addRelation: p %p(%s) depth %d", p, p->name, p->depth);
prtTSI(NULL, p->tsi);
/*@=nullpass@*/

    tsi = xcalloc(1, sizeof(*tsi));
    /*@-assignexpose@*/
    tsi->tsi_suc = p;
    /*@=assignexpose@*/

    /*@-type@*/
    tsi->tsi_reqx = requires->i;
    /*@=type@*/

    tsi->tsi_next = q->tsi->tsi_next;
/*@-nullpass -compmempass@*/
prtTSI("addRelation: new", tsi);
if (_te_debug)
fprintf(stderr, "addRelation: BEFORE q %p(%s)", q, q->name);
prtTSI(NULL, q->tsi);
/*@=nullpass =compmempass@*/
/*@-mods@*/
    q->tsi->tsi_next = tsi;
    q->tsi->tsi_qcnt++;			/* bump q successor count */
/*@=mods@*/
/*@-nullpass -compmempass@*/
if (_te_debug)
fprintf(stderr, "addRelation:  AFTER q %p(%s)", q, q->name);
prtTSI(NULL, q->tsi);
/*@=nullpass =compmempass@*/
    return 0;
}

/**
 * Compare ordered list entries by index (qsort/bsearch).
 * @param a		1st ordered list entry
 * @param b		2nd ordered list entry
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
 * Add element to list sorting by initial successor count.
 * @param p		new element
 * @retval qp		address of first element
 * @retval rp		address of last element
 */
static void addQ(transactionElement p,
		/*@in@*/ /*@out@*/ transactionElement * qp,
		/*@in@*/ /*@out@*/ transactionElement * rp)
	/*@modifies p, *qp, *rp @*/
{
    transactionElement q, qprev;

    if ((*rp) == NULL) {	/* 1st element */
	(*rp) = (*qp) = p;
	return;
    }
    for (qprev = NULL, q = (*qp); q != NULL; qprev = q, q = q->tsi->tsi_suc) {
	if (q->tsi->tsi_qcnt <= p->tsi->tsi_qcnt)
	    break;
    }
    /*@-assignexpose@*/
    if (qprev == NULL) {	/* insert at beginning of list */
	p->tsi->tsi_suc = q;
	(*qp) = p;		/* new head */
    } else if (q == NULL) {	/* insert at end of list */
	qprev->tsi->tsi_suc = p;
	(*rp) = p;		/* new tail */
    } else {			/* insert between qprev and q */
	p->tsi->tsi_suc = q;
	qprev->tsi->tsi_suc = p;
    }
    /*@=assignexpose@*/
}

int rpmdepOrder(rpmTransactionSet ts)
{
    int numAddedPackages = alGetSize(ts->addedPackages);
    int chainsaw = ts->transFlags & RPMTRANS_FLAG_CHAINSAW;
    teIterator pi; transactionElement p;
    teIterator qi; transactionElement q;
    teIterator ri; transactionElement r;
    tsortInfo tsi;
    tsortInfo tsi_next;
    alKey * ordering = alloca(sizeof(*ordering) * (numAddedPackages + 1));
    int orderingCount = 0;
    unsigned char * selected = alloca(sizeof(*selected) * (ts->orderCount + 1));
    int loopcheck;
    transactionElement newOrder;
    int newOrderCount = 0;
    orderListIndex orderList;
    int nrescans = 10;
    int _printed = 0;
    int qlen;
    int i, j;

assert(ts->numAddedPackages == alGetSize(ts->addedPackages));

    alMakeIndex(ts->addedPackages);

/*@-modfilesystem -nullpass@*/
if (_te_debug)
fprintf(stderr, "*** rpmdepOrder(%p) order %p[%d]\n", ts, ts->order, ts->orderCount);
/*@=modfilesystem =nullpass@*/

    /* T1. Initialize. */
    loopcheck = numAddedPackages;	/* XXX TR_ADDED only: should be ts->orderCount */
    pi = teInitIterator(ts);
    /* XXX Only added packages are ordered (for now). */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {

	p->tsi = xcalloc(1, sizeof(*p->tsi));

	/* Retrieve info from addedPackages. */
	p->NEVR = alGetNVR(ts->addedPackages, p->u.addedKey);
	p->name = alGetNVR(ts->addedPackages, p->u.addedKey);
/*@-nullpass@*/
	if ((p->release = strrchr(p->name, '-')) != NULL)
	    *p->release++ = '\0';
	if ((p->version = strrchr(p->name, '-')) != NULL)
	    *p->version++ = '\0';
/*@-modfilesystem@*/
prtTSI(p->NEVR, p->tsi);
/*@=modfilesystem@*/
/*@=nullpass@*/
    }
    pi = teFreeIterator(pi);

    /* Record all relations. */
    rpmMessage(RPMMESS_DEBUG, _("========== recording tsort relations\n"));
    pi = teInitIterator(ts);
    /* XXX Only added packages are ordered (for now). */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {
	rpmDepSet requires;
	int_32 Flags;

	requires = alGetRequires(ts->addedPackages, p->u.addedKey);

	if (requires == NULL)
	    continue;

	memset(selected, 0, sizeof(*selected) * ts->orderCount);

	/* Avoid narcisstic relations. */
	selected[teGetOc(pi)] = 1;

	/* T2. Next "q <- p" relation. */

	/* First, do pre-requisites. */
	requires = dsiInit(requires);
	if (requires != NULL)
	while (dsiNext(requires) >= 0) {

	    Flags = dsiGetFlags(requires);

	    /* Skip if not %pre/%post requires or legacy prereq. */

	    if (isErasePreReq(Flags) ||
		!( isInstallPreReq(Flags) ||
		   isLegacyPreReq(Flags) ))
		/*@innercontinue@*/ continue;

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, requires);

	}

	/* Then do co-requisites. */
	requires = dsiInit(requires);
	if (requires != NULL)
	while (dsiNext(requires) >= 0) {

	    Flags = dsiGetFlags(requires);

	    /* Skip if %pre/%post requires or legacy prereq. */

	    if (isErasePreReq(Flags) ||
		 ( isInstallPreReq(Flags) ||
		   isLegacyPreReq(Flags) ))
		/*@innercontinue@*/ continue;

	    /* T3. Record next "q <- p" relation (i.e. "p" requires "q"). */
	    (void) addRelation(ts, p, selected, requires);

	}
    }
    pi = teFreeIterator(pi);

    /* Save predecessor count. */
    pi = teInitIterator(ts);
    /* XXX Only added packages are ordered (for now). */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {

	p->npreds = p->tsi->tsi_count;
/*@-modfilesystem -nullpass@*/
if (_te_debug)
fprintf(stderr, "\t+++ %p[%d] %s npreds %d\n", p, teGetOc(pi), p->NEVR, p->npreds);
/*@=modfilesystem =nullpass@*/

    }
    pi = teFreeIterator(pi);

    /* T4. Scan for zeroes. */
    rpmMessage(RPMMESS_DEBUG, _("========== tsorting packages (order, #predecessors, #succesors, depth)\n"));

rescan:
    if (pi) pi = teFreeIterator(pi);
    q = r = NULL;
    qlen = 0;
    pi = teInitIterator(ts);
    /* XXX Only added packages are ordered (for now). */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {

	/* Prefer packages in chainsaw or presentation order. */
	if (!chainsaw)
	    p->tsi->tsi_qcnt = (ts->orderCount - teGetOc(pi));

	if (p->tsi->tsi_count != 0)
	    continue;
	p->tsi->tsi_suc = NULL;
	addQ(p, &q, &r);
	qlen++;
/*@-modfilesystem -nullpass@*/
if (_te_debug)
fprintf(stderr, "\t+++ addQ ++ qlen %d p %p(%s)", qlen, p, p->NEVR);
prtTSI(" p", p->tsi);
/*@=modfilesystem =nullpass@*/
    }
    pi = teFreeIterator(pi);

    /* T5. Output front of queue (T7. Remove from queue.) */
    /*@-branchstate@*/ /* FIX: r->tsi->tsi_next released */
    for (; q != NULL; q = q->tsi->tsi_suc) {

	/* XXX Only added packages are ordered (for now). */
	switch (q->type) {
	case TR_ADDED:
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}

	rpmMessage(RPMMESS_DEBUG, "%5d%5d%5d%3d %*s %s\n",
			orderingCount, q->npreds, q->tsi->tsi_qcnt, q->depth,
			(2 * q->depth), "",
			(q->NEVR ? q->NEVR : "???"));
	ordering[orderingCount] = q->u.addedKey;
	orderingCount++;
	qlen--;
	loopcheck--;

	/* T6. Erase relations. */
	tsi_next = q->tsi->tsi_next;
	q->tsi->tsi_next = NULL;
	while ((tsi = tsi_next) != NULL) {
	    tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    p = tsi->tsi_suc;
	    if (p && (--p->tsi->tsi_count) <= 0) {
		/* XXX TODO: add control bit. */
		p->tsi->tsi_suc = NULL;
		/*@-nullstate@*/	/* FIX: q->tsi->tsi_u.suc may be NULL */
		addQ(p, &q->tsi->tsi_suc, &r);
		/*@=nullstate@*/
		qlen++;
/*@-modfilesystem -nullpass@*/
if (_te_debug)
fprintf(stderr, "\t+++ addQ ++ qlen %d p %p(%s)", qlen, p, p->NEVR);
prtTSI(" p", p->tsi);
/*@=modfilesystem =nullpass@*/
	    }
	    tsi = _free(tsi);
	}
	if (!_printed && loopcheck == qlen && q->tsi->tsi_suc != NULL) {
	    _printed++;
	    rpmMessage(RPMMESS_DEBUG,
		_("========== successors only (presentation order)\n"));
	}
    }
    /*@=branchstate@*/

    /* T8. End of process. Check for loops. */
    if (loopcheck != 0) {
	int nzaps;

	/* T9. Initialize predecessor chain. */
	nzaps = 0;
	qi = teInitIterator(ts);
	/* XXX Only added packages are ordered (for now). */
	while ((q = teNext(qi, TR_ADDED)) != NULL) {
	    q->tsi->tsi_chain = NULL;
	    q->tsi->tsi_reqx = 0;
	    /* Mark packages already sorted. */
	    if (q->tsi->tsi_count == 0)
		q->tsi->tsi_count = -1;
	}
	qi = teFreeIterator(qi);

	/* T10. Mark all packages with their predecessors. */
	qi = teInitIterator(ts);
	/* XXX Only added packages are ordered (for now). */
	while ((q = teNext(qi, TR_ADDED)) != NULL) {
	    if ((tsi = q->tsi->tsi_next) == NULL)
		continue;
	    q->tsi->tsi_next = NULL;
	    markLoop(tsi, q);
	    q->tsi->tsi_next = tsi;
	}
	qi = teFreeIterator(qi);

	/* T11. Print all dependency loops. */
	ri = teInitIterator(ts);
	/* XXX Only added packages are ordered (for now). */
	while ((r = teNext(ri, TR_ADDED)) != NULL)
	/*@-branchstate@*/
	{
	    int printed;

	    printed = 0;

	    /* T12. Mark predecessor chain, looking for start of loop. */
	    for (q = r->tsi->tsi_chain; q != NULL; q = q->tsi->tsi_chain) {
		if (q->tsi->tsi_reqx)
		    /*@innerbreak@*/ break;
		q->tsi->tsi_reqx = 1;
	    }

	    /* T13. Print predecessor chain from start of loop. */
	    while ((p = q) != NULL && (q = p->tsi->tsi_chain) != NULL) {
		rpmDepSet requires;
		const char * dp;
		char buf[4096];

		/* Unchain predecessor loop. */
		p->tsi->tsi_chain = NULL;

		if (!printed) {
		    rpmMessage(RPMMESS_DEBUG, _("LOOP:\n"));
		    printed = 1;
		}

		/* Find (and destroy if co-requisite) "q <- p" relation. */
		requires = alGetRequires(ts->addedPackages, p->u.addedKey);
		requires = dsiInit(requires);
		dp = zapRelation(q, p, requires, 1, &nzaps);

		/* Print next member of loop. */
		buf[0] = '\0';
		if (p->NEVR != NULL)
		(void) stpcpy(buf, p->NEVR);
		rpmMessage(RPMMESS_DEBUG, "    %-40s %s\n", buf,
			(dp ? dp : "not found!?!"));

		dp = _free(dp);
	    }

	    /* Walk (and erase) linear part of predecessor chain as well. */
	    for (p = r, q = r->tsi->tsi_chain;
		 q != NULL;
		 p = q, q = q->tsi->tsi_chain)
	    {
		/* Unchain linear part of predecessor loop. */
		p->tsi->tsi_chain = NULL;
		p->tsi->tsi_reqx = 0;
	    }
	}
	/*@=branchstate@*/
	ri = teFreeIterator(ri);

	/* If a relation was eliminated, then continue sorting. */
	/* XXX TODO: add control bit. */
	if (nzaps && nrescans-- > 0) {
	    rpmMessage(RPMMESS_DEBUG, _("========== continuing tsort ...\n"));
	    goto rescan;
	}

	/* Return no. of packages that could not be ordered. */
	rpmMessage(RPMMESS_ERROR, _("rpmdepOrder failed, %d elements remain\n"),
			loopcheck);
	return loopcheck;
    }

    /*
     * The order ends up as installed packages followed by removed packages,
     * with removes for upgrades immediately following the installation of
     * the new package. This would be easier if we could sort the
     * addedPackages array, but we store indexes into it in various places.
     */
    orderList = xcalloc(numAddedPackages, sizeof(*orderList));
    j = 0;
    pi = teInitIterator(ts);
    /* XXX Only added packages are ordered (for now). */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {

	/* Clean up tsort remnants (if any). */
	while ((tsi = p->tsi->tsi_next) != NULL) {
	    p->tsi->tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    tsi = _free(tsi);
	}
	p->tsi = _free(p->tsi);
	p->NEVR = _free(p->NEVR);
	p->name = _free(p->name);

	/* Prepare added package ordering permutation. */
	orderList[j].pkgKey = p->u.addedKey;
	orderList[j].orIndex = teGetOc(pi);
	j++;
    }
    pi = teFreeIterator(pi);
    assert(j <= numAddedPackages);

    qsort(orderList, numAddedPackages, sizeof(*orderList), orderListIndexCmp);

    newOrder = xcalloc(ts->orderCount, sizeof(*newOrder));
    /*@-branchstate@*/
    for (i = 0, newOrderCount = 0; i < orderingCount; i++)
    {
	struct orderListIndex_s key;
	orderListIndex needle;

	key.pkgKey = ordering[i];
	needle = bsearch(&key, orderList, numAddedPackages,
				sizeof(key), orderListIndexCmp);
	/* bsearch should never, ever fail */
	if (needle == NULL) continue;

	j = needle->orIndex;
	/*@-assignexpose@*/
	q = ts->order + j;
/*@i@*/	newOrder[newOrderCount++] = *q;		/* structure assignment */
	/*@=assignexpose@*/
	for (j = needle->orIndex + 1; j < ts->orderCount; j++) {
	    q = ts->order + j;
	    if (q->type == TR_REMOVED &&
		q->u.removed.dependsOnKey == needle->pkgKey) {
		/*@-assignexpose@*/
/*@i@*/		newOrder[newOrderCount++] = *q;	/* structure assignment */
		/*@=assignexpose@*/
	    } else
		/*@innerbreak@*/ break;
	}
    }
    /*@=branchstate@*/

    /*@-compmempass -usereleased@*/ /* FIX: ts->order[].{NEVR,name} released */
    pi = teInitIterator(ts);
    /*@=compmempass =usereleased@*/
    while ((p = teNext(pi, TR_REMOVED)) != NULL) {
	if (p->u.removed.dependsOnKey == RPMAL_NOMATCH) {
	    /*@-assignexpose@*/
/*@i@*/	    newOrder[newOrderCount] = *p;	/* structure assignment */
	    /*@=assignexpose@*/
	    newOrderCount++;
	}
    }
    pi = teFreeIterator(pi);
    assert(newOrderCount == ts->orderCount);

    ts->order = _free(ts->order);
    ts->order = newOrder;
    ts->orderAlloced = ts->orderCount;
    orderList = _free(orderList);

    return 0;
}

/**
 * Close a single database index.
 * @param db		rpm database
 * @param rpmtag	rpm tag
 * @return              0 on success
 */
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
	if (db->_dbi[dbix] != NULL) {
	    int xx;
	    /*@-unqualifiedtrans@*/		/* FIX: double indirection. */
	    xx = dbiClose(db->_dbi[dbix], 0);
	    if (xx && rc == 0) rc = xx;
	    db->_dbi[dbix] = NULL;
	    /*@=unqualifiedtrans@*/
	}
	break;
    }
    return rc;
}

int rpmdepCheck(rpmTransactionSet ts,
		rpmDependencyConflict * conflicts, int * numConflicts)
{
    int scareMem = _DS_SCAREMEM;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmdbMatchIterator mi = NULL;
    Header h = NULL;
    problemsSet ps = NULL;
    teIterator pi = NULL; transactionElement p;
    int closeatexit = 0;
    int j, xx;
    int rc;

    /* Do lazy, readonly, open of rpm database. */
    if (ts->rpmdb == NULL) {
	if ((rc = rpmtsOpenDB(ts, ts->dbmode)) != 0)
	    goto exit;
	closeatexit = 1;
    }

    ps = xcalloc(1, sizeof(*ps));
    ps->alloced = 5;
    ps->num = 0;
    ps->problems = xcalloc(ps->alloced, sizeof(*ps->problems));

    *conflicts = NULL;
    *numConflicts = 0;

    alMakeIndex(ts->addedPackages);
    alMakeIndex(ts->availablePackages);

    /*
     * Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    pi = teInitIterator(ts);
    /* XXX Only added packages are checked (for now). */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {
	char * pkgNVR = NULL, * n, * v, * r;
	rpmDepSet provides;
	uint_32 multiLib;

	/* XXX Only added packages are checked (for now). */
	switch (p->type) {
	case TR_ADDED:
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}

	h = alGetHeader(ts->addedPackages, p->u.addedKey, 0);
	if (h == NULL)		/* XXX can't happen */
	    break;

	pkgNVR = _free(pkgNVR);
	pkgNVR = hGetNVR(h, NULL);
	multiLib = p->multiLib;

        rpmMessage(RPMMESS_DEBUG,  "========== +++ %s\n" , pkgNVR);
	rc = checkPackageDeps(ts, ps, h, NULL, multiLib);
	h = headerFree(h, "alGetHeader");

	if (rc) {
	    pkgNVR = _free(pkgNVR);
	    goto exit;
	}

	/* Adding: check name against conflicts matches. */

	if ((r = strrchr(pkgNVR, '-')) != NULL)
	    *r++ = '\0';
	if ((v = strrchr(pkgNVR, '-')) != NULL)
	    *v++ = '\0';
	n = pkgNVR;

	rc = checkDependentConflicts(ts, ps, n);
	pkgNVR = _free(pkgNVR);
	if (rc)
	    goto exit;

	provides = alGetProvides(ts->addedPackages, p->u.addedKey);

	rc = 0;
	provides = dsiInit(provides);
	if (provides == NULL || dsiGetN(provides) == NULL)
	    continue;
	while (dsiNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = dsiGetN(provides)) == NULL)
		/*@innercontinue@*/ continue;	/* XXX can't happen */

	    /* Adding: check provides key against conflicts matches. */
	    if (!checkDependentConflicts(ts, ps, Name))
		/*@innercontinue@*/ continue;
	    rc = 1;
	    /*@innerbreak@*/ break;
	}
	if (rc)
	    goto exit;
    }
    pi = teFreeIterator(pi);

    /*
     * Look at the removed packages and make sure they aren't critical.
     */
    /*@-branchstate@*/
    if (ts->numRemovedPackages > 0) {
      rpmDepSet provides;

      mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
      xx = rpmdbAppendIterator(mi,
			ts->removedPackages, ts->numRemovedPackages);
      while ((h = rpmdbNextIterator(mi)) != NULL) {

	{   const char * name, * version, * release;

	    xx = headerNVR(h, &name, &version, &release);
	    rpmMessage(RPMMESS_DEBUG,  "========== --- %s-%s-%s\n" ,
		name, version, release);

	    /* Erasing: check name against requiredby matches. */
	    rc = checkDependentPackages(ts, ps, name);
	    if (rc)
		goto exit;
	}

	    rc = 0;
	    provides = dsiInit(dsNew(h, RPMTAG_PROVIDENAME, scareMem));
	    if (provides != NULL)
	    while (dsiNext(provides) >= 0) {
		const char * Name;

		if ((Name = dsiGetN(provides)) == NULL)
		    /*@innercontinue@*/ continue;	/* XXX can't happen */

		/* Erasing: check provides against requiredby matches. */
		if (!checkDependentPackages(ts, ps, Name))
		    /*@innercontinue@*/ continue;
		rc = 1;
		/*@innerbreak@*/ break;
	    }
	    provides = dsFree(provides);
	    if (rc)
		goto exit;

	{   const char ** baseNames, ** dirNames;
	    int_32 * dirIndexes;
	    rpmTagType dnt, bnt;
	    int fileCount;
	    char * fileName = NULL;
	    int fileAlloced = 0;
	    int len;

	    if (hge(h, RPMTAG_BASENAMES, &bnt, (void **) &baseNames, &fileCount))
	    {
		xx = hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL);
		xx = hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes,
				NULL);
		rc = 0;
		for (j = 0; j < fileCount; j++) {
		    len = strlen(baseNames[j]) + 1 +
			  strlen(dirNames[dirIndexes[j]]);
		    if (len > fileAlloced) {
			fileAlloced = len * 2;
			fileName = xrealloc(fileName, fileAlloced);
		    }
		    *fileName = '\0';
		    (void) stpcpy( stpcpy(fileName, dirNames[dirIndexes[j]]) , baseNames[j]);
		    /* Erasing: check filename against requiredby matches. */
		    if (!checkDependentPackages(ts, ps, fileName))
			/*@innercontinue@*/ continue;
		    rc = 1;
		    /*@innerbreak@*/ break;
		}

		fileName = _free(fileName);
		baseNames = hfd(baseNames, bnt);
		dirNames = hfd(dirNames, dnt);
		if (rc)
		    goto exit;
	    }
	}

      }
      mi = rpmdbFreeIterator(mi);
    }
    /*@=branchstate@*/

    if (ps->num) {
	*conflicts = ps->problems;
	ps->problems = NULL;
	*numConflicts = ps->num;
    }
    rc = 0;

exit:
    mi = rpmdbFreeIterator(mi);
    pi = teFreeIterator(pi);
    if (ps) {
	ps->problems = _free(ps->problems);
	ps = _free(ps);
    }
    /*@-branchstate@*/
    if (closeatexit)
	xx = rpmtsCloseDB(ts);
    else if (_cacheDependsRC)
	xx = rpmdbCloseDBI(ts->rpmdb, RPMDBI_DEPENDS);
    /*@=branchstate@*/
    return rc;
}
