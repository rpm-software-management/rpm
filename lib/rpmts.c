/** \ingroup rpmdep
 * \file lib/rpmts.c
 * Routine(s) to handle a "rpmts" transaction sets.
 */
#include "system.h"

#include "rpmio_internal.h"	/* XXX for pgp and beecrypt */
#include <rpmlib.h>
#include <rpmmacro.h>		/* XXX rpmtsOpenDB() needs rpmGetPath */

#include "rpmdb.h"		/* XXX stealing db->db_mode. */

#include "rpmal.h"
#include "rpmds.h"
#include "rpmfi.h"

#define	_RPMTE_INTERNAL		/* XXX te->h */
#include "rpmte.h"

#define	_RPMTS_INTERNAL
#include "rpmts.h"

/* XXX FIXME: merge with existing (broken?) tests in system.h */
/* portability fiddles */
#if STATFS_IN_SYS_STATVFS
/*@-incondefs@*/
#if defined(__LCLINT__)
/*@-declundef -exportheader -protoparammatch @*/ /* LCL: missing annotation */
extern int statvfs (const char * file, /*@out@*/ struct statvfs * buf)
	/*@globals fileSystem @*/
	/*@modifies *buf, fileSystem @*/;
/*@=declundef =exportheader =protoparammatch @*/
/*@=incondefs@*/
#else
# include <sys/statvfs.h>
#endif
#else
# if STATFS_IN_SYS_VFS
#  include <sys/vfs.h>
# else
#  if STATFS_IN_SYS_MOUNT
#   include <sys/mount.h>
#  else
#   if STATFS_IN_SYS_STATFS
#    include <sys/statfs.h>
#   endif
#  endif
# endif
#endif

#include "debug.h"

/*@access rpmdb @*/		/* XXX db->db_chrootDone, NULL */

/*@access rpmps @*/
/*@access rpmDiskSpaceInfo @*/
/*@access rpmte @*/
/*@access rpmtsi @*/
/*@access fnpyKey @*/
/*@access pgpDig @*/
/*@access pgpDigParams @*/

/*@unchecked@*/
int _rpmts_debug = 0;

char * hGetNEVR(Header h, const char ** np)
{
    const char * n, * v, * r;
    char * NVR, * t;

    (void) headerNVR(h, &n, &v, &r);
    NVR = t = xcalloc(1, strlen(n) + strlen(v) + strlen(r) + sizeof("--"));
/*@-boundswrite@*/
    t = stpcpy(t, n);
    t = stpcpy(t, "-");
    t = stpcpy(t, v);
    t = stpcpy(t, "-");
    t = stpcpy(t, r);
    if (np)
	*np = n;
/*@=boundswrite@*/
    return NVR;
}

uint_32 hGetColor(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    uint_32 hcolor = 0;
    uint_32 * fcolors;
    int_32 ncolors;
    int i;

    fcolors = NULL;
    ncolors = 0;
    if (hge(h, RPMTAG_FILECOLORS, NULL, (void **)&fcolors, &ncolors)
     && fcolors != NULL && ncolors > 0)
    {
/*@-boundsread@*/
	for (i = 0; i < ncolors; i++)
	    hcolor |= fcolors[i];
/*@=boundsread@*/
    }
    hcolor &= 0x0f;

    return hcolor;
}

rpmts XrpmtsUnlink(rpmts ts, const char * msg, const char * fn, unsigned ln)
{
/*@-modfilesys@*/
if (_rpmts_debug)
fprintf(stderr, "--> ts %p -- %d %s at %s:%u\n", ts, ts->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    ts->nrefs--;
    return NULL;
}

rpmts XrpmtsLink(rpmts ts, const char * msg, const char * fn, unsigned ln)
{
    ts->nrefs++;
/*@-modfilesys@*/
if (_rpmts_debug)
fprintf(stderr, "--> ts %p ++ %d %s at %s:%u\n", ts, ts->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    /*@-refcounttrans@*/ return ts; /*@=refcounttrans@*/
}

int rpmtsCloseDB(rpmts ts)
{
    int rc = 0;

    if (ts->rdb != NULL) {
	rc = rpmdbClose(ts->rdb);
	ts->rdb = NULL;
    }
    return rc;
}

int rpmtsOpenDB(rpmts ts, int dbmode)
{
    int rc = 0;

    if (ts->rdb != NULL && ts->dbmode == dbmode)
	return 0;

    (void) rpmtsCloseDB(ts);

    /* XXX there's a potential db lock race here. */

    ts->dbmode = dbmode;
    rc = rpmdbOpen(ts->rootDir, &ts->rdb, ts->dbmode, 0644);
    if (rc) {
	const char * dn;
	dn = rpmGetPath(ts->rootDir, "%{_dbpath}", NULL);
	rpmMessage(RPMMESS_ERROR,
			_("cannot open Packages database in %s\n"), dn);
	dn = _free(dn);
    }
    return rc;
}

int rpmtsInitDB(rpmts ts, int dbmode)
{
    return rpmdbInit(ts->rootDir, dbmode);
}

int rpmtsRebuildDB(rpmts ts)
{
    int rc;
    if (!(ts->vsflags & RPMVSF_NOHDRCHK))
	rc = rpmdbRebuild(ts->rootDir, ts, headerCheck);
    else
	rc = rpmdbRebuild(ts->rootDir, NULL, NULL);
    return rc;
}

int rpmtsVerifyDB(rpmts ts)
{
    return rpmdbVerify(ts->rootDir);
}

rpmdbMatchIterator rpmtsInitIterator(const rpmts ts, rpmTag rpmtag,
			const void * keyp, size_t keylen)
{
    rpmdbMatchIterator mi;
    if (ts->rdb == NULL && rpmtsOpenDB(ts, ts->dbmode))
	return NULL;
    mi = rpmdbInitIterator(ts->rdb, rpmtag, keyp, keylen);
    if (mi && !(ts->vsflags & RPMVSF_NOHDRCHK))
	(void) rpmdbSetHdrChk(mi, ts, headerCheck);
    return mi;
}

rpmRC rpmtsFindPubkey(rpmts ts)
{
    const void * sig = rpmtsSig(ts);
    pgpDig dig = rpmtsDig(ts);
    pgpDigParams sigp = rpmtsSignature(ts);
    pgpDigParams pubp = rpmtsSignature(ts);
    rpmRC res;
    int xx;

    if (sig == NULL || dig == NULL || sigp == NULL || pubp == NULL) {
	res = RPMRC_NOKEY;
	goto exit;
    }

    if (ts->pkpkt == NULL
     || memcmp(sigp->signid, ts->pksignid, sizeof(ts->pksignid)))
    {
	int ix = -1;
	rpmdbMatchIterator mi;
	Header h;

	ts->pkpkt = _free(ts->pkpkt);
	ts->pkpktlen = 0;
	memset(ts->pksignid, 0, sizeof(ts->pksignid));

	/* Retrieve the pubkey that matches the signature. */
	mi = rpmtsInitIterator(ts, RPMTAG_PUBKEYS, sigp->signid, sizeof(sigp->signid));
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    const char ** pubkeys;
	    int_32 pt, pc;

	    if (!headerGetEntry(h, RPMTAG_PUBKEYS, &pt, (void **)&pubkeys, &pc))
		continue;
	    ix = rpmdbGetIteratorFileNum(mi);
/*@-boundsread@*/
	    if (ix >= pc
	     || b64decode(pubkeys[ix], (void **) &ts->pkpkt, &ts->pkpktlen))
		ix = -1;
/*@=boundsread@*/
	    pubkeys = headerFreeData(pubkeys, pt);
	    break;
	}
	mi = rpmdbFreeIterator(mi);

	/* Was a matching pubkey found? */
	if (ix < 0 || ts->pkpkt == NULL) {
	    res = RPMRC_NOKEY;
	    goto exit;
	}

	/*
	 * Can the pubkey packets be parsed?
	 * Do the parameters match the signature?
	 */
	if (pgpPrtPkts(ts->pkpkt, ts->pkpktlen, NULL, 0)
	 && sigp->pubkey_algo == pubp->pubkey_algo
#ifdef	NOTYET
	 && sigp->hash_algo == pubp->hash_algo
#endif
	 && !memcmp(sigp->signid, pubp->signid, sizeof(sigp->signid)))
	{
	    ts->pkpkt = _free(ts->pkpkt);
	    ts->pkpktlen = 0;
	    res = RPMRC_NOKEY;
	    goto exit;
	}

	/* XXX Verify the pubkey signature. */

	/* Packet looks good, save the signer id. */
/*@-boundsread@*/
	memcpy(ts->pksignid, sigp->signid, sizeof(ts->pksignid));
/*@=boundsread@*/

	rpmMessage(RPMMESS_DEBUG, "========== %s pubkey id %s\n",
		(sigp->pubkey_algo == PGPPUBKEYALGO_DSA ? "DSA" :
		(sigp->pubkey_algo == PGPPUBKEYALGO_RSA ? "RSA" : "???")),
		pgpHexStr(sigp->signid, sizeof(sigp->signid)));

    }

#ifdef	NOTNOW
    {
	if (ts->pkpkt == NULL) {
	    const char * pkfn = rpmExpand("%{_gpg_pubkey}", NULL);
	    if (pgpReadPkts(pkfn, &ts->pkpkt, &ts->pkpktlen) != PGPARMOR_PUBKEY) {
		pkfn = _free(pkfn);
		res = RPMRC_NOKEY;
		goto exit;
	    }
	    pkfn = _free(pkfn);
	}
    }
#endif

    /* Retrieve parameters from pubkey packet(s). */
    xx = pgpPrtPkts(ts->pkpkt, ts->pkpktlen, dig, 0);

    /* Do the parameters match the signature? */
    if (sigp->pubkey_algo == pubp->pubkey_algo
#ifdef	NOTYET
     && sigp->hash_algo == pubp->hash_algo
#endif
     &&	!memcmp(sigp->signid, pubp->signid, sizeof(sigp->signid)) )
	res = RPMRC_OK;
    else
	res = RPMRC_NOKEY;

    /* XXX Verify the signature signature. */

exit:
    return res;
}

int rpmtsCloseSDB(rpmts ts)
{
    int rc = 0;

    if (ts->sdb != NULL) {
	rc = rpmdbClose(ts->sdb);
	ts->sdb = NULL;
    }
    return rc;
}

int rpmtsOpenSDB(rpmts ts, int dbmode)
{
    static int has_sdbpath = -1;
    int rc = 0;

    if (ts->sdb != NULL && ts->sdbmode == dbmode)
	return 0;

    if (has_sdbpath < 0)
	has_sdbpath = rpmExpandNumeric("%{?_solve_dbpath:1}");

    /* If not configured, don't try to open. */
    if (has_sdbpath <= 0)
	return 1;

    addMacro(NULL, "_dbpath", NULL, "%{_solve_dbpath}", RMIL_DEFAULT);

    rc = rpmdbOpen(ts->rootDir, &ts->sdb, ts->sdbmode, 0644);
    if (rc) {
	const char * dn;
	dn = rpmGetPath(ts->rootDir, "%{_dbpath}", NULL);
	rpmMessage(RPMMESS_WARNING,
			_("cannot open Solve database in %s\n"), dn);
	dn = _free(dn);
    }
    delMacro(NULL, "_dbpath");

    return rc;
}

/**
 * Compare suggested package resolutions (qsort/bsearch).
 * @param a		1st instance address
 * @param b		2nd instance address
 * @return		result of comparison
 */
static int sugcmp(const void * a, const void * b)
	/*@*/
{
/*@-boundsread@*/
    const char * astr = *(const char **)a;
    const char * bstr = *(const char **)b;
/*@=boundsread@*/
    return strcmp(astr, bstr);
}

/*@-bounds@*/
int rpmtsSolve(rpmts ts, rpmds ds, /*@unused@*/ const void * data)
{
    const char * errstr;
    const char * str;
    const char * qfmt;
    rpmdbMatchIterator mi;
    Header bh;
    Header h;
    size_t bhnamelen;
    time_t bhtime;
    rpmTag rpmtag;
    const char * keyp;
    size_t keylen;
    int rc = 1;	/* assume not found */
    int xx;

    /* Make suggestions only for install Requires: */
    if (ts->goal != TSM_INSTALL)
	return rc;

    if (rpmdsTagN(ds) != RPMTAG_REQUIRENAME)
	return rc;

    keyp = rpmdsN(ds);
    if (keyp == NULL)
	return rc;

    if (ts->sdb == NULL) {
	xx = rpmtsOpenSDB(ts, ts->sdbmode);
	if (xx) return rc;
    }

    /* Look for a matching Provides: in suggested universe. */
    rpmtag = (*keyp == '/' ? RPMTAG_BASENAMES : RPMTAG_PROVIDENAME);
    keylen = 0;
    mi = rpmdbInitIterator(ts->sdb, rpmtag, keyp, keylen);
    bhnamelen = 0;
    bhtime = 0;
    bh = NULL;
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	const char * hname;
	size_t hnamelen;
	time_t htime;
	int_32 * ip;

	if (rpmtag == RPMTAG_PROVIDENAME && !rpmdsAnyMatchesDep(h, ds, 1))
	    continue;

	/* XXX Prefer the shortest name if given alternatives. */
	hname = NULL;
	hnamelen = 0;
	if (headerGetEntry(h, RPMTAG_NAME, NULL, (void **)&hname, NULL)) {
	    if (hname)
		hnamelen = strlen(hname);
	}
	if (bhnamelen > 0 && hnamelen > bhnamelen)
	    continue;

	/* XXX Prefer the newest build if given alternatives. */
	htime = 0;
	if (headerGetEntry(h, RPMTAG_BUILDTIME, NULL, (void **)&ip, NULL))
	    htime = (time_t)*ip;

	if (htime <= bhtime)
	    continue;

	bh = headerFree(bh);
	bh = headerLink(h);
	bhtime = htime;
	bhnamelen = hnamelen;
    }
    mi = rpmdbFreeIterator(mi);

    /* Is there a suggested resolution? */
    if (bh == NULL)
	goto exit;

    /* Format the suggestion. */
    qfmt = rpmExpand("%{?_solve_name_fmt}", NULL);
    if (qfmt == NULL || *qfmt == '\0')
	goto exit;
    str = headerSprintf(bh, qfmt, rpmTagTable, rpmHeaderFormats, &errstr);
    bh = headerFree(bh);
    qfmt = _free(qfmt);
    if (str == NULL) {
	rpmError(RPMERR_QFMT, _("incorrect format: %s\n"), errstr);
	goto exit;
    }

    if (ts->transFlags & RPMTRANS_FLAG_ADDINDEPS) {
	FD_t fd;
	rpmRC rpmrc;

	h = headerFree(h);
	fd = Fopen(str, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), str,
			Fstrerror(fd));
            if (fd != NULL) {
                xx = Fclose(fd);
                fd = NULL;
            }
	    str = _free(str);
	    goto exit;
	}
	rpmrc = rpmReadPackageFile(ts, fd, str, &h);
	xx = Fclose(fd);
	switch (rpmrc) {
	default:
	    str = _free(str);
	    break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    if (h != NULL &&
	        !rpmtsAddInstallElement(ts, h, (fnpyKey)str, 1, NULL))
	    {
		rpmMessage(RPMMESS_DEBUG, _("Adding: %s\n"), str);
		rc = -1;
		/* XXX str memory leak */
		break;
	    }
	    str = _free(str);
	    break;
	}
	h = headerFree(h);
	goto exit;
    }

    rpmMessage(RPMMESS_DEBUG, _("Suggesting: %s\n"), str);
    /* If suggestion is already present, don't bother. */
    if (ts->suggests != NULL && ts->nsuggests > 0) {
	if (bsearch(&str, ts->suggests, ts->nsuggests,
			sizeof(*ts->suggests), sugcmp))
	    goto exit;
    }

    /* Add a new (unique) suggestion. */
    ts->suggests = xrealloc(ts->suggests,
			sizeof(*ts->suggests) * (ts->nsuggests + 2));
    ts->suggests[ts->nsuggests] = str;
    ts->nsuggests++;
    ts->suggests[ts->nsuggests] = NULL;

    if (ts->nsuggests > 1)
	qsort(ts->suggests, ts->nsuggests, sizeof(*ts->suggests), sugcmp);

exit:
/*@-nullstate@*/ /* FIX: ts->suggests[] may be NULL */
    return rc;
/*@=nullstate@*/
}
/*@=bounds@*/

int rpmtsAvailable(rpmts ts, const rpmds ds)
{
    fnpyKey * sugkey;
    int rc = 1;	/* assume not found */

    if (ts->availablePackages == NULL)
	return rc;
    sugkey = rpmalAllSatisfiesDepend(ts->availablePackages, ds, NULL);
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

int rpmtsSetSolveCallback(rpmts ts,
		int (*solve) (rpmts ts, rpmds key, const void * data),
		const void * solveData)
{
    int rc = 0;

/*@-branchstate@*/
    if (ts) {
/*@-assignexpose -temptrans @*/
	ts->solve = solve;
	ts->solveData = solveData;
/*@=assignexpose =temptrans @*/
    }
/*@=branchstate@*/
    return rc;
}

rpmps rpmtsProblems(rpmts ts)
{
    rpmps ps = NULL;
    if (ts) {
	if (ts->probs)
	    ps = rpmpsLink(ts->probs, NULL);
    }
    return ps;
}

void rpmtsCleanDig(rpmts ts)
{
    ts->sig = headerFreeData(ts->sig, ts->sigtype);
    ts->dig = pgpFreeDig(ts->dig);
}

void rpmtsClean(rpmts ts)
{
    rpmtsi pi; rpmte p;

    if (ts == NULL)
	return;

    /* Clean up after dependency checks. */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteCleanDS(p);
    pi = rpmtsiFree(pi);

    ts->addedPackages = rpmalFree(ts->addedPackages);
    ts->numAddedPackages = 0;

    ts->suggests = _free(ts->suggests);
    ts->nsuggests = 0;

    ts->probs = rpmpsFree(ts->probs);

    rpmtsCleanDig(ts);
}

void rpmtsEmpty(rpmts ts)
{
    rpmtsi pi; rpmte p;
    int oc;

    if (ts == NULL)
	return;

/*@-nullstate@*/	/* FIX: partial annotations */
    rpmtsClean(ts);
/*@=nullstate@*/

    for (pi = rpmtsiInit(ts), oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
/*@-type -unqualifiedtrans @*/
	ts->order[oc] = rpmteFree(ts->order[oc]);
/*@=type =unqualifiedtrans @*/
    }
    pi = rpmtsiFree(pi);

    ts->orderCount = 0;

    ts->numRemovedPackages = 0;
/*@-nullstate@*/	/* FIX: partial annotations */
    return;
/*@=nullstate@*/
}

rpmts rpmtsFree(rpmts ts)
{
    if (ts == NULL)
	return NULL;

    if (ts->nrefs > 1)
	return rpmtsUnlink(ts, "tsCreate");

/*@-nullstate@*/	/* FIX: partial annotations */
    rpmtsEmpty(ts);
/*@=nullstate@*/

    (void) rpmtsCloseDB(ts);

    (void) rpmtsCloseSDB(ts);

    ts->removedPackages = _free(ts->removedPackages);

    ts->availablePackages = rpmalFree(ts->availablePackages);
    ts->numAvailablePackages = 0;

    ts->dsi = _free(ts->dsi);

    if (ts->scriptFd != NULL) {
	ts->scriptFd = fdFree(ts->scriptFd, "rpmtsFree");
	ts->scriptFd = NULL;
    }
    ts->rootDir = _free(ts->rootDir);
    ts->currDir = _free(ts->currDir);

/*@-type +voidabstract @*/	/* FIX: double indirection */
    ts->order = _free(ts->order);
/*@=type =voidabstract @*/
    ts->orderAlloced = 0;

    if (ts->pkpkt != NULL)
	ts->pkpkt = _free(ts->pkpkt);
    ts->pkpktlen = 0;
    memset(ts->pksignid, 0, sizeof(ts->pksignid));

    (void) rpmtsUnlink(ts, "tsCreate");

    /*@-refcounttrans -usereleased @*/
    ts = _free(ts);
    /*@=refcounttrans =usereleased @*/

    return NULL;
}

rpmVSFlags rpmtsVSFlags(rpmts ts)
{
    rpmVSFlags vsflags = 0;
    if (ts != NULL)
	vsflags = ts->vsflags;
    return vsflags;
}

rpmVSFlags rpmtsSetVSFlags(rpmts ts, rpmVSFlags vsflags)
{
    rpmVSFlags ovsflags = 0;
    if (ts != NULL) {
	ovsflags = ts->vsflags;
	ts->vsflags = vsflags;
    }
    return ovsflags;
}

int rpmtsUnorderedSuccessors(rpmts ts, int first)
{
    int unorderedSuccessors = 0;
    if (ts != NULL) {
	unorderedSuccessors = ts->unorderedSuccessors;
	if (first >= 0)
	    ts->unorderedSuccessors = first;
    }
    return unorderedSuccessors;
}

const char * rpmtsRootDir(rpmts ts)
{
    return (ts != NULL ? ts->rootDir : NULL);
}

void rpmtsSetRootDir(rpmts ts, const char * rootDir)
{
    if (ts != NULL) {
	size_t rootLen;

	ts->rootDir = _free(ts->rootDir);

	if (rootDir == NULL) {
#ifndef	DYING
	    ts->rootDir = xstrdup("");
#endif
	    return;
	}
	rootLen = strlen(rootDir);

/*@-branchstate@*/
	/* Make sure that rootDir has trailing / */
	if (!(rootLen && rootDir[rootLen - 1] == '/')) {
	    char * t = alloca(rootLen + 2);
	    *t = '\0';
	    (void) stpcpy( stpcpy(t, rootDir), "/");
	    rootDir = t;
	}
/*@=branchstate@*/
	ts->rootDir = xstrdup(rootDir);
    }
}

const char * rpmtsCurrDir(rpmts ts)
{
    const char * currDir = NULL;
    if (ts != NULL) {
	currDir = ts->currDir;
    }
    return currDir;
}

void rpmtsSetCurrDir(rpmts ts, const char * currDir)
{
    if (ts != NULL) {
	ts->currDir = _free(ts->currDir);
	if (currDir)
	    ts->currDir = xstrdup(currDir);
    }
}

FD_t rpmtsScriptFd(rpmts ts)
{
    FD_t scriptFd = NULL;
    if (ts != NULL) {
	scriptFd = ts->scriptFd;
    }
/*@-compdef -refcounttrans -usereleased@*/
    return scriptFd;
/*@=compdef =refcounttrans =usereleased@*/
}

void rpmtsSetScriptFd(rpmts ts, FD_t scriptFd)
{

    if (ts != NULL) {
	if (ts->scriptFd != NULL) {
	    ts->scriptFd = fdFree(ts->scriptFd, "rpmtsSetScriptFd");
	    ts->scriptFd = NULL;
	}
/*@+voidabstract@*/
	if (scriptFd != NULL)
	    ts->scriptFd = fdLink((void *)scriptFd, "rpmtsSetScriptFd");
/*@=voidabstract@*/
    }
}

int rpmtsChrootDone(rpmts ts)
{
    int chrootDone = 0;
    if (ts != NULL) {
	chrootDone = ts->chrootDone;
    }
    return chrootDone;
}

int rpmtsSetChrootDone(rpmts ts, int chrootDone)
{
    int ochrootDone = 0;
    if (ts != NULL) {
	ochrootDone = ts->chrootDone;
	if (ts->rdb != NULL)
	    ts->rdb->db_chrootDone = chrootDone;
	ts->chrootDone = chrootDone;
    }
    return ochrootDone;
}

int_32 rpmtsGetTid(rpmts ts)
{
    int_32 tid = 0;
    if (ts != NULL) {
	tid = ts->tid;
    }
    return tid;
}

int_32 rpmtsSetTid(rpmts ts, int_32 tid)
{
    int_32 otid = 0;
    if (ts != NULL) {
	otid = ts->tid;
	ts->tid = tid;
    }
    return otid;
}

int_32 rpmtsSigtag(const rpmts ts)
{
    int_32 sigtag = 0;
    if (ts != NULL)
	sigtag = ts->sigtag;
    return sigtag;
}

int_32 rpmtsSigtype(const rpmts ts)
{
    int_32 sigtag = 0;
    if (ts != NULL)
	sigtag = ts->sigtag;
    return sigtag;
}

const void * rpmtsSig(const rpmts ts)
{
    const void * sig = NULL;
    if (ts != NULL)
	sig = ts->sig;
    return sig;
}

int_32 rpmtsSiglen(const rpmts ts)
{
    int_32 siglen = 0;
    if (ts != NULL)
	siglen = ts->siglen;
    return siglen;
}

int rpmtsSetSig(rpmts ts,
		int_32 sigtag, int_32 sigtype, const void * sig, int_32 siglen)
{
    if (ts != NULL) {
	if (ts->sig && ts->sigtype)
	    ts->sig = headerFreeData(ts->sig, ts->sigtype);
	ts->sigtag = sigtag;
	ts->sigtype = (sig ? sigtype : 0);
/*@-assignexpose -kepttrans@*/
	ts->sig = sig;
/*@=assignexpose =kepttrans@*/
	ts->siglen = siglen;
    }
    return 0;
}

pgpDig rpmtsDig(rpmts ts)
{
/*@-mods@*/ /* FIX: hide lazy malloc for now */
    if (ts->dig == NULL)
	ts->dig = pgpNewDig();
/*@=mods@*/
    if (ts->dig == NULL)
	return NULL;
    return ts->dig;
}

pgpDigParams rpmtsSignature(const rpmts ts)
{
    pgpDig dig = rpmtsDig(ts);
    if (dig == NULL) return NULL;
/*@-immediatetrans@*/
    return &dig->signature;
/*@=immediatetrans@*/
}

pgpDigParams rpmtsPubkey(const rpmts ts)
{
    pgpDig dig = rpmtsDig(ts);
    if (dig == NULL) return NULL;
/*@-immediatetrans@*/
    return &dig->pubkey;
/*@=immediatetrans@*/
}

rpmdb rpmtsGetRdb(rpmts ts)
{
    rpmdb rdb = NULL;
    if (ts != NULL) {
	rdb = ts->rdb;
    }
/*@-compdef -refcounttrans -usereleased @*/
    return rdb;
/*@=compdef =refcounttrans =usereleased @*/
}

int rpmtsInitDSI(const rpmts ts)
{
    rpmDiskSpaceInfo dsi;
    struct stat sb;
    int rc;
    int i;

    if (rpmtsFilterFlags(ts) & RPMPROB_FILTER_DISKSPACE)
	return 0;

    rpmMessage(RPMMESS_DEBUG, _("mounted filesystems:\n"));
    rpmMessage(RPMMESS_DEBUG,
	_("    i    dev bsize       bavail       iavail mount point\n"));

    rc = rpmGetFilesystemList(&ts->filesystems, &ts->filesystemCount);
    if (rc || ts->filesystems == NULL || ts->filesystemCount <= 0)
	return rc;

    /* Get available space on mounted file systems. */

    ts->dsi = _free(ts->dsi);
    ts->dsi = xcalloc((ts->filesystemCount + 1), sizeof(*ts->dsi));

    dsi = ts->dsi;

    if (dsi != NULL)
    for (i = 0; (i < ts->filesystemCount) && dsi; i++, dsi++) {
#if STATFS_IN_SYS_STATVFS
	struct statvfs sfb;
	memset(&sfb, 0, sizeof(sfb));
	rc = statvfs(ts->filesystems[i], &sfb);
#else
	struct statfs sfb;
	memset(&sfb, 0, sizeof(sfb));
#  if STAT_STATFS4
/* This platform has the 4-argument version of the statfs call.  The last two
 * should be the size of struct statfs and 0, respectively.  The 0 is the
 * filesystem type, and is always 0 when statfs is called on a mounted
 * filesystem, as we're doing.
 */
	rc = statfs(ts->filesystems[i], &sfb, sizeof(sfb), 0);
#  else
	rc = statfs(ts->filesystems[i], &sfb);
#  endif
#endif
	if (rc)
	    break;

	rc = stat(ts->filesystems[i], &sb);
	if (rc)
	    break;
	dsi->dev = sb.st_dev;

	dsi->bsize = sfb.f_bsize;
	dsi->bneeded = 0;
	dsi->ineeded = 0;
#ifdef STATFS_HAS_F_BAVAIL
	dsi->bavail = sfb.f_bavail;
#else
/* FIXME: the statfs struct doesn't have a member to tell how many blocks are
 * available for non-superusers.  f_blocks - f_bfree is probably too big, but
 * it's about all we can do.
 */
	dsi->bavail = sfb.f_blocks - sfb.f_bfree;
#endif
	/* XXX Avoid FAT and other file systems that have not inodes. */
	dsi->iavail = !(sfb.f_ffree == 0 && sfb.f_files == 0)
				? sfb.f_ffree : -1;
	rpmMessage(RPMMESS_DEBUG, _("%5d 0x%04x %5u %12ld %12ld %s\n"),
		i, (unsigned) dsi->dev, (unsigned) dsi->bsize,
		(signed long) dsi->bavail, (signed long) dsi->iavail,
		ts->filesystems[i]);
    }
    return rc;
}

void rpmtsUpdateDSI(const rpmts ts, dev_t dev,
		uint_32 fileSize, uint_32 prevSize, uint_32 fixupSize,
		fileAction action)
{
    rpmDiskSpaceInfo dsi;
    uint_32 bneeded;

    dsi = ts->dsi;
    if (dsi) {
	while (dsi->bsize && dsi->dev != dev)
	    dsi++;
	if (dsi->bsize == 0)
	    dsi = NULL;
    }
    if (dsi == NULL)
	return;

    bneeded = BLOCK_ROUND(fileSize, dsi->bsize);

    switch (action) {
    case FA_BACKUP:
    case FA_SAVE:
    case FA_ALTNAME:
	dsi->ineeded++;
	dsi->bneeded += bneeded;
	/*@switchbreak@*/ break;

    /*
     * FIXME: If two packages share a file (same md5sum), and
     * that file is being replaced on disk, will dsi->bneeded get
     * adjusted twice? Quite probably!
     */
    case FA_CREATE:
	dsi->bneeded += bneeded;
	dsi->bneeded -= BLOCK_ROUND(prevSize, dsi->bsize);
	/*@switchbreak@*/ break;

    case FA_ERASE:
	dsi->ineeded--;
	dsi->bneeded -= bneeded;
	/*@switchbreak@*/ break;

    default:
	/*@switchbreak@*/ break;
    }

    if (fixupSize)
	dsi->bneeded -= BLOCK_ROUND(fixupSize, dsi->bsize);
}

void rpmtsCheckDSIProblems(const rpmts ts, const rpmte te)
{
    rpmDiskSpaceInfo dsi;
    rpmps ps;
    int fc;
    int i;

    if (ts->filesystems == NULL || ts->filesystemCount <= 0)
	return;

    dsi = ts->dsi;
    if (dsi == NULL)
	return;
    fc = rpmfiFC( rpmteFI(te, RPMTAG_BASENAMES) );
    if (fc <= 0)
	return;

    ps = rpmtsProblems(ts);
    for (i = 0; i < ts->filesystemCount; i++, dsi++) {

	if (dsi->bavail > 0 && adj_fs_blocks(dsi->bneeded) > dsi->bavail) {
	    rpmpsAppend(ps, RPMPROB_DISKSPACE,
			rpmteNEVR(te), rpmteKey(te),
			ts->filesystems[i], NULL, NULL,
 	   (adj_fs_blocks(dsi->bneeded) - dsi->bavail) * dsi->bsize);
	}

	if (dsi->iavail > 0 && adj_fs_blocks(dsi->ineeded) > dsi->iavail) {
	    rpmpsAppend(ps, RPMPROB_DISKNODES,
			rpmteNEVR(te), rpmteKey(te),
			ts->filesystems[i], NULL, NULL,
 	    (adj_fs_blocks(dsi->ineeded) - dsi->iavail));
	}
    }
    ps = rpmpsFree(ps);
}

void * rpmtsNotify(rpmts ts, rpmte te,
		rpmCallbackType what, unsigned long amount, unsigned long total)
{
    void * ptr = NULL;
    if (ts && ts->notify && te) {
assert(!(te->type == TR_ADDED && te->h == NULL));
	/*@-type@*/ /* FIX: cast? */
	/*@-noeffectuncon @*/ /* FIX: check rc */
	ptr = ts->notify(te->h, what, amount, total,
			rpmteKey(te), ts->notifyData);
	/*@=noeffectuncon @*/
	/*@=type@*/
    }
    return ptr;
}

int rpmtsNElements(rpmts ts)
{
    int nelements = 0;
    if (ts != NULL && ts->order != NULL) {
	nelements = ts->orderCount;
    }
    return nelements;
}

rpmte rpmtsElement(rpmts ts, int ix)
{
    rpmte te = NULL;
    if (ts != NULL && ts->order != NULL) {
	if (ix >= 0 && ix < ts->orderCount)
	    te = ts->order[ix];
    }
    /*@-compdef@*/
    return te;
    /*@=compdef@*/
}

rpmprobFilterFlags rpmtsFilterFlags(rpmts ts)
{
    return (ts != NULL ? ts->ignoreSet : 0);
}

rpmtransFlags rpmtsFlags(rpmts ts)
{
    return (ts != NULL ? ts->transFlags : 0);
}

rpmtransFlags rpmtsSetFlags(rpmts ts, rpmtransFlags transFlags)
{
    rpmtransFlags otransFlags = 0;
    if (ts != NULL) {
	otransFlags = ts->transFlags;
	ts->transFlags = transFlags;
    }
    return otransFlags;
}

Spec rpmtsSpec(rpmts ts)
{
/*@-compdef -retexpose -usereleased@*/
    return ts->spec;
/*@=compdef =retexpose =usereleased@*/
}

Spec rpmtsSetSpec(rpmts ts, Spec spec)
{
    Spec ospec = ts->spec;
/*@-assignexpose -temptrans@*/
    ts->spec = spec;
/*@=assignexpose =temptrans@*/
    return ospec;
}

rpmte rpmtsRelocateElement(rpmts ts)
{
/*@-compdef -retexpose -usereleased@*/
    return ts->relocateElement;
/*@=compdef =retexpose =usereleased@*/
}

rpmte rpmtsSetRelocateElement(rpmts ts, rpmte relocateElement)
{
    rpmte orelocateElement = ts->relocateElement;
/*@-assignexpose -temptrans@*/
    ts->relocateElement = relocateElement;
/*@=assignexpose =temptrans@*/
    return orelocateElement;
}

uint_32 rpmtsColor(rpmts ts)
{
    return (ts != NULL ? ts->color : 0);
}

uint_32 rpmtsSetColor(rpmts ts, uint_32 color)
{
    uint_32 ocolor = 0;
    if (ts != NULL) {
	ocolor = ts->color;
	ts->color = color;
    }
    return ocolor;
}

int rpmtsSetNotifyCallback(rpmts ts,
		rpmCallbackFunction notify, rpmCallbackData notifyData)
{
    if (ts != NULL) {
	ts->notify = notify;
	ts->notifyData = notifyData;
    }
    return 0;
}

int rpmtsGetKeys(const rpmts ts, fnpyKey ** ep, int * nep)
{
    int rc = 0;

    if (nep) *nep = ts->orderCount;
    if (ep) {
	rpmtsi pi;	rpmte p;
	fnpyKey * e;

	*ep = e = xmalloc(ts->orderCount * sizeof(*e));
	pi = rpmtsiInit(ts);
	while ((p = rpmtsiNext(pi, 0)) != NULL) {
	    switch (rpmteType(p)) {
	    case TR_ADDED:
		/*@-dependenttrans@*/
		*e = rpmteKey(p);
		/*@=dependenttrans@*/
		/*@switchbreak@*/ break;
	    case TR_REMOVED:
	    default:
		*e = NULL;
		/*@switchbreak@*/ break;
	    }
	    e++;
	}
	pi = rpmtsiFree(pi);
    }
    return rc;
}

rpmts rpmtsCreate(void)
{
    rpmts ts;

    ts = xcalloc(1, sizeof(*ts));
    ts->goal = TSM_UNKNOWN;
    ts->filesystemCount = 0;
    ts->filesystems = NULL;
    ts->dsi = NULL;

    ts->solve = rpmtsSolve;
    ts->solveData = NULL;
    ts->nsuggests = 0;
    ts->suggests = NULL;
    ts->sdb = NULL;
    ts->sdbmode = O_RDONLY;

    ts->rdb = NULL;
    ts->dbmode = O_RDONLY;

    ts->scriptFd = NULL;
    ts->tid = (int_32) time(NULL);
    ts->delta = 5;

    ts->color = rpmExpandNumeric("%{?_transaction_color}");

    ts->numRemovedPackages = 0;
    ts->allocedRemovedPackages = ts->delta;
    ts->removedPackages = xcalloc(ts->allocedRemovedPackages,
			sizeof(*ts->removedPackages));

    ts->rootDir = NULL;
    ts->currDir = NULL;
    ts->chrootDone = 0;

    ts->numAddedPackages = 0;
    ts->addedPackages = NULL;

    ts->numAvailablePackages = 0;
    ts->availablePackages = NULL;

    ts->orderAlloced = 0;
    ts->orderCount = 0;
    ts->order = NULL;

    ts->probs = NULL;

    ts->sig = NULL;
    ts->pkpkt = NULL;
    ts->pkpktlen = 0;
    memset(ts->pksignid, 0, sizeof(ts->pksignid));
    ts->dig = NULL;

    ts->nrefs = 0;

    return rpmtsLink(ts, "tsCreate");
}
