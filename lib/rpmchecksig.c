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
		if(pgpPubKeyCertLen(pkti, pktlen, &certlen)) {
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
static int readFile(FD_t fd, const char * fn, rpmDigestBundle hdrbundle)
{
    unsigned char buf[4*BUFSIZ];
    ssize_t count;
    int rc = 1;
    struct hdrblob_s blob;
    char *msg = NULL;

    /* Read the header from the package. */
    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERIMMUTABLE, &blob, &msg) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("%s: headerRead failed: %s\n"), fn, msg);
	goto exit;
    }

    if (blob.regionTag == RPMTAG_HEADERIMMUTABLE) {
	rpmDigestBundleUpdate(hdrbundle,
				rpm_header_magic, sizeof(rpm_header_magic));
	rpmDigestBundleUpdate(hdrbundle, blob.ei, blob.pvlen);
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
    free(blob.ei);
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

static int rpmpkgVerifySigs(rpmKeyring keyring, rpmQueryFlags flags,
			   FD_t fd, const char *fn)
{

    char *buf = NULL;
    char *missingKeys = NULL; 
    char *untrustedKeys = NULL;
    struct rpmtd_s sigtd;
    pgpDigParams sig = NULL;
    Header sigh = NULL;
    HeaderIterator hi = NULL;
    char * msg = NULL;
    char *result = NULL;
    int res = 1; /* assume failure */
    rpmRC rc;
    int failed = 0;
    int nodigests = !(flags & VERIFY_DIGEST);
    int nosignatures = !(flags & VERIFY_SIGNATURE);
    struct sigtInfo_s sinfo;
    rpmDigestBundle plbundle = rpmDigestBundleNew();
    rpmDigestBundle hdrbundle = rpmDigestBundleNew();

    if ((rc = rpmLeadRead(fd, NULL, &msg)) != RPMRC_OK) {
	goto exit;
    }

    rc = rpmReadSignature(fd, &sigh, &msg);

    if (rc != RPMRC_OK) {
	goto exit;
    }

    /* Initialize all digests we'll be needing */
    hi = headerInitIterator(sigh);
    for (; headerNext(hi, &sigtd) != 0; rpmtdFreeData(&sigtd)) {
	rc = rpmSigInfoParse(&sigtd, "package", &sinfo, NULL, NULL);

	if (nosignatures && sinfo.type == RPMSIG_SIGNATURE_TYPE)
	    continue;
	if (nodigests && sinfo.type == RPMSIG_DIGEST_TYPE)
	    continue;
	if (rc == RPMRC_OK && sinfo.hashalgo) {
	    rpmDigestBundleAdd((sinfo.range & RPMSIG_PAYLOAD) ?
				plbundle : hdrbundle,
			       sinfo.hashalgo, RPMDIGEST_NONE);
	}
    }
    hi = headerFreeIterator(hi);

    /* Read the file, generating digest(s) on the fly. */
    fdSetBundle(fd, plbundle);
    if (readFile(fd, fn, hdrbundle)) {
	goto exit;
    }

    rasprintf(&buf, "%s:%c", fn, (rpmIsVerbose() ? '\n' : ' ') );

    hi = headerInitIterator(sigh);
    for (; headerNext(hi, &sigtd) != 0; rpmtdFreeData(&sigtd)) {
	DIGEST_CTX ctx = NULL;
	if (sigtd.data == NULL) /* XXX can't happen */
	    continue;

	/* Clean up parameters from previous sigtag. */
	sig = pgpDigParamsFree(sig);
	result = _free(result);

	/* Note: we permit failures to be ignored via disablers */
	rc = rpmSigInfoParse(&sigtd, "package", &sinfo, &sig, &result);

	if (nosignatures && sinfo.type == RPMSIG_SIGNATURE_TYPE)
	    continue;
	if (nodigests &&  sinfo.type == RPMSIG_DIGEST_TYPE)
	    continue;
	if (sinfo.type == RPMSIG_UNKNOWN_TYPE)
	    continue;

	if (sinfo.type != RPMSIG_OTHER_TYPE && rc == RPMRC_OK) {
	    ctx = rpmDigestBundleDupCtx((sinfo.range & RPMSIG_PAYLOAD) ?
					plbundle : hdrbundle,
					sinfo.hashalgo);
	    rc = rpmVerifySignature(keyring, &sigtd, sig, ctx, &result);
	    rpmDigestFinal(ctx, NULL, NULL, 0);
	}

	if (result) {
	    formatResult(sigtd.tag, rc, result,
		     (rc == RPMRC_NOKEY ? &missingKeys : &untrustedKeys),
		     &buf);
	}

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
    if (res && msg != NULL)
	rpmlog(RPMLOG_ERR, "%s: %s\n", fn, msg);
    free(msg);
    free(result);
    free(buf);
    rpmDigestBundleFree(hdrbundle);
    rpmDigestBundleFree(plbundle);
    fdSetBundle(fd, NULL); /* XXX avoid double-free from fd close */
    sigh = headerFree(sigh);
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
	rpmsqPoll();
    }
    rpmKeyringFree(keyring);
    return res;
}
