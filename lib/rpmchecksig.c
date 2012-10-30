/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include <ctype.h>

#include <rpm/rpmlib.h>			/* RPMSIGTAG & related */
#include <rpm/rpmpgp.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmfileutil.h>	/* rpmMkTemp() */
#include <rpm/rpmdb.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmkeyring.h>

#include "rpmio/rpmio_internal.h" 	/* fdSetBundle() */
#include "lib/rpmlead.h"
#include "lib/signature.h"

#include "debug.h"

int _print_pkts = 0;

static int doImport(rpmts ts, const char *fn, char *buf, ssize_t blen)
{
    char const * const pgpmark = "-----BEGIN PGP ";
    size_t marklen = strlen(pgpmark);
    int res = 0;
    int keyno = 1;
    char *start = strstr(buf, pgpmark);

    do {
	uint8_t *pkt = NULL;
	size_t pktlen = 0;
	
	/* Read pgp packet. */
	if (pgpParsePkts(start, &pkt, &pktlen) == PGPARMOR_PUBKEY) {
	    /* Import pubkey packet(s). */
	    if (rpmtsImportPubkey(ts, pkt, pktlen) != RPMRC_OK) {
		rpmlog(RPMLOG_ERR, _("%s: key %d import failed.\n"), fn, keyno);
		res++;
	    }
	} else {
	    rpmlog(RPMLOG_ERR, _("%s: key %d not an armored public key.\n"),
		   fn, keyno);
	    res++;
	}
	
	/* See if there are more keys in the buffer */
	if (start && start + marklen < buf + blen) {
	    start = strstr(start + marklen, pgpmark);
	} else {
	    start = NULL;
	}

	keyno++;
	free(pkt);
    } while (start != NULL);

    return res;
}

int rpmcliImportPubkeys(rpmts ts, ARGV_const_t argv)
{
    int res = 0;
    for (ARGV_const_t arg = argv; arg && *arg; arg++) {
	const char *fn = *arg;
	uint8_t *buf = NULL;
	ssize_t blen = 0;
	char *t = NULL;
	int iorc;

	/* If arg looks like a keyid, then attempt keyserver retrieve. */
	if (rstreqn(fn, "0x", 2)) {
	    const char * s = fn + 2;
	    int i;
	    for (i = 0; *s && isxdigit(*s); s++, i++)
		{};
	    if (i == 8 || i == 16) {
		t = rpmExpand("%{_hkp_keyserver_query}", fn+2, NULL);
		if (t && *t != '%')
		    fn = t;
	    }
	}

	/* Read the file and try to import all contained keys */
	iorc = rpmioSlurp(fn, &buf, &blen);
	if (iorc || buf == NULL || blen < 64) {
	    rpmlog(RPMLOG_ERR, _("%s: import read failed(%d).\n"), fn, iorc);
	    res++;
	} else {
	    res += doImport(ts, fn, (char *)buf, blen);
	}
	
	free(t);
	free(buf);
    }
    return res;
}

/**
 * @todo If the GPG key was known available, the md5 digest could be skipped.
 */
static int readFile(FD_t fd, const char * fn,
		    rpmDigestBundle plbundle, rpmDigestBundle hdrbundle)
{
    unsigned char buf[4*BUFSIZ];
    ssize_t count;
    int rc = 1;
    Header h = NULL;
    char *msg = NULL;

    /* Read the header from the package. */
    if (rpmReadHeader(NULL, fd, &h, &msg) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("%s: headerRead failed: %s\n"), fn, msg);
	goto exit;
    }

    if (headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	struct rpmtd_s utd;
    
	if (!headerGet(h, RPMTAG_HEADERIMMUTABLE, &utd, HEADERGET_DEFAULT)){
	    rpmlog(RPMLOG_ERR, 
		    _("%s: Immutable header region could not be read. "
		    "Corrupted package?\n"), fn);
	    goto exit;
	}
	rpmDigestBundleUpdate(hdrbundle, rpm_header_magic, sizeof(rpm_header_magic));
	rpmDigestBundleUpdate(hdrbundle, utd.data, utd.count);
	rpmtdFreeData(&utd);
    }

    /* Read the payload from the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0) {}
    if (count < 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fread failed: %s\n"), fn, Fstrerror(fd));
	goto exit;
    }

    rc = 0;

exit:
    free(msg);
    headerFree(h);
    return rc;
}

/* 
 * Figure best available signature. 
 * XXX TODO: Similar detection in rpmReadPackageFile(), unify these.
 */
static rpmTagVal bestSig(Header sigh, int nosignatures, int nodigests)
{
    rpmTagVal sigtag = 0;
    if (sigtag == 0 && !nosignatures) {
	if (headerIsEntry(sigh, RPMSIGTAG_DSA))
	    sigtag = RPMSIGTAG_DSA;
	else if (headerIsEntry(sigh, RPMSIGTAG_RSA))
	    sigtag = RPMSIGTAG_RSA;
	else if (headerIsEntry(sigh, RPMSIGTAG_GPG))
	    sigtag = RPMSIGTAG_GPG;
	else if (headerIsEntry(sigh, RPMSIGTAG_PGP))
	    sigtag = RPMSIGTAG_PGP;
    }
    if (sigtag == 0 && !nodigests) {
	if (headerIsEntry(sigh, RPMSIGTAG_MD5))
	    sigtag = RPMSIGTAG_MD5;
	else if (headerIsEntry(sigh, RPMSIGTAG_SHA1))
	    sigtag = RPMSIGTAG_SHA1;	/* XXX never happens */
    }
    return sigtag;
}

static const char *sigtagname(rpmTagVal sigtag, int upper)
{
    const char *n = NULL;

    switch (sigtag) {
    case RPMSIGTAG_SIZE:
	n = (upper ? "SIZE" : "size");
	break;
    case RPMSIGTAG_SHA1:
	n = (upper ? "SHA1" : "sha1");
	break;
    case RPMSIGTAG_MD5:
	n = (upper ? "MD5" : "md5");
	break;
    case RPMSIGTAG_RSA:
	n = (upper ? "RSA" : "rsa");
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	n = (upper ? "(MD5) PGP" : "(md5) pgp");
	break;
    case RPMSIGTAG_DSA:
	n = (upper ? "(SHA1) DSA" : "(sha1) dsa");
	break;
    case RPMSIGTAG_GPG:
	n = (upper ? "GPG" : "gpg");
	break;
    default:
	n = (upper ? "?UnknownSigatureType?" : "???");
	break;
    }
    return n;
}

/* 
 * Format sigcheck result for output, appending the message spew to buf and
 * bad/missing keyids to keyprob.
 *
 * In verbose mode, just dump it all. Otherwise ok signatures
 * are dumped lowercase, bad sigs uppercase and for PGP/GPG
 * if misssing/untrusted key it's uppercase in parenthesis
 * and stash the key id as <SIGTYPE>#<keyid>. Pfft.
 */
static void formatResult(rpmTagVal sigtag, rpmRC sigres, const char *result,
			 int havekey, char **keyprob, char **buf)
{
    char *msg = NULL;
    if (rpmIsVerbose()) {
	rasprintf(&msg, "    %s", result);
    } else { 
	/* Check for missing / untrusted keys in result. */
	const char *signame = sigtagname(sigtag, (sigres != RPMRC_OK));
	
	if (havekey && (sigres == RPMRC_NOKEY || sigres == RPMRC_NOTTRUSTED)) {
	    const char *tempKey = strstr(result, "ey ID");
	    if (tempKey) {
		char keyid[sizeof(pgpKeyID_t) + 1];
		rstrlcpy(keyid, tempKey + 6, sizeof(keyid));
		rstrscat(keyprob, " ", signame, "#", keyid, NULL);
	    }
	}
	rasprintf(&msg, (*keyprob ? "(%s) " : "%s "), signame);
    }
    rstrcat(buf, msg);
    free(msg);
}

static int rpmpkgVerifySigs(rpmKeyring keyring, rpmQueryFlags flags,
			   FD_t fd, const char *fn)
{

    char *buf = NULL;
    char *missingKeys = NULL; 
    char *untrustedKeys = NULL;
    struct rpmtd_s sigtd;
    rpmTagVal sigtag;
    pgpDigParams sig = NULL;
    Header sigh = NULL;
    HeaderIterator hi = NULL;
    char * msg = NULL;
    int res = 1; /* assume failure */
    rpmRC rc;
    int failed = 0;
    int nodigests = !(flags & VERIFY_DIGEST);
    int nosignatures = !(flags & VERIFY_SIGNATURE);
    rpmDigestBundle plbundle = rpmDigestBundleNew();
    rpmDigestBundle hdrbundle = rpmDigestBundleNew();

    if ((rc = rpmLeadRead(fd, NULL, NULL, &msg)) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, "%s: %s\n", fn, msg);
	free(msg);
	goto exit;
    }

    rc = rpmReadSignature(fd, &sigh, RPMSIGTYPE_HEADERSIG, &msg);
    switch (rc) {
    default:
	rpmlog(RPMLOG_ERR, _("%s: rpmReadSignature failed: %s"), fn,
		    (msg && *msg ? msg : "\n"));
	msg = _free(msg);
	goto exit;
	break;
    case RPMRC_OK:
	if (sigh == NULL) {
	    rpmlog(RPMLOG_ERR, _("%s: No signature available\n"), fn);
	    goto exit;
	}
	break;
    }
    msg = _free(msg);

    /* Grab a hint of what needs doing to avoid duplication. */
    sigtag = bestSig(sigh, nosignatures, nodigests);

    /* XXX RSA needs the hash_algo, so decode early. */
    if (sigtag == RPMSIGTAG_RSA || sigtag == RPMSIGTAG_PGP ||
		sigtag == RPMSIGTAG_DSA || sigtag == RPMSIGTAG_GPG) {
	unsigned int hashalgo;
	if (headerGet(sigh, sigtag, &sigtd, HEADERGET_DEFAULT)) {
	    parsePGPSig(&sigtd, "package", fn, &sig);
	    rpmtdFreeData(&sigtd);
	}
	if (sig == NULL) goto exit;
	    
	/* XXX assume same hash_algo in header-only and header+payload */
	hashalgo = pgpDigParamsAlgo(sig, PGPVAL_HASHALGO);
	rpmDigestBundleAdd(plbundle, hashalgo, RPMDIGEST_NONE);
	rpmDigestBundleAdd(hdrbundle, hashalgo, RPMDIGEST_NONE);
    }

    if (headerIsEntry(sigh, RPMSIGTAG_PGP) ||
		      headerIsEntry(sigh, RPMSIGTAG_PGP5) ||
		      headerIsEntry(sigh, RPMSIGTAG_MD5)) {
	rpmDigestBundleAdd(plbundle, PGPHASHALGO_MD5, RPMDIGEST_NONE);
    }
    if (headerIsEntry(sigh, RPMSIGTAG_GPG)) {
	rpmDigestBundleAdd(plbundle, PGPHASHALGO_SHA1, RPMDIGEST_NONE);
    }

    /* always do sha1 hash of header */
    rpmDigestBundleAdd(hdrbundle, PGPHASHALGO_SHA1, RPMDIGEST_NONE);

    /* Read the file, generating digest(s) on the fly. */
    fdSetBundle(fd, plbundle);
    if (readFile(fd, fn, plbundle, hdrbundle)) {
	goto exit;
    }

    rasprintf(&buf, "%s:%c", fn, (rpmIsVerbose() ? '\n' : ' ') );

    hi = headerInitIterator(sigh);
    for (; headerNext(hi, &sigtd) != 0; rpmtdFreeData(&sigtd)) {
	char *result = NULL;
	int havekey = 0;
	DIGEST_CTX ctx = NULL;
	if (sigtd.data == NULL) /* XXX can't happen */
	    continue;

	/* Clean up parameters from previous sigtag. */
	sig = pgpDigParamsFree(sig);

	switch (sigtd.tag) {
	case RPMSIGTAG_GPG:
	case RPMSIGTAG_PGP5:	/* XXX legacy */
	case RPMSIGTAG_PGP:
	    havekey = 1;
	case RPMSIGTAG_RSA:
	case RPMSIGTAG_DSA:
	    if (nosignatures)
		 continue;
	    if (parsePGPSig(&sigtd, "package", fn, &sig))
		goto exit;
	    ctx = rpmDigestBundleDupCtx(havekey ? plbundle : hdrbundle,
					pgpDigParamsAlgo(sig, PGPVAL_HASHALGO));
	    break;
	case RPMSIGTAG_SHA1:
	    if (nodigests)
		 continue;
	    ctx = rpmDigestBundleDupCtx(hdrbundle, PGPHASHALGO_SHA1);
	    break;
	case RPMSIGTAG_MD5:
	    if (nodigests)
		 continue;
	    ctx = rpmDigestBundleDupCtx(plbundle, PGPHASHALGO_MD5);
	    break;
	default:
	    continue;
	    break;
	}

	rc = rpmVerifySignature(keyring, &sigtd, sig, ctx, &result);
	rpmDigestFinal(ctx, NULL, NULL, 0);

	formatResult(sigtd.tag, rc, result, havekey, 
		     (rc == RPMRC_NOKEY ? &missingKeys : &untrustedKeys),
		     &buf);
	free(result);

	if (rc != RPMRC_OK) {
	    failed = 1;
	}

    }
    res = failed;

    if (rpmIsVerbose()) {
	rpmlog(RPMLOG_NOTICE, "%s", buf);
    } else {
	const char *ok = (failed ? _("NOT OK") : _("OK"));
	rpmlog(RPMLOG_NOTICE, "%s%s%s%s%s%s%s%s\n", buf, ok,
	       missingKeys ? _(" (MISSING KEYS:") : "",
	       missingKeys ? missingKeys : "",
	       missingKeys ? _(") ") : "",
	       untrustedKeys ? _(" (UNTRUSTED KEYS:") : "",
	       untrustedKeys ? untrustedKeys : "",
	       untrustedKeys ? _(")") : "");
    }
    free(missingKeys);
    free(untrustedKeys);

exit:
    free(buf);
    rpmDigestBundleFree(hdrbundle);
    rpmDigestBundleFree(plbundle);
    fdSetBundle(fd, NULL); /* XXX avoid double-free from fd close */
    sigh = rpmFreeSignature(sigh);
    hi = headerFreeIterator(hi);
    pgpDigParamsFree(sig);
    return res;
}

/* Wrapper around rpmkVerifySigs to preserve API */
int rpmVerifySignatures(QVA_t qva, rpmts ts, FD_t fd, const char * fn)
{
    int rc = 1; /* assume failure */
    if (ts && qva && fd && fn) {
	rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
	rc = rpmpkgVerifySigs(keyring, qva->qva_flags, fd, fn);
    	rpmKeyringFree(keyring);
    }
    return rc;
}

int rpmcliVerifySignatures(rpmts ts, ARGV_const_t argv)
{
    const char * arg;
    int res = 0;
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
    rpmVerifyFlags verifyFlags = (VERIFY_DIGEST|VERIFY_SIGNATURE);
    
    verifyFlags &= ~rpmcliQueryFlags;

    while ((arg = *argv++) != NULL) {
	FD_t fd = Fopen(arg, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), 
		     arg, Fstrerror(fd));
	    res++;
	} else if (rpmpkgVerifySigs(keyring, verifyFlags, fd, arg)) {
	    res++;
	}

	Fclose(fd);
	rpmdbCheckSignals();
    }
    rpmKeyringFree(keyring);
    return res;
}
