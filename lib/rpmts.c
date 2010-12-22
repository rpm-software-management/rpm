/** \ingroup rpmdep
 * \file lib/rpmts.c
 * Routine(s) to handle a "rpmts" transaction sets.
 */
#include "system.h"

#include <inttypes.h>
#include <libgen.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>			/* rpmReadPackage etc */
#include <rpm/rpmmacro.h>
#include <rpm/rpmfileutil.h>		/* rpmtsOpenDB() needs rpmGetPath */
#include <rpm/rpmstring.h>
#include <rpm/rpmkeyring.h>

#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmte.h>

#include "rpmio/digest.h"
#include "lib/rpmal.h"
#include "lib/rpmchroot.h"
#include "lib/rpmplugins.h"
#include "lib/rpmts_internal.h"
#include "lib/rpmte_internal.h"
#include "lib/misc.h"

#include "debug.h"

/**
 * Iterator across transaction elements, forward on install, backward on erase.
 */
struct rpmtsi_s {
    rpmts ts;		/*!< transaction set. */
    int oc;		/*!< iterator index. */
};

static void loadKeyring(rpmts ts);

int _rpmts_stats = 0;

static rpmts rpmtsUnlink(rpmts ts)
{
    if (ts)
	ts->nrefs--;
    return NULL;
}

rpmts rpmtsLink(rpmts ts)
{
    if (ts)
	ts->nrefs++;
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
    rpmlock lock = rpmtsAcquireLock(ts);
    int rc = -1;
    if (lock)
	    rc = rpmdbInit(ts->rootDir, dbmode);
    rpmlockFree(lock);
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
    int rc = -1;
    rpmlock lock = NULL;

    /* Cannot do this on a populated transaction set */
    if (rpmtsNElements(ts) > 0)
	return -1;

    lock = rpmtsAcquireLock(ts);
    if (lock) {
	if (!(ts->vsflags & RPMVSF_NOHDRCHK))
	    rc = rpmdbRebuild(ts->rootDir, ts, headerCheck);
	else
	    rc = rpmdbRebuild(ts->rootDir, NULL, NULL);
	rpmlockFree(lock);
    }
    return rc;
}

int rpmtsVerifyDB(rpmts ts)
{
    int rc = -1;
    rpmlock lock = rpmtsAcquireLock(ts);
    if (lock) {
	rc = rpmdbVerify(ts->rootDir);
	rpmlockFree(lock);
    }
    return rc;
}

/* keyp might no be defined. */
rpmdbMatchIterator rpmtsInitIterator(const rpmts ts, rpmDbiTagVal rpmtag,
			const void * keyp, size_t keylen)
{
    rpmdbMatchIterator mi = NULL;
    const char * arch = NULL;
    char *tmp = NULL;
    int xx;

    if (ts == NULL)
	return NULL;

    if (ts && ts->keyring == NULL)
	loadKeyring(ts);

    if (ts->rdb == NULL && rpmtsOpenDB(ts, ts->dbmode))
	return NULL;

    /* Parse out "N(EVR).A" tokens from a label key. */
    if (rpmtag == RPMDBI_LABEL && keyp != NULL) {
	const char *se, *s = keyp;
	char *t;
	size_t slen = strlen(s);
	int level = 0;
	int c;

	tmp = xmalloc(slen+1);
	keyp = t = tmp;
	while ((c = *s++) != '\0') {
	    switch (c) {
	    default:
		*t++ = c;
		break;
	    case '(':
		/* XXX Fail if nested parens. */
		if (level++ != 0) {
		    rpmlog(RPMLOG_ERR, _("extra '(' in package label: %s\n"), (const char*)keyp);
		    goto exit;
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
		    goto exit;
		}
		/* Don't copy trailing ')' */
		break;
	    }
	}
	if (level) {
	    rpmlog(RPMLOG_ERR, _("missing ')' in package label: %s\n"), (const char*)keyp);
	    goto exit;
	}
	*t = '\0';
	t = (char *) keyp;
	t = strrchr(t, '.');
	/* Is this a valid ".arch" suffix? */
	if (t != NULL && rpmIsKnownArch(t+1)) {
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

exit:
    free(tmp);

    return mi;
}

rpmKeyring rpmtsGetKeyring(rpmts ts, int autoload)
{
    rpmKeyring keyring = NULL;
    if (ts) {
	if (ts->keyring == NULL && autoload) {
	    loadKeyring(ts);
	}
	keyring = rpmKeyringLink(ts->keyring);
    }
    return keyring;
}

int rpmtsSetKeyring(rpmts ts, rpmKeyring keyring)
{
    /*
     * Should we permit switching keyring on the fly? For now, require
     * rpmdb isn't open yet (fairly arbitrary limitation)...
     */
    if (ts == NULL || rpmtsGetRdb(ts) != NULL)
	return -1;

    rpmKeyringFree(ts->keyring);
    ts->keyring = rpmKeyringLink(keyring);
    return 0;
}

static int loadKeyringFromFiles(rpmts ts)
{
    ARGV_t files = NULL;
    /* XXX TODO: deal with chroot path issues */
    char *pkpath = rpmGetPath(ts->rootDir, "%{_keyringpath}/*.key", NULL);
    int nkeys = 0;

    rpmlog(RPMLOG_DEBUG, "loading keyring from pubkeys in %s\n", pkpath);
    if (rpmGlob(pkpath, NULL, &files)) {
	rpmlog(RPMLOG_DEBUG, "couldn't find any keys in %s\n", pkpath);
	goto exit;
    }

    for (char **f = files; *f; f++) {
	rpmPubkey key = rpmPubkeyRead(*f);
	if (!key) {
	    rpmlog(RPMLOG_ERR, _("%s: reading of public key failed.\n"), *f);
	    continue;
	}
	if (rpmKeyringAddKey(ts->keyring, key) == 0) {
	    nkeys++;
	    rpmlog(RPMLOG_DEBUG, "added key %s to keyring\n", *f);
	}
	rpmPubkeyFree(key);
    }
exit:
    free(pkpath);
    argvFree(files);
    return nkeys;
}

static int loadKeyringFromDB(rpmts ts)
{
    Header h;
    rpmdbMatchIterator mi;
    int nkeys = 0;

    rpmlog(RPMLOG_DEBUG, "loading keyring from rpmdb\n");
    mi = rpmtsInitIterator(ts, RPMDBI_NAME, "gpg-pubkey", 0);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	struct rpmtd_s pubkeys;
	const char *key;

	if (!headerGet(h, RPMTAG_PUBKEYS, &pubkeys, HEADERGET_MINMEM))
	   continue;

	while ((key = rpmtdNextString(&pubkeys))) {
	    uint8_t *pkt;
	    size_t pktlen;

	    if (b64decode(key, (void **) &pkt, &pktlen) == 0) {
		rpmPubkey key = rpmPubkeyNew(pkt, pktlen);
		if (rpmKeyringAddKey(ts->keyring, key) == 0) {
		    char *nvr = headerGetAsString(h, RPMTAG_NVR);
		    rpmlog(RPMLOG_DEBUG, "added key %s to keyring\n", nvr);
		    free(nvr);
		    nkeys++;
		}
		rpmPubkeyFree(key);
		free(pkt);
	    }
	}
	rpmtdFreeData(&pubkeys);
    }
    rpmdbFreeIterator(mi);

    return nkeys;
}

static void loadKeyring(rpmts ts)
{
    ts->keyring = rpmKeyringNew();
    if (loadKeyringFromFiles(ts) == 0) {
	if (loadKeyringFromDB(ts) > 0) {
	    /* XXX make this a warning someday... */
	    rpmlog(RPMLOG_DEBUG, "Using legacy gpg-pubkey(s) from rpmdb\n");
	}
    }
}

/* Build pubkey header. */
static int makePubkeyHeader(rpmts ts, rpmPubkey key, Header h)
{
    const char * afmt = "%{pubkeys:armor}";
    const char * group = "Public Keys";
    const char * license = "pubkey";
    const char * buildhost = "localhost";
    rpmsenseFlags pflags = (RPMSENSE_KEYRING|RPMSENSE_EQUAL);
    uint32_t zero = 0;
    pgpDig dig = NULL;
    pgpDigParams pubp = NULL;
    char * d = NULL;
    char * enc = NULL;
    char * n = NULL;
    char * u = NULL;
    char * v = NULL;
    char * r = NULL;
    char * evr = NULL;
    int rc = -1;

    if ((enc = rpmPubkeyBase64(key)) == NULL)
	goto exit;
    if ((dig = rpmPubkeyDig(key)) == NULL)
	goto exit;

    /* Build header elements. */
    pubp = &dig->pubkey;
    v = pgpHexStr(pubp->signid, sizeof(pubp->signid)); 
    r = pgpHexStr(pubp->time, sizeof(pubp->time));

    rasprintf(&n, "gpg(%s)", v+8);
    rasprintf(&u, "gpg(%s)", pubp->userid ? pubp->userid : "none");
    rasprintf(&evr, "%d:%s-%s", pubp->version, v, r);

    headerPutString(h, RPMTAG_PUBKEYS, enc);

    if ((d = headerFormat(h, afmt, NULL)) == NULL)
	goto exit;

    headerPutString(h, RPMTAG_NAME, "gpg-pubkey");
    headerPutString(h, RPMTAG_VERSION, v+8);
    headerPutString(h, RPMTAG_RELEASE, r);
    headerPutString(h, RPMTAG_DESCRIPTION, d);
    headerPutString(h, RPMTAG_GROUP, group);
    headerPutString(h, RPMTAG_LICENSE, license);
    headerPutString(h, RPMTAG_SUMMARY, u);

    headerPutUint32(h, RPMTAG_SIZE, &zero, 1);

    headerPutString(h, RPMTAG_PROVIDENAME, u);
    headerPutString(h, RPMTAG_PROVIDEVERSION, evr);
    headerPutUint32(h, RPMTAG_PROVIDEFLAGS, &pflags, 1);
	
    headerPutString(h, RPMTAG_PROVIDENAME, n);
    headerPutString(h, RPMTAG_PROVIDEVERSION, evr);
    headerPutUint32(h, RPMTAG_PROVIDEFLAGS, &pflags, 1);

    headerPutString(h, RPMTAG_RPMVERSION, RPMVERSION);
    headerPutString(h, RPMTAG_BUILDHOST, buildhost);
    headerPutString(h, RPMTAG_SOURCERPM, "(none)");

    {   rpm_tid_t tid = rpmtsGetTid(ts);
	headerPutUint32(h, RPMTAG_INSTALLTIME, &tid, 1);
	headerPutUint32(h, RPMTAG_INSTALLTID, &tid, 1);
	headerPutUint32(h, RPMTAG_BUILDTIME, &tid, 1);
    }
    rc = 0;

exit:
    pgpFreeDig(dig);
    free(n);
    free(u);
    free(v);
    free(r);
    free(evr);
    free(enc);
    free(d);

    return rc;
}

rpmRC rpmtsImportPubkey(const rpmts ts, const unsigned char * pkt, size_t pktlen)
{
    Header h = headerNew();
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    rpmPubkey pubkey = NULL;
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
    int krc;

    if ((pubkey = rpmPubkeyNew(pkt, pktlen)) == NULL)
	goto exit;
    krc = rpmKeyringAddKey(keyring, pubkey);
    if (krc < 0)
	goto exit;

    /* If we dont already have the key, make a persistent record of it */
    if (krc == 0) {
	if (makePubkeyHeader(ts, pubkey, h) != 0) 
	    goto exit;

	/* Add header to database. */
	if (rpmtsOpenDB(ts, (O_RDWR|O_CREAT)))
	    goto exit;
	if (rpmdbAdd(rpmtsGetRdb(ts), h) != 0)
	    goto exit;
    }
    rc = RPMRC_OK;

exit:
    /* Clean up. */
    headerFree(h);
    rpmPubkeyFree(pubkey);
    rpmKeyringFree(keyring);
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

int rpmtsSolve(rpmts ts, rpmds key)
{
    int rc = 1; /* assume not found */
    if (ts && ts->solve) {
	rc = (*ts->solve)(ts, key, ts->solveData);
    }
    return rc;
}

rpmps rpmtsProblems(rpmts ts)
{
    rpmps ps = rpmpsCreate();
    rpmtsi pi = rpmtsiInit(ts);
    rpmte p;

    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	rpmps teprobs = rpmteProblems(p);
	rpmpsMerge(ps, teprobs);
	rpmpsFree(teprobs);
    }
    pi = rpmtsiFree(pi);

    /* Return NULL on no problems instead of an empty set */
    if (rpmpsNumProblems(ps) == 0) {
	ps = rpmpsFree(ps);
    }

    return ps;
}

void rpmtsCleanProblems(rpmts ts)
{
    rpmte p;
    rpmtsi pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteCleanProblems(p);
    pi = rpmtsiFree(pi);
}

void rpmtsClean(rpmts ts)
{
    rpmtsi pi; rpmte p;
    tsMembers tsmem = rpmtsMembers(ts);

    if (ts == NULL)
	return;

    /* Clean up after dependency checks. */
    pi = rpmtsiInit(ts);
    while ((p = rpmtsiNext(pi, 0)) != NULL)
	rpmteCleanDS(p);
    pi = rpmtsiFree(pi);

    tsmem->addedPackages = rpmalFree(tsmem->addedPackages);

    rpmtsCleanProblems(ts);
}

/* hash comparison function */
static int uintCmp(unsigned int a, unsigned int b)
{
    return (a != b);
}

/* "hash"function*/ 
static unsigned int uintId(unsigned int a)
{
    return a;
}

void rpmtsEmpty(rpmts ts)
{
    tsMembers tsmem = rpmtsMembers(ts);
    if (ts == NULL)
	return;

    rpmtsClean(ts);

    for (int oc = 0; oc < tsmem->orderCount; oc++) {
	tsmem->order[oc] = rpmteFree(tsmem->order[oc]);
    }

    tsmem->orderCount = 0;
    intHashEmpty(tsmem->removedPackages);
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
    tsMembers tsmem = rpmtsMembers(ts);
    if (ts == NULL)
	return NULL;

    if (ts->nrefs > 1)
	return rpmtsUnlink(ts);

    rpmtsEmpty(ts);

    (void) rpmtsCloseDB(ts);

    tsmem->removedPackages = intHashFree(tsmem->removedPackages);
    tsmem->order = _free(tsmem->order);
    ts->members = _free(ts->members);

    ts->dsi = _free(ts->dsi);

    if (ts->scriptFd != NULL) {
	ts->scriptFd = fdFree(ts->scriptFd);
	ts->scriptFd = NULL;
    }
    ts->rootDir = _free(ts->rootDir);
    ts->lockPath = _free(ts->lockPath);

    ts->keyring = rpmKeyringFree(ts->keyring);
    ts->netsharedPaths = argvFree(ts->netsharedPaths);
    ts->installLangs = argvFree(ts->installLangs);

    ts->plugins = rpmpluginsFree(ts->plugins);

    if (_rpmts_stats)
	rpmtsPrintStats(ts);

    (void) rpmtsUnlink(ts);

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

const char * rpmtsRootDir(rpmts ts)
{
    return ts ? ts->rootDir : NULL;
}

int rpmtsSetRootDir(rpmts ts, const char * rootDir)
{
    if (ts == NULL || (rootDir && rootDir[0] != '/')) {
	return -1;
    }

    ts->rootDir = _free(ts->rootDir);
    /* Ensure clean path with a trailing slash */
    ts->rootDir = rootDir ? rpmGetPath(rootDir, NULL) : xstrdup("/");
    if (!rstreq(ts->rootDir, "/")) {
	rstrcat(&ts->rootDir, "/");
    }
    return 0;
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
	    ts->scriptFd = fdFree(ts->scriptFd);
	    ts->scriptFd = NULL;
	}
	if (scriptFd != NULL)
	    ts->scriptFd = fdLink(scriptFd);
    }
}

struct selabel_handle * rpmtsSELabelHandle(rpmts ts)
{
#if WITH_SELINUX
    if (ts != NULL) {
	return ts->selabelHandle;
    }
#endif
    return NULL;
}

rpmRC rpmtsSELabelInit(rpmts ts, const char *path)
{
#if WITH_SELINUX
    if (ts == NULL || path == NULL) {
	return RPMRC_FAIL;
    }

    struct selinux_opt opts[] = {
	{SELABEL_OPT_PATH, path}
    };

    if (ts->selabelHandle) {
	rpmtsSELabelFini(ts);
    }
    ts->selabelHandle = selabel_open(SELABEL_CTX_FILE, opts, 1);

    if (!ts->selabelHandle) {
	return RPMRC_FAIL;
    }
#endif
    return RPMRC_OK;
}

void rpmtsSELabelFini(rpmts ts)
{
#if WITH_SELINUX
    if (ts && ts->selabelHandle) {
	selabel_close(ts->selabelHandle);
	ts->selabelHandle = NULL;
    }
#endif
}

rpm_tid_t rpmtsGetTid(rpmts ts)
{
    rpm_tid_t tid = (rpm_tid_t)-1;  /* XXX -1 is time(2) error return. */
    if (ts != NULL) {
	tid = ts->tid;
    }
    return tid;
}

rpm_tid_t rpmtsSetTid(rpmts ts, rpm_tid_t tid)
{
    rpm_tid_t otid = (rpm_tid_t)-1; /* XXX -1 is time(2) error return. */
    if (ts != NULL) {
	otid = ts->tid;
	ts->tid = tid;
    }
    return otid;
}

rpmdb rpmtsGetRdb(rpmts ts)
{
    rpmdb rdb = NULL;
    if (ts != NULL) {
	rdb = ts->rdb;
    }
    return rdb;
}

void * rpmtsNotify(rpmts ts, rpmte te,
		rpmCallbackType what, rpm_loff_t amount, rpm_loff_t total)
{
    void * ptr = NULL;
    if (ts && ts->notify) {
	Header h = NULL;
	fnpyKey cbkey = NULL;
	if (te) {
	    h = rpmteHeader(te);
	    cbkey = rpmteKey(te);
	}
	ptr = ts->notify(h, what, amount, total, cbkey, ts->notifyData);

	if (h) {
	    headerFree(h); /* undo rpmteHeader() ref */
	}
    }
    return ptr;
}

int rpmtsNElements(rpmts ts)
{
    int nelements = 0;
    tsMembers tsmem = rpmtsMembers(ts);
    if (tsmem != NULL && tsmem->order != NULL) {
	nelements = tsmem->orderCount;
    }
    return nelements;
}

rpmte rpmtsElement(rpmts ts, int ix)
{
    rpmte te = NULL;
    tsMembers tsmem = rpmtsMembers(ts);
    if (tsmem != NULL && tsmem->order != NULL) {
	if (ix >= 0 && ix < tsmem->orderCount)
	    te = tsmem->order[ix];
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

rpm_color_t rpmtsSetPrefColor(rpmts ts, rpm_color_t color)
{
    rpm_color_t ocolor = 0;
    if (ts != NULL) {
	ocolor = ts->prefcolor;
	ts->prefcolor = color;
    }
    return ocolor;
}

rpmop rpmtsOp(rpmts ts, rpmtsOpX opx)
{
    rpmop op = NULL;

    if (ts != NULL && opx >= 0 && opx < RPMTS_OP_MAX)
	op = ts->ops + opx;
    return op;
}

rpmPlugins rpmtsPlugins(rpmts ts)
{
    return (ts != NULL ? ts->plugins : NULL);
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

tsMembers rpmtsMembers(rpmts ts)
{
    return (ts != NULL) ? ts->members : NULL;
}

rpmts rpmtsCreate(void)
{
    rpmts ts;
    tsMembers tsmem;

    ts = xcalloc(1, sizeof(*ts));
    memset(&ts->ops, 0, sizeof(ts->ops));
    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_TOTAL), -1);
    ts->dsi = NULL;

    ts->solve = NULL;
    ts->solveData = NULL;

    ts->rdb = NULL;
    ts->dbmode = O_RDONLY;

    ts->scriptFd = NULL;
    ts->tid = (rpm_tid_t) time(NULL);

    ts->color = rpmExpandNumeric("%{?_transaction_color}");
    ts->prefcolor = rpmExpandNumeric("%{?_prefer_color}")?:2;

    ts->netsharedPaths = NULL;
    ts->installLangs = NULL;
    {	char *tmp = rpmExpand("%{_netsharedpath}", NULL);
	if (tmp && *tmp != '%') {
	    argvSplit(&ts->netsharedPaths, tmp, ":");
	}
	free(tmp);

	tmp = rpmExpand("%{_install_langs}", NULL);
	if (tmp && *tmp != '%') {
	    ARGV_t langs = NULL;
	    argvSplit(&langs, tmp, ":");	
	    /* If we'll be installing all languages anyway, don't bother */
	    for (ARGV_t l = langs; *l; l++) {
		if (rstreq(*l, "all")) {
		    langs = argvFree(langs);
		    break;
		}
	    }
	    ts->installLangs = langs;
	}
	free(tmp);
    }

    tsmem = xcalloc(1, sizeof(*ts->members));
    tsmem->delta = 5;
    tsmem->addedPackages = NULL;
    tsmem->removedPackages = intHashCreate(128, uintId, uintCmp, NULL);
    tsmem->orderAlloced = 0;
    tsmem->orderCount = 0;
    tsmem->order = NULL;
    ts->members = tsmem;

    ts->rootDir = NULL;
    ts->keyring = NULL;

    ts->selabelHandle = NULL;

    ts->nrefs = 0;

    ts->plugins = rpmpluginsNew(ts);

    return rpmtsLink(ts);
}

rpmtsi rpmtsiFree(rpmtsi tsi)
{
    /* XXX watchout: a funky recursion segfaults here iff nrefs is wrong. */
    if (tsi) {
	tsi->ts = rpmtsFree(tsi->ts);
	_free(tsi);
    }
    return NULL;
}

rpmtsi rpmtsiInit(rpmts ts)
{
    rpmtsi tsi = NULL;

    tsi = xcalloc(1, sizeof(*tsi));
    tsi->ts = rpmtsLink(ts);
    tsi->oc = 0;
    return tsi;
}

/**
 * Return next transaction element.
 * @param tsi		transaction element iterator
 * @return		transaction element, NULL on termination
 */
static
rpmte rpmtsiNextElement(rpmtsi tsi)
{
    rpmte te = NULL;
    int oc = -1;

    if (tsi == NULL || tsi->ts == NULL || rpmtsNElements(tsi->ts) <= 0)
	return te;

    if (tsi->oc < rpmtsNElements(tsi->ts))	oc = tsi->oc++;
    if (oc != -1)
	te = rpmtsElement(tsi->ts, oc);
    return te;
}

rpmte rpmtsiNext(rpmtsi tsi, rpmElementTypes types)
{
    rpmte te;

    while ((te = rpmtsiNextElement(tsi)) != NULL) {
	if (types == 0 || (rpmteType(te) & types) != 0)
	    break;
    }
    return te;
}

#define RPMLOCK_PATH LOCALSTATEDIR "/rpm/.rpm.lock"
rpmlock rpmtsAcquireLock(rpmts ts)
{
    static const char * const rpmlock_path_default = "%{?_rpmlock_path}";

    if (ts->lockPath == NULL) {
	const char *rootDir = rpmtsRootDir(ts);
	char *t;

	if (!rootDir || rpmChrootDone())
	    rootDir = "/";

	t = rpmGenPath(rootDir, rpmlock_path_default, NULL);
	if (t == NULL || *t == '\0' || *t == '%') {
	    free(t);
	    t = xstrdup(RPMLOCK_PATH);
	}
	ts->lockPath = xstrdup(t);
	(void) rpmioMkpath(dirname(t), 0755, getuid(), getgid());
	free(t);
    }
    return rpmlockAcquire(ts->lockPath, _("transaction"));
}

