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
#include <rpm/rpmsq.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmkeyring.h>

#include "rpmio/rpmio_internal.h" 	/* fdSetBundle() */
#include "lib/rpmlead.h"
#include "lib/signature.h"
#include "lib/header_internal.h"

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
	uint8_t *pkti = NULL;
	size_t pktlen = 0;
	size_t certlen;
	
	/* Read pgp packet. */
	if (pgpParsePkts(start, &pkt, &pktlen) == PGPARMOR_PUBKEY) {
	    pkti = pkt;

	    /* Iterate over certificates in pkt */
	    while (pktlen > 0) {
		if (pgpPubKeyCertLen(pkti, pktlen, &certlen)) {
		    rpmlog(RPMLOG_ERR, _("%s: key %d import failed.\n"), fn,
			    keyno);
		    res++;
		    break;
		}

		/* Import pubkey certificate. */
		if (rpmtsImportPubkey(ts, pkti, certlen) != RPMRC_OK) {
		    rpmlog(RPMLOG_ERR, _("%s: key %d import failed.\n"), fn,
			    keyno);
		    res++;
		}
		pkti += certlen;
		pktlen -= certlen;
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
static int readFile(FD_t fd, const char * fn)
{
    unsigned char buf[4*BUFSIZ];
    ssize_t count;
    int rc = 1;
    char *msg = NULL;

    /* Read the payload from the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0) {}
    if (count < 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fread failed: %s\n"), fn, Fstrerror(fd));
	goto exit;
    }

    rc = 0;

exit:
    free(msg);
    return rc;
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
    case RPMSIGTAG_SHA256:
	n = (upper ? "SHA256" : "sha256");
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
    case RPMTAG_PAYLOADDIGEST:
	n = (upper ? "PAYLOAD" : "payload");
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
			 char **keyprob, char **buf)
{
    char *msg = NULL;
    if (rpmIsVerbose()) {
	rasprintf(&msg, "    %s\n", result);
    } else { 
	/* Check for missing / untrusted keys in result. */
	const char *signame = sigtagname(sigtag, (sigres != RPMRC_OK));
	
	if (sigres == RPMRC_NOKEY || sigres == RPMRC_NOTTRUSTED) {
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

static void initDigests(FD_t fd, Header sigh, int range, rpmQueryFlags flags)
{
    struct rpmsinfo_s sinfo;
    struct rpmtd_s sigtd;
    HeaderIterator hi = headerInitIterator(sigh);

    for (; headerNext(hi, &sigtd) != 0; rpmtdFreeData(&sigtd)) {
	if (rpmsinfoInit(&sigtd, "package", &sinfo, NULL, NULL))
	    continue;
	if (!(flags & VERIFY_SIGNATURE) && sinfo.type == RPMSIG_SIGNATURE_TYPE)
	    continue;
	if (!(flags & VERIFY_DIGEST) && sinfo.type == RPMSIG_DIGEST_TYPE)
	    continue;

	if (sinfo.hashalgo && (sinfo.range & range))
	    fdInitDigestID(fd, sinfo.hashalgo, sinfo.id, 0);
    }
    headerFreeIterator(hi);
}

static int verifyItems(FD_t fd, Header sigh, int range, rpmQueryFlags flags,
		       rpmKeyring keyring,
		       char **missingKeys, char **untrustedKeys, char **buf)
{
    int failed = 0;
    struct rpmsinfo_s sinfo;
    struct rpmtd_s sigtd;
    pgpDigParams sig = NULL;
    char *result = NULL;
    HeaderIterator hi = headerInitIterator(sigh);

    for (; headerNext(hi, &sigtd) != 0; rpmtdFreeData(&sigtd)) {
	/* Clean up parameters from previous sigtag. */
	sig = pgpDigParamsFree(sig);
	result = _free(result);

	/* Note: we permit failures to be ignored via disablers */
	rpmRC rc = rpmsinfoInit(&sigtd, "package", &sinfo, &sig, &result);

	if (!(flags & VERIFY_SIGNATURE) && sinfo.type == RPMSIG_SIGNATURE_TYPE)
	    continue;
	if (!(flags & VERIFY_DIGEST) && sinfo.type == RPMSIG_DIGEST_TYPE)
	    continue;
	if (sinfo.type == RPMSIG_UNKNOWN_TYPE)
	    continue;

	if (sinfo.hashalgo && sinfo.range == range && rc ==  RPMRC_OK) {
	    DIGEST_CTX ctx = fdDupDigest(fd, sinfo.id);
	    rc = rpmVerifySignature(keyring, &sigtd, sig, ctx, &result);
	    rpmDigestFinal(ctx, NULL, NULL, 0);
	    fdFiniDigest(fd, sinfo.id, NULL, NULL, 0);
	}

	if (result) {
	    formatResult(sigtd.tag, rc, result,
		     (rc == RPMRC_NOKEY ? missingKeys : untrustedKeys), buf);
	}

	if (rc != RPMRC_OK)
	    failed = 1;
    }
    pgpDigParamsFree(sig);
    headerFreeIterator(hi);
    free(result);

    return failed;
}

static int rpmpkgVerifySigs(rpmKeyring keyring, rpmQueryFlags flags,
			   FD_t fd, const char *fn)
{

    char *buf = NULL;
    char *missingKeys = NULL; 
    char *untrustedKeys = NULL;
    Header sigh = NULL;
    Header h = NULL;
    char * msg = NULL;
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    int failed = 0;
    struct hdrblob_s blob;
    rpmTagVal copyTags[] = { RPMTAG_PAYLOADDIGEST, 0 };

    memset(&blob, 0, sizeof(blob));

    if (rpmLeadRead(fd, NULL, &msg))
	goto exit;

    if (rpmReadSignature(fd, &sigh, &msg))
	goto exit;

    /* Initialize digests ranging over the header */
    initDigests(fd, sigh, RPMSIG_HEADER, flags);

    /* Read the header from the package. */
    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERIMMUTABLE, &blob, &msg))
	goto exit;

    rasprintf(&buf, "%s:%c", fn, (rpmIsVerbose() ? '\n' : ' ') );

    /* Verify header signatures and digests */
    failed += verifyItems(fd, sigh, (RPMSIG_HEADER), flags, keyring,
			&missingKeys, &untrustedKeys, &buf);

    /* Fish interesting tags from the main header */
    if (hdrblobImport(&blob, 0, &h, &msg))
	goto exit;
    headerCopyTags(h, sigh, copyTags);

    /* Initialize digests ranging over the payload only */
    initDigests(fd, sigh, RPMSIG_PAYLOAD, flags);

    /* Read the file, generating digest(s) on the fly. */
    if (readFile(fd, fn))
	goto exit;

    /* Verify signatures and digests ranging over the payload */
    failed += verifyItems(fd, sigh, (RPMSIG_PAYLOAD), flags,
			keyring, &missingKeys, &untrustedKeys, &buf);
    failed += verifyItems(fd, sigh, (RPMSIG_HEADER|RPMSIG_PAYLOAD), flags,
			keyring, &missingKeys, &untrustedKeys, &buf);

    if (failed == 0)
	rc = RPMRC_OK;

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

exit:
    if (rc && msg != NULL)
	rpmlog(RPMLOG_ERR, "%s: %s\n", fn, msg);
    free(msg);
    free(buf);
    free(blob.ei);
    free(missingKeys);
    free(untrustedKeys);
    sigh = headerFree(sigh);
    h = headerFree(h);
    return rc;
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
	rpmsqPoll();
    }
    rpmKeyringFree(keyring);
    return res;
}
