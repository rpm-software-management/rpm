/** \ingroup rpmdep
 * \file lib/depends.c
 */

#define	_DS_SCAREMEM	0	/* XXX remove? */

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>		/* XXX rpmtsOpenDB() needs rpmGetPath */
#include <rpmpgp.h>		/* XXX rpmtransFree() needs pgpFreeDig */

#define _NEED_TEITERATOR	1
#include "depends.h"

#include "rpmdb.h"		/* XXX response cache needs dbiOpen et al. */

#include "debug.h"

/*@access tsortInfo @*/
/*@access rpmTransactionSet @*/

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

/*@-type -mustmod@*/	/* FIX: transactionElement not opaque */
/**
 */
static void delTE(transactionElement p)
	/*@modifies p @*/
{
    rpmRelocation * r;

    if (p->relocs) {
	for (r = p->relocs; (r->oldPath || r->newPath); r++) {
	    r->oldPath = _free(r->oldPath);
	    r->newPath = _free(r->newPath);
	}
	p->relocs = _free(p->relocs);
    }

    p->this = dsFree(p->this);
    p->provides = dsFree(p->provides);
    p->requires = dsFree(p->requires);
    p->conflicts = dsFree(p->conflicts);
    p->obsoletes = dsFree(p->obsoletes);
    p->fi = fiFree(p->fi, 1);

    /*@-noeffectuncon@*/
    if (p->fd != NULL)
        p->fd = fdFree(p->fd, "delTE");
    /*@=noeffectuncon@*/

    p->os = _free(p->os);
    p->arch = _free(p->arch);
    p->epoch = _free(p->epoch);
    p->name = _free(p->name);
    p->NEVR = _free(p->NEVR);

    p->h = headerFree(p->h, "delTE");

    /*@-abstract@*/
    memset(p, 0, sizeof(*p));	/* XXX trash and burn */
    /*@=abstract@*/
    /*@-nullstate@*/ /* FIX: p->{NEVR,name} annotations */
    return;
    /*@=nullstate@*/
}

/**
 */
static void addTE(rpmTransactionSet ts, transactionElement p, Header h,
		/*@dependent@*/ /*@null@*/ fnpyKey key,
		/*@null@*/ rpmRelocation * relocs)
	/*@modifies ts, p, h @*/
{
    int scareMem = _DS_SCAREMEM;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    int_32 * ep;
    const char * arch, * os;
    int xx;

    p->NEVR = hGetNEVR(h, NULL);
    p->name = xstrdup(p->NEVR);
    if ((p->release = strrchr(p->name, '-')) != NULL)
	*p->release++ = '\0';
    if ((p->version = strrchr(p->name, '-')) != NULL)
	*p->version++ = '\0';

    arch = NULL;
    xx = hge(h, RPMTAG_ARCH, NULL, (void **)&arch, NULL);
    p->arch = (arch != NULL ? xstrdup(arch) : NULL);
    os = NULL;
    xx = hge(h, RPMTAG_OS, NULL, (void **)&os, NULL);
    p->os = (os != NULL ? xstrdup(os) : NULL);

    ep = NULL;
    xx = hge(h, RPMTAG_EPOCH, NULL, (void **)&ep, NULL);
    /*@-branchstate@*/
    if (ep) {
	p->epoch = xmalloc(20);
	sprintf(p->epoch, "%d", *ep);
    } else
	p->epoch = NULL;
    /*@=branchstate@*/

    p->this = dsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
    p->provides = dsNew(h, RPMTAG_PROVIDENAME, scareMem);
    p->fi = fiNew(ts, NULL, h, RPMTAG_BASENAMES, scareMem);
    p->requires = dsNew(h, RPMTAG_REQUIRENAME, scareMem);
    p->conflicts = dsNew(h, RPMTAG_CONFLICTNAME, scareMem);
    p->obsoletes = dsNew(h, RPMTAG_OBSOLETENAME, scareMem);

    p->key = key;

    p->fd = NULL;

    if (relocs != NULL) {
	rpmRelocation * r;
	int i;

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
}
/*@=type =mustmod@*/

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
    ts->addedPackages = alCreate(ts->delta);

    ts->numAvailablePackages = 0;
    ts->availablePackages = alCreate(ts->delta);

    ts->orderAlloced = ts->delta;
    ts->orderCount = 0;
/*@-type -abstract@*/
    ts->order = xcalloc(ts->orderAlloced, sizeof(*ts->order));
/*@=type =abstract@*/

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
 * @param h		header
 * @param dboffset	rpm database instance
 * @param depends	installed package of pair (or RPMAL_NOMATCH on erase)
 * @return		0 on success
 */
static int removePackage(rpmTransactionSet ts, Header h, int dboffset,
		alKey depends)
	/*@modifies ts, h @*/
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

/*@-type -abstract@*/
    if (ts->orderCount >= ts->orderAlloced) {
	ts->orderAlloced += (ts->orderCount - ts->orderAlloced) + ts->delta;
	ts->order = xrealloc(ts->order, sizeof(*ts->order) * ts->orderAlloced);
    }

    p = ts->order + ts->orderCount;
    ts->orderCount++;

    memset(p, 0, sizeof(*p));
/*@=type =abstract@*/

    /* XXX FIXME: what should a TR_REMOVED key be ??? */
    addTE(ts, p, h, NULL, NULL);

/*@-type@*/ /* FIX: transactionElement not opaque */
    p->type = TR_REMOVED;
    p->u.removed.dboffset = dboffset;
    /*@-assignexpose -temptrans@*/
    p->u.removed.dependsOnKey = depends;
    /*@=assignexpose =temptrans@*/
/*@=type@*/

    return 0;
}

int rpmtransAddPackage(rpmTransactionSet ts, Header h,
			fnpyKey key, int upgrade, rpmRelocation * relocs)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    int isSource;
    int duplicate = 0;
    teIterator pi; transactionElement p;
    rpmDepSet add;
    rpmDepSet obsoletes;
    const char * name;
    alKey pkgKey;	/* addedPackages key */
    int xx;
    int ec = 0;
    int rc;
    int oc;

    /*
     * Check for previously added versions with the same name.
     * FIXME: only catches previously added, older packages.
     */
    add = dsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL|RPMSENSE_LESS));
    pkgKey = RPMAL_NOMATCH;
    pi = teInitIterator(ts);
    /* XXX Only added packages need be checked for dupes. */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {
/*@-type@*/
	rc = dsCompare(add, p->this);
/*@=type@*/
	if (rc != 0) {
/*@-type@*/
	    const char * pkgNEVR = dsiGetDNEVR(p->this);
/*@=type@*/
	    const char * addNEVR = dsiGetDNEVR(add);
	    rpmMessage(RPMMESS_WARNING,
		_("package %s was already added, replacing with %s\n"),
		(pkgNEVR ? pkgNEVR + 2 : "?pkgNEVR?"),
		(addNEVR ? addNEVR + 2 : "?addNEVR?"));
	    duplicate = 1;
	    pkgKey = teGetAddedKey(p);
	    break;
	}
    }
    pi = teFreeIterator(pi);
    add = dsFree(add);
/*@-abstract@*/
    oc = (p != NULL ? (p - ts->order) : ts->orderCount);
/*@=abstract@*/

    isSource = headerIsEntry(h, RPMTAG_SOURCEPACKAGE);

    if (p != NULL && duplicate && oc < ts->orderCount) {
    /* XXX FIXME removed transaction element side effects need to be weeded */
	delTE(p);
    }

/*@-type@*/ /* FIX: transactionElement not opaque */
/*@-abstract@*/
    if (oc >= ts->orderAlloced) {
	ts->orderAlloced += (oc - ts->orderAlloced) + ts->delta;
	ts->order = xrealloc(ts->order, ts->orderAlloced * sizeof(*ts->order));
    }

    p = ts->order + oc;
    memset(p, 0, sizeof(*p));		/* XXX trash and burn */
/*@=abstract@*/
    
    addTE(ts, p, h, key, relocs);

    p->type = TR_ADDED;
    pkgKey = alAddPackage(ts->addedPackages, pkgKey, p->key,
			p->provides, p->fi);
    if (pkgKey == RPMAL_NOMATCH) {
	ec = 1;
	goto exit;
    }
    p->u.addedKey = pkgKey;
    p->multiLib = 0;
/*@=type@*/

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

    if (!duplicate) {
	ts->numAddedPackages++;
	ts->orderCount++;
    }

    if (!upgrade)
	goto exit;

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
    if (isSource)
	goto exit;

    /* Do lazy (readonly?) open of rpm database. */
    if (ts->rpmdb == NULL) {
	if ((ec = rpmtsOpenDB(ts, ts->dbmode)) != 0)
	    goto exit;
    }

    /* XXX WARNING: nothing below can access *p as t->order may be realloc'd */
    name = teGetN(p);

    {	rpmdbMatchIterator mi;
	Header h2;

	mi = rpmtsInitIterator(ts, RPMTAG_NAME, name, 0);
	while((h2 = rpmdbNextIterator(mi)) != NULL) {
	    /*@-branchstate@*/
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
/*@-abstract@*/
		    p = ts->order + oc;
/*@=abstract@*/
/*@-type@*/ /* FIX: transactionElement not opaque */
		    p->multiLib = multiLibMask;
/*@=type@*/
		}
	    }
	    /*@=branchstate@*/
	}
	mi = rpmdbFreeIterator(mi);
    }

/*@-abstract@*/
    p = ts->order + oc;
/*@=abstract@*/
/*@-type@*/ /* FIX: transactionElement not opaque */
    obsoletes = dsiInit(rpmdsLink(p->obsoletes, "Obsoletes"));
/*@=type@*/
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
		    xx = removePackage(ts, h2, rpmdbGetIteratorOffset(mi), pkgKey);
		/*@=branchstate@*/
	    }
	    mi = rpmdbFreeIterator(mi);
	}
    }
    obsoletes = dsFree(obsoletes);

    ec = 0;

exit:
    pi = teFreeIterator(pi);
    return ec;
}

void rpmtransAvailablePackage(rpmTransactionSet ts, Header h, fnpyKey key)
{
    int scareMem = _DS_SCAREMEM;
    rpmDepSet provides = dsNew(h, RPMTAG_PROVIDENAME, scareMem);
    TFI_t fi = fiNew(ts, NULL, h, RPMTAG_BASENAMES, scareMem);

    /* XXX FIXME: return code RPMAL_NOMATCH is error */
    (void) alAddPackage(ts->availablePackages, RPMAL_NOMATCH, key,
		provides, fi);
    fi = fiFree(fi, 1);
    provides = dsFree(provides);
}

int rpmtransRemovePackage(rpmTransactionSet ts, Header h, int dboffset)
{
    return removePackage(ts, h, dboffset, RPMAL_NOMATCH);
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
	while ((p = teNextIterator(pi)) != NULL) {
	    delTE(p);
	}
	pi = teFreeIterator(pi);

	ts->addedPackages = alFree(ts->addedPackages);
	ts->availablePackages = alFree(ts->availablePackages);
	ts->di = _free(ts->di);
	ts->removedPackages = _free(ts->removedPackages);
/*@-type@*/ /* FIX: transactionElement not opaque */
	ts->order = _free(ts->order);
/*@=type@*/
	if (ts->scriptFd != NULL) {
	    ts->scriptFd =
		fdFree(ts->scriptFd, "rpmtransSetScriptFd (rpmtransFree");
	    ts->scriptFd = NULL;
	}
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
 * @param pkgNEVR	package name-version-release
 * @param requires	Requires: dependencies (or NULL)
 * @param conflicts	Conflicts: dependencies (or NULL)
 * @param keyName	dependency name to filter (or NULL)
 * @param multiLib	skip multilib colored dependencies?
 * @return		0 no problems found
 */
static int checkPackageDeps(rpmTransactionSet ts, const char * pkgNEVR,
		/*@null@*/ rpmDepSet requires, /*@null@*/ rpmDepSet conflicts,
		/*@null@*/ const char * keyName, uint_32 multiLib)
	/*@globals fileSystem @*/
	/*@modifies ts, requires, conflicts, fileSystem */
{
    const char * Name;
    int_32 Flags;
    int rc;
    int ourrc = 0;

    requires = dsiInit(requires);
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

	    dsProblem(ts->probs, pkgNEVR, requires, suggestedKeys);

	}
	    /*@switchbreak@*/ break;
	case 2:		/* something went wrong! */
	default:
	    ourrc = 1;
	    /*@switchbreak@*/ break;
	}
    }

    conflicts = dsiInit(conflicts);
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
	    dsProblem(ts->probs, pkgNEVR, conflicts, NULL);
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

    return ourrc;
}

/**
 * Check dependency against installed packages.
 * Adding: check name/provides key against each conflict match,
 * Erasing: check name/provides/filename key against each requiredby match.
 * @param ts		transaction set
 * @param key		dependency name
 * @param mi		rpm database iterator
 * @return		0 no problems found
 */
static int checkPackageSet(rpmTransactionSet ts,
		const char * key, /*@only@*/ /*@null@*/ rpmdbMatchIterator mi)
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
	rpmDepSet requires, conflicts;
	int rc;

	pkgNEVR = hGetNEVR(h, NULL);
	requires = dsNew(h, RPMTAG_REQUIRENAME, scareMem);
	conflicts = dsNew(h, RPMTAG_CONFLICTNAME, scareMem);
	rc = checkPackageDeps(ts, pkgNEVR, requires, conflicts, key, 0);
	conflicts = dsFree(conflicts);
	requires = dsFree(requires);
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
 * Erasing: check name/provides/filename key against requiredby matches.
 * @param ts		transaction set
 * @param key		requires name
 * @return		0 no problems found
 */
static int checkDependentPackages(rpmTransactionSet ts, const char * key)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/
{
    rpmdbMatchIterator mi;
    mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, key, 0);
    return checkPackageSet(ts, key, mi);
}

/**
 * Adding: check name/provides key against conflicts matches.
 * @param ts		transaction set
 * @param key		conflicts name
 * @return		0 no problems found
 */
static int checkDependentConflicts(rpmTransactionSet ts, const char * key)
	/*@globals fileSystem @*/
	/*@modifies ts, fileSystem @*/
{
    int rc = 0;

    if (ts->rpmdb != NULL) {	/* XXX is this necessary? */
	rpmdbMatchIterator mi;
	mi = rpmtsInitIterator(ts, RPMTAG_CONFLICTNAME, key, 0);
	rc = checkPackageSet(ts, key, mi);
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
	if (!strcmp(teGetN(p), bdp->pname) && !strcmp(teGetN(q), bdp->qname))
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
	if (teGetTSI(p)->tsi_chain != NULL)
	    continue;
	/*@-assignexpose -temptrans@*/
	teGetTSI(p)->tsi_chain = q;
	/*@=assignexpose =temptrans@*/
	if (teGetTSI(p)->tsi_next != NULL)
	    markLoop(teGetTSI(p)->tsi_next, p);
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
	/*@modifies q, p, requires, *nzaps @*/
{
    tsortInfo tsi_prev;
    tsortInfo tsi;
    const char *dp = NULL;

    for (tsi_prev = teGetTSI(q), tsi = teGetTSI(q)->tsi_next;
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

	(void) dsiSetIx(requires, tsi->tsi_reqx);

	Flags = dsiGetFlags(requires);

	dp = dsDNEVR( identifyDepend(Flags), requires);

	/*
	 * Attempt to unravel a dependency loop by eliminating Requires's.
	 */
	/*@-branchstate@*/
	if (zap && !(Flags & RPMSENSE_PREREQ)) {
	    rpmMessage(RPMMESS_DEBUG,
			_("removing %s \"%s\" from tsort relations.\n"),
			(teGetNEVR(p) ?  teGetNEVR(p) : "???"), dp);
	    teGetTSI(p)->tsi_count--;
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
/*@-nullpass -type -abstract@*/
if (_te_debug) {
    if (msg) fprintf(stderr, "%s", msg);
    fprintf(stderr, " tsi %p suc %p next %p chain %p reqx %d qcnt %d\n", tsi, tsi->tsi_suc, tsi->tsi_next, tsi->tsi_chain, tsi->tsi_reqx, tsi->tsi_qcnt);
}
/*@=nullpass =type =abstract@*/
}

/**
 * Record next "q <- p" relation (i.e. "p" requires "q").
 * @param ts		transaction set
 * @param p		predecessor (i.e. package that "Requires: q")
 * @param selected	boolean package selected array
 * @param j		relation index
 * @return		0 always
 */
/*@-mustmod@*/
static inline int addRelation(rpmTransactionSet ts,
		/*@dependent@*/ transactionElement p,
		unsigned char * selected,
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
	if (pkgKey == teGetAddedKey(q))
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

/*@-type -abstract@*/
    i = q - ts->order;
/*@=type =abstract@*/

/*@-nullpass -nullderef -type -abstract@*/
if (_te_debug)
fprintf(stderr, "addRelation: q %p(%s) from %p[%d:%d]\n", q, teGetN(q), ts->order, i, ts->orderCount);
/*@=nullpass =nullderef =type =abstract@*/

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
    teGetTSI(p)->tsi_count++;			/* bump p predecessor count */
/*@-type@*/
    if (p->depth <= q->depth)		/* Save max. depth in dependency tree */
	p->depth = q->depth + 1;
/*@=type@*/
/*@-nullpass -type -abstract@*/
if (_te_debug)
fprintf(stderr, "addRelation: p %p(%s) depth %d", p, teGetN(p), p->depth);
prtTSI(NULL, teGetTSI(p));
/*@=nullpass =type =abstract@*/

    tsi = xcalloc(1, sizeof(*tsi));
    tsi->tsi_suc = p;

    tsi->tsi_reqx = dsiGetIx(requires);

    tsi->tsi_next = teGetTSI(q)->tsi_next;
/*@-nullpass -compmempass -type -abstract@*/
prtTSI("addRelation: new", tsi);
if (_te_debug)
fprintf(stderr, "addRelation: BEFORE q %p(%s)", q, teGetN(q));
prtTSI(NULL, teGetTSI(q));
/*@=nullpass =compmempass =type =abstract@*/
/*@-mods@*/
    teGetTSI(q)->tsi_next = tsi;
    teGetTSI(q)->tsi_qcnt++;			/* bump q successor count */
/*@=mods@*/
/*@-nullpass -compmempass -type -abstract@*/
if (_te_debug)
fprintf(stderr, "addRelation:  AFTER q %p(%s)", q, teGetN(q));
prtTSI(NULL, teGetTSI(q));
/*@=nullpass =compmempass =type =abstract@*/
    return 0;
}
/*@=mustmod@*/

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
/*@-mustmod@*/
static void addQ(/*@dependent@*/ transactionElement p,
		/*@in@*/ /*@out@*/ transactionElement * qp,
		/*@in@*/ /*@out@*/ transactionElement * rp)
	/*@modifies p, *qp, *rp @*/
{
    transactionElement q, qprev;

    if ((*rp) == NULL) {	/* 1st element */
	/*@-dependenttrans@*/ /* FIX: double indirection */
	(*rp) = (*qp) = p;
	/*@=dependenttrans@*/
	return;
    }
    for (qprev = NULL, q = (*qp); q != NULL; qprev = q, q = teGetTSI(q)->tsi_suc) {
	if (teGetTSI(q)->tsi_qcnt <= teGetTSI(p)->tsi_qcnt)
	    break;
    }
    if (qprev == NULL) {	/* insert at beginning of list */
	teGetTSI(p)->tsi_suc = q;
	/*@-dependenttrans@*/
	(*qp) = p;		/* new head */
	/*@=dependenttrans@*/
    } else if (q == NULL) {	/* insert at end of list */
	teGetTSI(qprev)->tsi_suc = p;
	/*@-dependenttrans@*/
	(*rp) = p;		/* new tail */
	/*@=dependenttrans@*/
    } else {			/* insert between qprev and q */
	teGetTSI(p)->tsi_suc = q;
	teGetTSI(qprev)->tsi_suc = p;
    }
}
/*@=mustmod@*/

int rpmdepOrder(rpmTransactionSet ts)
{
	rpmDepSet requires;
	int_32 Flags;

    int numAddedPackages = ts->numAddedPackages;
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
/*@exposed@*/
    transactionElement newOrder;
    int newOrderCount = 0;
    orderListIndex orderList;
    int nrescans = 10;
    int _printed = 0;
    int qlen;
    int i, j;

    alMakeIndex(ts->addedPackages);

/*@-modfilesystem -nullpass -type -abstract@*/
if (_te_debug)
fprintf(stderr, "*** rpmdepOrder(%p) order %p[%d]\n", ts, ts->order, ts->orderCount);
/*@=modfilesystem =nullpass =type =abstract@*/

    /* T1. Initialize. */
    loopcheck = numAddedPackages;	/* XXX TR_ADDED only: should be ts->orderCount */
    pi = teInitIterator(ts);
    /* XXX Only added packages are ordered (for now). */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {
/*@-type@*/	/* FIX: transactionElement not opaque */
	p->tsi = xcalloc(1, sizeof(*p->tsi));
/*@=type@*/
    }
    pi = teFreeIterator(pi);

    /* Record all relations. */
    rpmMessage(RPMMESS_DEBUG, _("========== recording tsort relations\n"));
    pi = teInitIterator(ts);
    /* XXX Only added packages are ordered (for now). */
    while ((p = teNext(pi, TR_ADDED)) != NULL) {

/*@-type@*/	/* FIX: transactionElement not opaque */
	requires = p->requires;
/*@=type@*/
	if (requires == NULL)
	    continue;

	memset(selected, 0, sizeof(*selected) * ts->orderCount);

	/* Avoid narcisstic relations. */
	selected[teiGetOc(pi)] = 1;

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

/*@-type@*/
	p->npreds = teGetTSI(p)->tsi_count;
/*@=type@*/

/*@-modfilesystem -nullpass -type -abstract@*/
if (_te_debug)
fprintf(stderr, "\t+++ %p[%d] %s npreds %d\n", p, teiGetOc(pi), teGetNEVR(p), p->npreds);
/*@=modfilesystem =nullpass =type =abstract@*/

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
	    teGetTSI(p)->tsi_qcnt = (ts->orderCount - teiGetOc(pi));

	if (teGetTSI(p)->tsi_count != 0)
	    continue;
	teGetTSI(p)->tsi_suc = NULL;
	addQ(p, &q, &r);
	qlen++;
/*@-modfilesystem -nullpass -type -abstract @*/
if (_te_debug)
fprintf(stderr, "\t+++ addQ ++ qlen %d p %p(%s)", qlen, p, teGetNEVR(p));
prtTSI(" p", teGetTSI(p));
/*@=modfilesystem =nullpass =type =abstract @*/
    }
    pi = teFreeIterator(pi);

    /* T5. Output front of queue (T7. Remove from queue.) */
    /*@-branchstate@*/ /* FIX: r->tsi->tsi_next released */
    for (; q != NULL; q = teGetTSI(q)->tsi_suc) {

	/* XXX Only added packages are ordered (for now). */
	switch (teGetType(q)) {
	case TR_ADDED:
	    /*@switchbreak@*/ break;
	case TR_REMOVED:
	default:
	    continue;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	}

/*@-type@*/
	rpmMessage(RPMMESS_DEBUG, "%5d%5d%5d%3d %*s %s\n",
			orderingCount, q->npreds, teGetTSI(q)->tsi_qcnt, q->depth,
			(2 * q->depth), "",
			(teGetNEVR(q) ? teGetNEVR(q) : "???"));
/*@=type@*/
	ordering[orderingCount] = teGetAddedKey(q);
	orderingCount++;
	qlen--;
	loopcheck--;

	/* T6. Erase relations. */
	tsi_next = teGetTSI(q)->tsi_next;
	teGetTSI(q)->tsi_next = NULL;
	while ((tsi = tsi_next) != NULL) {
	    tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    p = tsi->tsi_suc;
	    if (p && (--teGetTSI(p)->tsi_count) <= 0) {
		/* XXX TODO: add control bit. */
		teGetTSI(p)->tsi_suc = NULL;
		/*@-nullstate@*/	/* FIX: q->tsi->tsi_u.suc may be NULL */
		addQ(p, &teGetTSI(q)->tsi_suc, &r);
		/*@=nullstate@*/
		qlen++;
/*@-modfilesystem -nullpass -type -abstract@*/
if (_te_debug)
fprintf(stderr, "\t+++ addQ ++ qlen %d p %p(%s)", qlen, p, teGetNEVR(p));
prtTSI(" p", teGetTSI(p));
/*@=modfilesystem =nullpass =type =abstract@*/
	    }
	    tsi = _free(tsi);
	}
	if (!_printed && loopcheck == qlen && teGetTSI(q)->tsi_suc != NULL) {
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
	    teGetTSI(q)->tsi_chain = NULL;
	    teGetTSI(q)->tsi_reqx = 0;
	    /* Mark packages already sorted. */
	    if (teGetTSI(q)->tsi_count == 0)
		teGetTSI(q)->tsi_count = -1;
	}
	qi = teFreeIterator(qi);

	/* T10. Mark all packages with their predecessors. */
	qi = teInitIterator(ts);
	/* XXX Only added packages are ordered (for now). */
	while ((q = teNext(qi, TR_ADDED)) != NULL) {
	    if ((tsi = teGetTSI(q)->tsi_next) == NULL)
		continue;
	    teGetTSI(q)->tsi_next = NULL;
	    markLoop(tsi, q);
	    teGetTSI(q)->tsi_next = tsi;
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
	    for (q = teGetTSI(r)->tsi_chain; q != NULL; q = teGetTSI(q)->tsi_chain) {
		if (teGetTSI(q)->tsi_reqx)
		    /*@innerbreak@*/ break;
		teGetTSI(q)->tsi_reqx = 1;
	    }

	    /* T13. Print predecessor chain from start of loop. */
	    while ((p = q) != NULL && (q = teGetTSI(p)->tsi_chain) != NULL) {
		const char * dp;
		char buf[4096];

		/* Unchain predecessor loop. */
		teGetTSI(p)->tsi_chain = NULL;

		if (!printed) {
		    rpmMessage(RPMMESS_DEBUG, _("LOOP:\n"));
		    printed = 1;
		}

		/* Find (and destroy if co-requisite) "q <- p" relation. */
/*@-type@*/	/* FIX: transactionElement not opaque */
		requires = p->requires;
/*@=type@*/
		requires = dsiInit(requires);
		dp = zapRelation(q, p, requires, 1, &nzaps);

		/* Print next member of loop. */
		buf[0] = '\0';
		if (teGetNEVR(p) != NULL)
		    (void) stpcpy(buf, teGetNEVR(p));
		rpmMessage(RPMMESS_DEBUG, "    %-40s %s\n", buf,
			(dp ? dp : "not found!?!"));

		dp = _free(dp);
	    }

	    /* Walk (and erase) linear part of predecessor chain as well. */
	    for (p = r, q = teGetTSI(r)->tsi_chain;
		 q != NULL;
		 p = q, q = teGetTSI(q)->tsi_chain)
	    {
		/* Unchain linear part of predecessor loop. */
		teGetTSI(p)->tsi_chain = NULL;
		teGetTSI(p)->tsi_reqx = 0;
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
	while ((tsi = teGetTSI(p)->tsi_next) != NULL) {
	    teGetTSI(p)->tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    tsi = _free(tsi);
	}
/*@-type@*/	/* FIX: transactionElement not opaque */
	p->tsi = _free(p->tsi);
/*@=type@*/

	/* Prepare added package ordering permutation. */
	orderList[j].pkgKey = teGetAddedKey(p);
	orderList[j].orIndex = teiGetOc(pi);
	j++;
    }
    pi = teFreeIterator(pi);
    assert(j <= numAddedPackages);

    qsort(orderList, numAddedPackages, sizeof(*orderList), orderListIndexCmp);

/*@-type -abstract @*/	/* FIX: transactionElement not opaque */
    newOrder = xcalloc(ts->orderCount, sizeof(*newOrder));
/*@=type =abstract @*/
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
/*@-type -abstract -noeffect @*/	/* FIX: transactionElement not opaque */
	q = ts->order + j;
	newOrder[newOrderCount++] = *q;		/* structure assignment */
/*@=type =abstract =noeffect @*/
	for (j = needle->orIndex + 1; j < ts->orderCount; j++) {
/*@-type -abstract -noeffect @*/	/* FIX: transactionElement not opaque */
	    q = ts->order + j;
	    if (teGetType(q) == TR_REMOVED
	     && teGetDependsOnKey(q) == needle->pkgKey)
	    {
		newOrder[newOrderCount++] = *q;	/* structure assignment */
	    } else
		/*@innerbreak@*/ break;
/*@=type =abstract =noeffect @*/
	}
    }
    /*@=branchstate@*/

    /*@-compmempass -usereleased@*/ /* FIX: ts->order[].{NEVR,name} released */
    pi = teInitIterator(ts);
    /*@=compmempass =usereleased@*/
    while ((p = teNext(pi, TR_REMOVED)) != NULL) {
	if (teGetDependsOnKey(p) == RPMAL_NOMATCH)
/*@-type -abstract -noeffect @*/	/* FIX: transactionElement not opaque */
	    newOrder[newOrderCount++] = *p;	/* structure assignment */
/*@=type =abstract =noeffect @*/
    }
    pi = teFreeIterator(pi);
    assert(newOrderCount == ts->orderCount);

/*@-type@*/	/* FIX: transactionElement not opaque */
    ts->order = _free(ts->order);
    ts->order = newOrder;
    ts->orderAlloced = ts->orderCount;
    orderList = _free(orderList);
/*@=type@*/

    /* Clean up after dependency checks */
    pi = teInitIterator(ts);
    while ((p = teNextIterator(pi)) != NULL) {
/*@-type@*/	/* FIX: transactionElement not opaque */
	p->this = dsFree(p->this);
	p->provides = dsFree(p->provides);
	p->requires = dsFree(p->requires);
	p->conflicts = dsFree(p->conflicts);
	p->obsoletes = dsFree(p->obsoletes);
/*@=type@*/
    }
    pi = teFreeIterator(pi);

    ts->addedPackages = alFree(ts->addedPackages);

    return 0;
}

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
/*@=mustmod =type@*/

int rpmdepCheck(rpmTransactionSet ts,
		rpmProblem * dsprobs, int * numProbs)
{
    rpmdbMatchIterator mi = NULL;
    teIterator pi = NULL; transactionElement p;
    int closeatexit = 0;
    int xx;
    int rc;

    /* Do lazy, readonly, open of rpm database. */
    if (ts->rpmdb == NULL) {
	if ((rc = rpmtsOpenDB(ts, ts->dbmode)) != 0)
	    goto exit;
	closeatexit = 1;
    }

    ts->probs = rpmProblemSetCreate();

    *dsprobs = NULL;
    *numProbs = 0;

    alMakeIndex(ts->addedPackages);
    alMakeIndex(ts->availablePackages);

    /*
     * Look at all of the added packages and make sure their dependencies
     * are satisfied.
     */
    pi = teInitIterator(ts);
    while ((p = teNext(pi, TR_ADDED)) != NULL) {
	rpmDepSet provides;
	rpmDepSet requires;
	rpmDepSet conflicts;
	uint_32 multiLib;

	multiLib = teGetMultiLib(p);

	/*@-type@*/	/* FIX: transactionElement not opaque */
	provides = p->provides;
	requires = p->requires;
	conflicts = p->conflicts;
	/*@=type@*/
        rpmMessage(RPMMESS_DEBUG,  "========== +++ %s\n" , teGetNEVR(p));
	rc = checkPackageDeps(ts, teGetNEVR(p), requires, conflicts,
			NULL, multiLib);
	if (rc)
	    goto exit;

	/* Adding: check name against conflicts matches. */
	rc = checkDependentConflicts(ts, teGetN(p));
	if (rc)
	    goto exit;

	rc = 0;
	provides = dsiInit(provides);
	if (provides == NULL || dsiGetN(provides) == NULL)
	    continue;
	while (dsiNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = dsiGetN(provides)) == NULL)
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
    pi = teFreeIterator(pi);

    /*
     * Look at the removed packages and make sure they aren't critical.
     */
    /*@-branchstate@*/
    pi = teInitIterator(ts);
    while ((p = teNext(pi, TR_REMOVED)) != NULL) {
	rpmDepSet provides;
	TFI_t fi;

	rpmMessage(RPMMESS_DEBUG,  "========== --- %s\n" , teGetNEVR(p));

	/* Erasing: check name against requiredby matches. */
	rc = checkDependentPackages(ts, teGetN(p));
	if (rc)
		goto exit;

	rc = 0;
	/*@-type@*/	/* FIX: transactionElement not opaque */
	provides = p->provides;
	/*@=type@*/
	provides = dsiInit(provides);
	if (provides != NULL)
	while (dsiNext(provides) >= 0) {
	    const char * Name;

	    if ((Name = dsiGetN(provides)) == NULL)
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
	/*@-type@*/	/* FIX: transactionElement not opaque */
	fi = p->fi;
	/*@=type@*/
	if ((fi = tfiInit(fi, 0)) != NULL)
	while (tfiNext(fi) >= 0) {
	    const char * fn = tfiGetFN(fi);

	    /* Erasing: check filename against requiredby matches. */
	    if (!checkDependentPackages(ts, fn))
		/*@innercontinue@*/ continue;
	    rc = 1;
	    /*@innerbreak@*/ break;
	}
	if (rc)
	    goto exit;
    }
    /*@=branchstate@*/
    pi = teFreeIterator(pi);

/*@-type@*/ /* FIX: return refcounted rpmProblemSet */
    if (ts->probs->numProblems) {
	*dsprobs = ts->probs->probs;
	ts->probs->probs = NULL;
	*numProbs = ts->probs->numProblems;
    }
/*@=type@*/
    rc = 0;

exit:
    mi = rpmdbFreeIterator(mi);
    pi = teFreeIterator(pi);
    /*@-branchstate@*/
    if (closeatexit)
	xx = rpmtsCloseDB(ts);
    else if (_cacheDependsRC)
	xx = rpmdbCloseDBI(ts->rpmdb, RPMDBI_DEPENDS);
    /*@=branchstate@*/
    ts->probs = rpmProblemSetFree(ts->probs);
    return rc;
}
