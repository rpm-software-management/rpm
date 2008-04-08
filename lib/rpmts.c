/** \ingroup rpmdep
 * \file lib/rpmts.c
 * Routine(s) to handle a "rpmts" transaction sets.
 */
#include "system.h"

#include <rpm/rpmtag.h>
#include <rpm/rpmlib.h>			/* rpmReadPackage etc */
#include <rpm/rpmurl.h>
#include "rpmio/digest.h"
#include <rpm/rpmmacro.h>
#include <rpm/rpmfileutil.h>		/* rpmtsOpenDB() needs rpmGetPath */
#include <rpm/rpmstring.h>

#include <rpm/rpmdb.h>
#include <rpm/rpmal.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmte.h>

#include "lib/rpmlock.h"
#include "lib/rpmts_internal.h"

/* XXX FIXME: merge with existing (broken?) tests in system.h */
/* portability fiddles */
#if STATFS_IN_SYS_STATVFS
#include <sys/statvfs.h>

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

int _rpmts_debug = 0;

int _rpmts_stats = 0;

rpmts rpmtsUnlink(rpmts ts, const char * msg)
{
if (_rpmts_debug)
fprintf(stderr, "--> ts %p -- %d %s\n", ts, ts->nrefs, msg);
    ts->nrefs--;
    return NULL;
}

rpmts rpmtsLink(rpmts ts, const char * msg)
{
    ts->nrefs++;
if (_rpmts_debug)
fprintf(stderr, "--> ts %p ++ %d %s\n", ts, ts->nrefs, msg);
    return ts;
}

int rpmtsCloseDB(rpmts ts)
{
    int rc = 0;

    if (ts->rdb != NULL) {
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBGET), 
			rpmdbOp(ts->rdb, RPMDB_OP_DBGET));
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBPUT),
			rpmdbOp(ts->rdb, RPMDB_OP_DBPUT));
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBDEL),
			rpmdbOp(ts->rdb, RPMDB_OP_DBDEL));
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
	char * dn = rpmGetPath(ts->rootDir, "%{_dbpath}", NULL);
	rpmlog(RPMLOG_ERR,
			_("cannot open Packages database in %s\n"), dn);
	dn = _free(dn);
    }
    return rc;
}

int rpmtsInitDB(rpmts ts, int dbmode)
{
    void *lock = rpmtsAcquireLock(ts);
    int rc = -1;
    if (lock)
	    rc = rpmdbInit(ts->rootDir, dbmode);
    rpmtsFreeLock(lock);
    return rc;
}

int rpmtsGetDBMode(rpmts ts)
{
    assert(ts != NULL);
    return (ts->dbmode);
}

int rpmtsSetDBMode(rpmts ts, int dbmode)
{
    int rc = 1;
    /* mode setting only permitted on non-open db */
    if (ts != NULL && rpmtsGetRdb(ts) == NULL) {
    	ts->dbmode = dbmode;
	rc = 0;
    }
    return rc;
}


int rpmtsRebuildDB(rpmts ts)
{
    int rc;
    void *lock = rpmtsAcquireLock(ts);
    if (!lock) return -1;
    if (!(ts->vsflags & RPMVSF_NOHDRCHK))
	rc = rpmdbRebuild(ts->rootDir, ts, headerCheck);
    else
	rc = rpmdbRebuild(ts->rootDir, NULL, NULL);
    rpmtsFreeLock(lock);
    return rc;
}

int rpmtsVerifyDB(rpmts ts)
{
    return rpmdbVerify(ts->rootDir);
}

static int isArch(const char * arch)
{
    const char * const * av;
    static const char * const arches[] = {
	"i386", "i486", "i586", "i686", "athlon", "pentium3", "pentium4", "x86_64", "amd64", "ia32e", "geode",
	"alpha", "alphaev5", "alphaev56", "alphapca56", "alphaev6", "alphaev67",
	"sparc", "sun4", "sun4m", "sun4c", "sun4d", "sparcv8", "sparcv9", "sparcv9v",
	"sparc64", "sparc64v", "sun4u",
	"mips", "mipsel", "IP",
	"ppc", "ppciseries", "ppcpseries",
	"ppc64", "ppc64iseries", "ppc64pseries",
	"m68k",
	"rs6000",
	"ia64",
	"armv3l", "armv4b", "armv4l", "armv4tl", "armv5tel", "armv5tejl", "armv6l",
	"s390", "i370", "s390x",
	"sh", "xtensa",
	"noarch",
	NULL,
    };

    for (av = arches; *av != NULL; av++) {
	if (!strcmp(arch, *av))
	    return 1;
    }
    return 0;
}

/* keyp might no be defined. */
rpmdbMatchIterator rpmtsInitIterator(const rpmts ts, rpmTag rpmtag,
			const void * keyp, size_t keylen)
{
    rpmdbMatchIterator mi;
    const char * arch = NULL;
    int xx;

    if (ts->rdb == NULL && rpmtsOpenDB(ts, ts->dbmode))
	return NULL;

    /* Parse out "N(EVR).A" tokens from a label key. */
    if (rpmtag == RPMDBI_LABEL && keyp != NULL) {
	const char * s = keyp;
	const char *se;
	size_t slen = strlen(s);
	char *t = alloca(slen+1);
	int level = 0;
	int c;

	keyp = t;
	while ((c = *s++) != '\0') {
	    switch (c) {
	    default:
		*t++ = c;
		break;
	    case '(':
		/* XXX Fail if nested parens. */
		if (level++ != 0) {
		    rpmlog(RPMLOG_ERR, _("extra '(' in package label: %s\n"), (const char*)keyp);
		    return NULL;
		}
		/* Parse explicit epoch. */
		for (se = s; *se && risdigit(*se); se++)
		    {};
		if (*se == ':') {
		    /* XXX skip explicit epoch's (for now) */
		    *t++ = '-';
		    s = se + 1;
		} else {
		    /* No Epoch: found. Convert '(' to '-' and chug. */
		    *t++ = '-';
		}
		break;
	    case ')':
		/* XXX Fail if nested parens. */
		if (--level != 0) {
		    rpmlog(RPMLOG_ERR, _("missing '(' in package label: %s\n"), (const char*)keyp);
		    return NULL;
		}
		/* Don't copy trailing ')' */
		break;
	    }
	}
	if (level) {
	    rpmlog(RPMLOG_ERR, _("missing ')' in package label: %s\n"), (const char*)keyp);
	    return NULL;
	}
	*t = '\0';
	t = (char *) keyp;
	t = strrchr(t, '.');
	/* Is this a valid ".arch" suffix? */
	if (t != NULL && isArch(t+1)) {
	   *t++ = '\0';
	   arch = t;
	}
    }

    mi = rpmdbInitIterator(ts->rdb, rpmtag, keyp, keylen);

    /* Verify header signature/digest during retrieve (if not disabled). */
    if (mi && !(ts->vsflags & RPMVSF_NOHDRCHK))
	(void) rpmdbSetHdrChk(mi, ts, headerCheck);

    /* Select specified arch only. */
    if (arch != NULL)
	xx = rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_DEFAULT, arch);
    return mi;
}

rpmRC rpmtsFindPubkey(rpmts ts)
{
    const void * sig = rpmtsSig(ts);
    pgpDig dig = rpmtsDig(ts);
    pgpDigParams sigp = rpmtsSignature(ts);
    pgpDigParams pubp = rpmtsPubkey(ts);
    rpmRC res = RPMRC_NOKEY;
    char * pubkeysource = NULL;
    int xx;

    if (sig == NULL || dig == NULL || sigp == NULL || pubp == NULL)
	goto exit;

#if 0
fprintf(stderr, "==> find sig id %08x %08x ts pubkey id %08x %08x\n",
pgpGrab(sigp->signid, 4), pgpGrab(sigp->signid+4, 4),
pgpGrab(ts->pksignid, 4), pgpGrab(ts->pksignid+4, 4));
#endif

    /* Lazy free of previous pubkey if pubkey does not match this signature. */
    if (memcmp(sigp->signid, ts->pksignid, sizeof(ts->pksignid))) {
#if 0
fprintf(stderr, "*** free pkt %p[%d] id %08x %08x\n", ts->pkpkt, ts->pkpktlen, pgpGrab(ts->pksignid, 4), pgpGrab(ts->pksignid+4, 4));
#endif
	ts->pkpkt = _free(ts->pkpkt);
	ts->pkpktlen = 0;
	memset(ts->pksignid, 0, sizeof(ts->pksignid));
    }

    /* Try rpmdb keyring lookup. */
    if (ts->pkpkt == NULL) {
	int hx = -1;
	int ix = -1;
	rpmdbMatchIterator mi;
	Header h;

	/* Retrieve the pubkey that matches the signature. */
	mi = rpmtsInitIterator(ts, RPMTAG_PUBKEYS, sigp->signid, sizeof(sigp->signid));
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    const char ** pubkeys;
	    rpmTagType pt;
	    rpm_count_t pc;

	    if (!headerGetEntry(h, RPMTAG_PUBKEYS, &pt, (rpm_data_t *)&pubkeys, &pc))
		continue;
	    hx = rpmdbGetIteratorOffset(mi);
	    ix = rpmdbGetIteratorFileNum(mi);
	    if (ix >= pc
	     || b64decode(pubkeys[ix], (void **) &ts->pkpkt, &ts->pkpktlen))
		ix = -1;
	    pubkeys = headerFreeData(pubkeys, pt);
	    break;
	}
	mi = rpmdbFreeIterator(mi);

	if (ix >= 0) {
	    char hnum[32];
	    sprintf(hnum, "h#%d", hx);
	    pubkeysource = xstrdup(hnum);
	} else {
	    ts->pkpkt = _free(ts->pkpkt);
	    ts->pkpktlen = 0;
	}
    }

    /* Was a matching pubkey found? */
    if (ts->pkpkt == NULL || ts->pkpktlen == 0)
	goto exit;

    /* Retrieve parameters from pubkey packet(s). */
    xx = pgpPrtPkts(ts->pkpkt, ts->pkpktlen, dig, 0);

    /* Do the parameters match the signature? */
    if (sigp->pubkey_algo == pubp->pubkey_algo
#ifdef	NOTYET
     && sigp->hash_algo == pubp->hash_algo
#endif
     &&	!memcmp(sigp->signid, pubp->signid, sizeof(sigp->signid)) )
    {

	/* XXX Verify any pubkey signatures. */

	/* Pubkey packet looks good, save the signer id. */
	memcpy(ts->pksignid, pubp->signid, sizeof(ts->pksignid));

	if (pubkeysource)
	    rpmlog(RPMLOG_DEBUG, "========== %s pubkey id %08x %08x (%s)\n",
		(sigp->pubkey_algo == PGPPUBKEYALGO_DSA ? "DSA" :
		(sigp->pubkey_algo == PGPPUBKEYALGO_RSA ? "RSA" : "???")),
		pgpGrab(sigp->signid, 4), pgpGrab(sigp->signid+4, 4),
		pubkeysource);

	res = RPMRC_OK;
    }

exit:
    pubkeysource = _free(pubkeysource);
    if (res != RPMRC_OK) {
	ts->pkpkt = _free(ts->pkpkt);
	ts->pkpktlen = 0;
    }
    return res;
}

rpmRC rpmtsImportPubkey(const rpmts ts, const unsigned char * pkt, size_t pktlen)
{
    static unsigned char zeros[] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const char * afmt = "%{pubkeys:armor}";
    const char * group = "Public Keys";
    const char * license = "pubkey";
    const char * buildhost = "localhost";
    rpmsenseFlags pflags = (RPMSENSE_KEYRING|RPMSENSE_EQUAL);
    int32_t zero = 0;
    pgpDig dig = NULL;
    pgpDigParams pubp = NULL;
    char * d = NULL;
    char * enc = NULL;
    char * n = NULL;
    char * u = NULL;
    char * v = NULL;
    char * r = NULL;
    char * evr = NULL;
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;

    if (pkt == NULL || pktlen == 0)
	return RPMRC_FAIL;
    if (rpmtsOpenDB(ts, (O_RDWR|O_CREAT)))
	return RPMRC_FAIL;

    if ((enc = b64encode(pkt, pktlen, -1)) == NULL)
	goto exit;

    dig = pgpNewDig();

    /* Build header elements. */
    (void) pgpPrtPkts(pkt, pktlen, dig, 0);
    pubp = &dig->pubkey;

    if (!memcmp(pubp->signid, zeros, sizeof(pubp->signid))
     || !memcmp(pubp->time, zeros, sizeof(pubp->time))
     || pubp->userid == NULL)
	goto exit;

    v = pgpHexStr(pubp->signid, sizeof(pubp->signid)); 
    r = pgpHexStr(pubp->time, sizeof(pubp->time));

    rasprintf(&n, "gpg(%s)", v+8);
    rasprintf(&u, "gpg(%s)", pubp->userid ? pubp->userid : "none");
    rasprintf(&evr, "%d:%s-%s", pubp->version, v, r);
    /* Check for pre-existing header. */

    /* Build pubkey header. */
    h = headerNew();

    xx = headerAddOrAppendEntry(h, RPMTAG_PUBKEYS,
			RPM_STRING_ARRAY_TYPE, &enc, 1);

    d = headerSprintf(h, afmt, rpmTagTable, rpmHeaderFormats, NULL);
    if (d == NULL)
	goto exit;

    xx = headerAddEntry(h, RPMTAG_NAME, RPM_STRING_TYPE, "gpg-pubkey", 1);
    xx = headerAddEntry(h, RPMTAG_VERSION, RPM_STRING_TYPE, v+8, 1);
    xx = headerAddEntry(h, RPMTAG_RELEASE, RPM_STRING_TYPE, r, 1);
    xx = headerAddEntry(h, RPMTAG_DESCRIPTION, RPM_STRING_TYPE, d, 1);
    xx = headerAddEntry(h, RPMTAG_GROUP, RPM_STRING_TYPE, group, 1);
    xx = headerAddEntry(h, RPMTAG_LICENSE, RPM_STRING_TYPE, license, 1);
    xx = headerAddEntry(h, RPMTAG_SUMMARY, RPM_STRING_TYPE, u, 1);

    xx = headerAddEntry(h, RPMTAG_SIZE, RPM_INT32_TYPE, &zero, 1);

    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME,
			RPM_STRING_ARRAY_TYPE, &u, 1);
    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION,
			RPM_STRING_ARRAY_TYPE, &evr, 1);
    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS,
			RPM_INT32_TYPE, &pflags, 1);

    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME,
			RPM_STRING_ARRAY_TYPE, &n, 1);
    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION,
			RPM_STRING_ARRAY_TYPE, &evr, 1);
    xx = headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS,
			RPM_INT32_TYPE, &pflags, 1);

    xx = headerAddEntry(h, RPMTAG_RPMVERSION, RPM_STRING_TYPE, RPMVERSION, 1);

    /* XXX W2DO: tag value inheirited from parent? */
    xx = headerAddEntry(h, RPMTAG_BUILDHOST, RPM_STRING_TYPE, buildhost, 1);
    {   rpm_tid_t tid = rpmtsGetTid(ts);
	xx = headerAddEntry(h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &tid, 1);
	/* XXX W2DO: tag value inheirited from parent? */
	xx = headerAddEntry(h, RPMTAG_BUILDTIME, RPM_INT32_TYPE, &tid, 1);
    }

#ifdef	NOTYET
    /* XXX W2DO: tag value inheirited from parent? */
    xx = headerAddEntry(h, RPMTAG_SOURCERPM, RPM_STRING_TYPE, fn, 1);
#endif

    /* Add header to database. */
    xx = rpmdbAdd(rpmtsGetRdb(ts), rpmtsGetTid(ts), h, NULL, NULL);
    if (xx != 0)
	goto exit;
    rc = RPMRC_OK;

exit:
    /* Clean up. */
    h = headerFree(h);
    dig = pgpFreeDig(dig);
    n = _free(n);
    u = _free(u);
    v = _free(v);
    r = _free(r);
    evr = _free(evr);
    enc = _free(enc);
    d = _free(d);
    
    return rc;
}

int rpmtsCloseSDB(rpmts ts)
{
    int rc = 0;

    if (ts->sdb != NULL) {
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBGET), 
			rpmdbOp(ts->sdb, RPMDB_OP_DBGET));
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBPUT),
			rpmdbOp(ts->sdb, RPMDB_OP_DBPUT));
	(void) rpmswAdd(rpmtsOp(ts, RPMTS_OP_DBDEL),
			rpmdbOp(ts->sdb, RPMDB_OP_DBDEL));
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
	char * dn = rpmGetPath(ts->rootDir, "%{_dbpath}", NULL);
	rpmlog(RPMLOG_WARNING,
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
{
    const char * astr = *(const char **)a;
    const char * bstr = *(const char **)b;
    return strcmp(astr, bstr);
}

int rpmtsSolve(rpmts ts, rpmds ds, const void * data)
{
    const char * errstr;
    char * str;
    char * qfmt;
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
	int32_t * ip;

	if (rpmtag == RPMTAG_PROVIDENAME && !rpmdsAnyMatchesDep(h, ds, 1))
	    continue;

	/* XXX Prefer the shortest name if given alternatives. */
	hname = NULL;
	hnamelen = 0;
	if (headerGetEntry(h, RPMTAG_NAME, NULL, (rpm_data_t *)&hname, NULL)) {
	    if (hname)
		hnamelen = strlen(hname);
	}
	if (bhnamelen > 0 && hnamelen > bhnamelen)
	    continue;

	/* XXX Prefer the newest build if given alternatives. */
	htime = 0;
	if (headerGetEntry(h, RPMTAG_BUILDTIME, NULL, (rpm_data_t *)&ip, NULL))
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
	rpmlog(RPMLOG_ERR, _("incorrect format: %s\n"), errstr);
	goto exit;
    }

    if (ts->transFlags & RPMTRANS_FLAG_ADDINDEPS) {
	FD_t fd;
	rpmRC rpmrc;

	h = headerFree(h);
	fd = Fopen(str, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), str,
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
		rpmlog(RPMLOG_DEBUG, "Adding: %s\n", str);
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

    rpmlog(RPMLOG_DEBUG, "Suggesting: %s\n", str);
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
/* FIX: ts->suggests[] may be NULL */
    return rc;
}

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
/* FIX: ts->suggests[] may be NULL */
    return rc;
}

int rpmtsSetSolveCallback(rpmts ts,
		int (*solve) (rpmts ts, rpmds key, const void * data),
		const void * solveData)
{
    int rc = 0;

    if (ts) {
	ts->solve = solve;
	ts->solveData = solveData;
    }
    return rc;
}

void rpmtsPrintSuggests(rpmts ts)
{
    if (ts->suggests != NULL && ts->nsuggests > 0) {
	int i;
	rpmlog(RPMLOG_NOTICE, _("    Suggested resolutions:\n"));
	for (i = 0; i < ts->nsuggests; i++) {
	    const char * str = ts->suggests[i];

	    if (str == NULL)
		break;

	    rpmlog(RPMLOG_NOTICE, "\t%s\n", str);
	}
    }
}


rpmps rpmtsProblems(rpmts ts)
{
    rpmps ps = NULL;
    if (ts) {
	if (ts->probs)
	    ps = rpmpsLink(ts->probs, RPMDBG_M("rpmtsProblems"));
    }
    return ps;
}

void rpmtsCleanProblems(rpmts ts)
{
    if (ts && ts->probs) {
	ts->probs = rpmpsFree(ts->probs);
    }
}

void rpmtsCleanDig(rpmts ts)
{
    ts->sig = headerFreeData(ts->sig, ts->sigtype);
    ts->dig = pgpFreeDig(ts->dig);
}

void rpmtsClean(rpmts ts)
{
    rpmtsi pi; rpmte p;
    int i;

    if (ts == NULL)
	return;

    /* Clean up after dependency checks. */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteCleanDS(p);
    pi = rpmtsiFree(pi);

    ts->addedPackages = rpmalFree(ts->addedPackages);
    ts->numAddedPackages = 0;

    for (i = 0; i < ts->nsuggests; i++) {
	const char * str = ts->suggests[i];
	ts->suggests[i] = NULL;
	_constfree(str);
    }
    ts->suggests = _free(ts->suggests);
    ts->nsuggests = 0;

    rpmtsCleanProblems(ts);

    rpmtsCleanDig(ts);
}

void rpmtsEmpty(rpmts ts)
{
    rpmtsi pi; rpmte p;
    int oc;

    if (ts == NULL)
	return;

    rpmtsClean(ts);

    for (pi = rpmtsiInit(ts), oc = 0; (p = rpmtsiNext(pi, 0)) != NULL; oc++) {
	ts->order[oc] = rpmteFree(ts->order[oc]);
    }
    pi = rpmtsiFree(pi);

    ts->orderCount = 0;
    ts->ntrees = 0;
    ts->maxDepth = 0;

    ts->numRemovedPackages = 0;
    return;
}

static void rpmtsPrintStat(const char * name, struct rpmop_s * op)
{
    static const unsigned int scale = (1000 * 1000);
    if (op != NULL && op->count > 0)
	fprintf(stderr, "   %s %6d %6lu.%06lu MB %6lu.%06lu secs\n",
		name, op->count,
		(unsigned long)op->bytes/scale, (unsigned long)op->bytes%scale,
		op->usecs/scale, op->usecs%scale);
}

static void rpmtsPrintStats(rpmts ts)
{
    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_TOTAL), 0);

    rpmtsPrintStat("total:       ", rpmtsOp(ts, RPMTS_OP_TOTAL));
    rpmtsPrintStat("check:       ", rpmtsOp(ts, RPMTS_OP_CHECK));
    rpmtsPrintStat("order:       ", rpmtsOp(ts, RPMTS_OP_ORDER));
    rpmtsPrintStat("fingerprint: ", rpmtsOp(ts, RPMTS_OP_FINGERPRINT));
    rpmtsPrintStat("install:     ", rpmtsOp(ts, RPMTS_OP_INSTALL));
    rpmtsPrintStat("erase:       ", rpmtsOp(ts, RPMTS_OP_ERASE));
    rpmtsPrintStat("scriptlets:  ", rpmtsOp(ts, RPMTS_OP_SCRIPTLETS));
    rpmtsPrintStat("compress:    ", rpmtsOp(ts, RPMTS_OP_COMPRESS));
    rpmtsPrintStat("uncompress:  ", rpmtsOp(ts, RPMTS_OP_UNCOMPRESS));
    rpmtsPrintStat("digest:      ", rpmtsOp(ts, RPMTS_OP_DIGEST));
    rpmtsPrintStat("signature:   ", rpmtsOp(ts, RPMTS_OP_SIGNATURE));
    rpmtsPrintStat("dbadd:       ", rpmtsOp(ts, RPMTS_OP_DBADD));
    rpmtsPrintStat("dbremove:    ", rpmtsOp(ts, RPMTS_OP_DBREMOVE));
    rpmtsPrintStat("dbget:       ", rpmtsOp(ts, RPMTS_OP_DBGET));
    rpmtsPrintStat("dbput:       ", rpmtsOp(ts, RPMTS_OP_DBPUT));
    rpmtsPrintStat("dbdel:       ", rpmtsOp(ts, RPMTS_OP_DBDEL));
}

rpmts rpmtsFree(rpmts ts)
{
    if (ts == NULL)
	return NULL;

    if (ts->nrefs > 1)
	return rpmtsUnlink(ts, RPMDBG_M("tsCreate"));

    rpmtsEmpty(ts);

    (void) rpmtsCloseDB(ts);

    (void) rpmtsCloseSDB(ts);

    ts->removedPackages = _free(ts->removedPackages);

    ts->availablePackages = rpmalFree(ts->availablePackages);
    ts->numAvailablePackages = 0;

    ts->dsi = _free(ts->dsi);

    if (ts->scriptFd != NULL) {
	ts->scriptFd = fdFree(ts->scriptFd, RPMDBG_M("rpmtsFree"));
	ts->scriptFd = NULL;
    }
    ts->rootDir = _free(ts->rootDir);
    ts->currDir = _free(ts->currDir);

    ts->order = _free(ts->order);
    ts->orderAlloced = 0;

    if (ts->pkpkt != NULL)
	ts->pkpkt = _free(ts->pkpkt);
    ts->pkpktlen = 0;
    memset(ts->pksignid, 0, sizeof(ts->pksignid));

    if (_rpmts_stats)
	rpmtsPrintStats(ts);

    (void) rpmtsUnlink(ts, RPMDBG_M("tsCreate"));

    ts = _free(ts);

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
    const char * rootDir = NULL;

    if (ts != NULL && ts->rootDir != NULL) {
	urltype ut = urlPath(ts->rootDir, &rootDir);
	switch (ut) {
	case URL_IS_UNKNOWN:
	case URL_IS_PATH:
	    break;
	/* XXX these shouldn't be allowed as rootdir! */
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_HKP:
	case URL_IS_FTP:
	case URL_IS_DASH:
	default:
	    rootDir = "/";
	    break;
	}
    }
    return rootDir;
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

	/* Make sure that rootDir has trailing / */
	if (!(rootLen && rootDir[rootLen - 1] == '/')) {
	    char * t = alloca(rootLen + 2);
	    *t = '\0';
	    (void) stpcpy( stpcpy(t, rootDir), "/");
	    rootDir = t;
	}
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
    return scriptFd;
}

void rpmtsSetScriptFd(rpmts ts, FD_t scriptFd)
{

    if (ts != NULL) {
	if (ts->scriptFd != NULL) {
	    ts->scriptFd = fdFree(ts->scriptFd, 
				  RPMDBG_M("rpmtsSetScriptFd"));
	    ts->scriptFd = NULL;
	}
	if (scriptFd != NULL)
	    ts->scriptFd = fdLink((void *)scriptFd, 
				  RPMDBG_M("rpmtsSetScriptFd"));
    }
}

int rpmtsSELinuxEnabled(rpmts ts)
{
    return (ts != NULL ? (ts->selinuxEnabled > 0) : 0);
}

int rpmtsChrootDone(rpmts ts)
{
    return (ts != NULL ? ts->chrootDone : 0);
}

int rpmtsSetChrootDone(rpmts ts, int chrootDone)
{
    int ochrootDone = 0;
    if (ts != NULL) {
	ochrootDone = ts->chrootDone;
	rpmdbSetChrootDone(rpmtsGetRdb(ts), chrootDone);
	ts->chrootDone = chrootDone;
    }
    return ochrootDone;
}

rpm_tid_t rpmtsGetTid(rpmts ts)
{
    rpm_tid_t tid = -1;  /* XXX -1 is time(2) error return. */
    if (ts != NULL) {
	tid = ts->tid;
    }
    return tid;
}

rpm_tid_t rpmtsSetTid(rpmts ts, rpm_tid_t tid)
{
    rpm_tid_t otid = -1; /* XXX -1 is time(2) error return. */
    if (ts != NULL) {
	otid = ts->tid;
	ts->tid = tid;
    }
    return otid;
}

rpmSigTag rpmtsSigtag(const rpmts ts)
{
    rpmSigTag sigtag = 0;
    if (ts != NULL)
	sigtag = ts->sigtag;
    return sigtag;
}

rpmTagType rpmtsSigtype(const rpmts ts)
{
    rpmTagType sigtype = 0;
    if (ts != NULL)
	sigtype = ts->sigtype;
    return sigtype;
}

rpm_constdata_t rpmtsSig(const rpmts ts)
{
    rpm_constdata_t sig = NULL;
    if (ts != NULL)
	sig = ts->sig;
    return sig;
}

size_t rpmtsSiglen(const rpmts ts)
{
    size_t siglen = 0;
    if (ts != NULL)
	siglen = ts->siglen;
    return siglen;
}

int rpmtsSetSig(rpmts ts, rpmSigTag sigtag, rpmTagType sigtype, 
		rpm_data_t sig, size_t siglen)
{
    if (ts != NULL) {
	if (ts->sig && ts->sigtype)
	    ts->sig = headerFreeData(ts->sig, ts->sigtype);
	ts->sigtag = sigtag;
	ts->sigtype = (sig ? sigtype : 0);
	ts->sig = sig;
	ts->siglen = siglen;
    }
    return 0;
}

pgpDig rpmtsDig(rpmts ts)
{
/* FIX: hide lazy malloc for now */
    if (ts->dig == NULL)
	ts->dig = pgpNewDig();
    if (ts->dig == NULL)
	return NULL;
    return ts->dig;
}

pgpDigParams rpmtsSignature(const rpmts ts)
{
    pgpDig dig = rpmtsDig(ts);
    if (dig == NULL) return NULL;
    return &dig->signature;
}

pgpDigParams rpmtsPubkey(const rpmts ts)
{
    pgpDig dig = rpmtsDig(ts);
    if (dig == NULL) return NULL;
    return &dig->pubkey;
}

rpmdb rpmtsGetRdb(rpmts ts)
{
    rpmdb rdb = NULL;
    if (ts != NULL) {
	rdb = ts->rdb;
    }
    return rdb;
}

int rpmtsInitDSI(const rpmts ts)
{
    rpmDiskSpaceInfo dsi;
    struct stat sb;
    int rc;
    int i;

    if (rpmtsFilterFlags(ts) & RPMPROB_FILTER_DISKSPACE)
	return 0;

    rpmlog(RPMLOG_DEBUG, "mounted filesystems:\n");
    rpmlog(RPMLOG_DEBUG,
	"    i        dev    bsize       bavail       iavail mount point\n");

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
	/* XXX assigning negative value to unsigned type */
	dsi->iavail = !(sfb.f_ffree == 0 && sfb.f_files == 0)
				? sfb.f_ffree : -1;
	rpmlog(RPMLOG_DEBUG, "%5d 0x%08x %8u %12ld %12ld %s\n",
		i, (unsigned) dsi->dev, (unsigned) dsi->bsize,
		(signed long) dsi->bavail, (signed long) dsi->iavail,
		ts->filesystems[i]);
    }
    return rc;
}

void rpmtsUpdateDSI(const rpmts ts, dev_t dev,
		rpm_off_t fileSize, rpm_off_t prevSize, rpm_off_t fixupSize,
		rpmFileAction action)
{
    rpmDiskSpaceInfo dsi;
    uint32_t bneeded;

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
	break;

    /*
     * FIXME: If two packages share a file (same md5sum), and
     * that file is being replaced on disk, will dsi->bneeded get
     * adjusted twice? Quite probably!
     */
    case FA_CREATE:
	dsi->bneeded += bneeded;
	dsi->bneeded -= BLOCK_ROUND(prevSize, dsi->bsize);
	break;

    case FA_ERASE:
	dsi->ineeded--;
	dsi->bneeded -= bneeded;
	break;

    default:
	break;
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

	if (dsi->bavail >= 0 && adj_fs_blocks(dsi->bneeded) > dsi->bavail) {
	    rpmpsAppend(ps, RPMPROB_DISKSPACE,
			rpmteNEVRA(te), rpmteKey(te),
			ts->filesystems[i], NULL, NULL,
 	   (adj_fs_blocks(dsi->bneeded) - dsi->bavail) * dsi->bsize);
	}

	if (dsi->iavail >= 0 && adj_fs_blocks(dsi->ineeded) > dsi->iavail) {
	    rpmpsAppend(ps, RPMPROB_DISKNODES,
			rpmteNEVRA(te), rpmteKey(te),
			ts->filesystems[i], NULL, NULL,
 	    (adj_fs_blocks(dsi->ineeded) - dsi->iavail));
	}
    }
    ps = rpmpsFree(ps);
}

void * rpmtsNotify(rpmts ts, rpmte te,
		rpmCallbackType what, rpm_off_t amount, rpm_off_t total)
{
    void * ptr = NULL;
    if (ts && ts->notify && te) {
	Header h = rpmteHeader(te);
assert(!(rpmteType(te) == TR_ADDED && h == NULL));
	/* FIX: cast? */
	/* FIX: check rc */
	ptr = ts->notify(h, what, amount, total,
			rpmteKey(te), ts->notifyData);
	headerUnlink(h); /* undo rpmteHeader() ref */
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
    return te;
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

rpmSpec rpmtsSpec(rpmts ts)
{
    return ts->spec;
}

rpmSpec rpmtsSetSpec(rpmts ts, rpmSpec spec)
{
    rpmSpec ospec = ts->spec;
    ts->spec = spec;
    return ospec;
}

rpmte rpmtsRelocateElement(rpmts ts)
{
    return ts->relocateElement;
}

rpmte rpmtsSetRelocateElement(rpmts ts, rpmte relocateElement)
{
    rpmte orelocateElement = ts->relocateElement;
    ts->relocateElement = relocateElement;
    return orelocateElement;
}

rpm_color_t rpmtsColor(rpmts ts)
{
    return (ts != NULL ? ts->color : 0);
}

rpm_color_t rpmtsSetColor(rpmts ts, rpm_color_t color)
{
    rpm_color_t ocolor = 0;
    if (ts != NULL) {
	ocolor = ts->color;
	ts->color = color;
    }
    return ocolor;
}

rpm_color_t rpmtsPrefColor(rpmts ts)
{
    return (ts != NULL ? ts->prefcolor : 0);
}

rpmop rpmtsOp(rpmts ts, rpmtsOpX opx)
{
    rpmop op = NULL;

    if (ts != NULL && opx >= 0 && opx < RPMTS_OP_MAX)
	op = ts->ops + opx;
    return op;
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
		*e = rpmteKey(p);
		break;
	    case TR_REMOVED:
	    default:
		*e = NULL;
		break;
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
    memset(&ts->ops, 0, sizeof(ts->ops));
    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_TOTAL), -1);
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
    ts->tid = (rpm_tid_t) time(NULL);
    ts->delta = 5;

    ts->color = rpmExpandNumeric("%{?_transaction_color}");
    ts->prefcolor = rpmExpandNumeric("%{?_prefer_color}")?:2;

    ts->numRemovedPackages = 0;
    ts->allocedRemovedPackages = ts->delta;
    ts->removedPackages = xcalloc(ts->allocedRemovedPackages,
			sizeof(*ts->removedPackages));

    ts->rootDir = NULL;
    ts->currDir = NULL;
    ts->chrootDone = 0;

    ts->selinuxEnabled = is_selinux_enabled();

    ts->numAddedPackages = 0;
    ts->addedPackages = NULL;

    ts->numAvailablePackages = 0;
    ts->availablePackages = NULL;

    ts->orderAlloced = 0;
    ts->orderCount = 0;
    ts->order = NULL;
    ts->ntrees = 0;
    ts->maxDepth = 0;

    ts->probs = NULL;

    ts->sig = NULL;
    ts->pkpkt = NULL;
    ts->pkpktlen = 0;
    memset(ts->pksignid, 0, sizeof(ts->pksignid));
    ts->dig = NULL;

    ts->nrefs = 0;

    return rpmtsLink(ts, RPMDBG_M("tsCreate"));
}

