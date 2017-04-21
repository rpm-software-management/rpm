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
	n = (upper ? "PGP" : "pgp");
	break;
    case RPMSIGTAG_DSA:
	n = (upper ? "DSA" : "dsa");
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
 * Format sigcheck result for output, appending the message spew to buf.
 * In verbose mode, just dump it all. Otherwise ok signatures
 * are dumped lowercase, bad sigs uppercase and in parentheses if
 * missing key. Pfft.
 */
static void formatResult(rpmTagVal sigtag, rpmRC sigres, const char *result,
			 char **buf)
{
    char *msg = NULL;
    if (rpmIsVerbose()) {
	rasprintf(&msg, "    %s\n", result);
    } else { 
	/* Check for missing keys in result. */
	const char *signame = sigtagname(sigtag, (sigres != RPMRC_OK));
	rasprintf(&msg, ((sigres == RPMRC_NOKEY) ? "(%s) " : "%s "), signame);
    }
    rstrcat(buf, msg);
    free(msg);
}

static int sinfoDisabled(struct rpmsinfo_s *sinfo, rpmVSFlags vsflags)
{
    if (vsflags & sinfo->disabler)
	return 1;
    if ((vsflags & RPMVSF_NEEDPAYLOAD) && (sinfo->range & RPMSIG_PAYLOAD))
	return 1;
    return 0;
}

static void initDigests(FD_t fd, Header sigh, int range, rpmVSFlags flags)
{
    struct rpmsinfo_s sinfo;
    struct rpmtd_s sigtd;
    HeaderIterator hi = headerInitIterator(sigh);
    memset(&sinfo, 0, sizeof(sinfo));

    for (; headerNext(hi, &sigtd) != 0; rpmtdFreeData(&sigtd)) {
	rpmsinfoFini(&sinfo);
	if (rpmsinfoInit(&sigtd, "package", &sinfo, NULL))
	    continue;
	if (sinfoDisabled(&sinfo, flags))
	    continue;

	if (sinfo.hashalgo && (sinfo.range & range))
	    fdInitDigestID(fd, sinfo.hashalgo, sinfo.id, 0);
    }
    rpmsinfoFini(&sinfo);
    headerFreeIterator(hi);
}

static int verifyItems(FD_t fd, Header sigh, int range, rpmVSFlags flags,
		       rpmKeyring keyring, char **buf)
{
    int failed = 0;
    struct rpmsinfo_s sinfo;
    struct rpmtd_s sigtd;
    char *result = NULL;
    HeaderIterator hi = headerInitIterator(sigh);
    memset(&sinfo, 0, sizeof(sinfo));

    for (; headerNext(hi, &sigtd) != 0; rpmtdFreeData(&sigtd)) {
	/* Clean up parameters from previous sigtag. */
	rpmsinfoFini(&sinfo);
	result = _free(result);

	/* Note: we permit failures to be ignored via disablers */
	rpmRC rc = rpmsinfoInit(&sigtd, "package", &sinfo, &result);

	if (sinfoDisabled(&sinfo, flags))
	    continue;
	if (sinfo.type == RPMSIG_UNKNOWN_TYPE)
	    continue;

	if (sinfo.hashalgo && sinfo.range == range && rc ==  RPMRC_OK) {
	    DIGEST_CTX ctx = fdDupDigest(fd, sinfo.id);
	    rc = rpmVerifySignature(keyring, &sinfo, ctx, &result);
	    rpmDigestFinal(ctx, NULL, NULL, 0);
	    fdFiniDigest(fd, sinfo.id, NULL, NULL, 0);
	}

	if (result) {
	    formatResult(sinfo.tag, rc, result, buf);
	}

	if (rc != RPMRC_OK)
	    failed = 1;
    }
    rpmsinfoFini(&sinfo);
    headerFreeIterator(hi);
    free(result);

    return failed;
}

static int rpmpkgVerifySigs(rpmKeyring keyring, rpmVSFlags flags,
			   FD_t fd, const char *fn)
{

    char *buf = NULL;
    Header sigh = NULL;
    Header h = NULL;
    char * msg = NULL;
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    int failed = 0;
    struct hdrblob_s sigblob, blob;
    rpmTagVal copyTags[] = { RPMTAG_PAYLOADDIGEST, 0 };

    memset(&blob, 0, sizeof(blob));
    memset(&sigblob, 0, sizeof(sigblob));

    if (rpmLeadRead(fd, NULL, &msg))
	goto exit;

    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERSIGNATURES, &sigblob, &msg))
	goto exit;
    if (hdrblobImport(&sigblob, 0, &sigh, &msg))
	goto exit;

    /* Initialize digests ranging over the header */
    initDigests(fd, sigh, RPMSIG_HEADER, flags);

    /* Read the header from the package. */
    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERIMMUTABLE, &blob, &msg))
	goto exit;

    /* Verify header signatures and digests */
    failed += verifyItems(fd, sigh, (RPMSIG_HEADER), flags, keyring, &buf);

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
			keyring, &buf);
    failed += verifyItems(fd, sigh, (RPMSIG_HEADER|RPMSIG_PAYLOAD), flags,
			keyring, &buf);

    if (failed == 0)
	rc = RPMRC_OK;

    if (rpmIsVerbose()) {
	rpmlog(RPMLOG_NOTICE, "%s:\n%s", fn, buf);
    } else {
	const char *ok = (failed ? _("NOT OK") : _("OK"));
	rpmlog(RPMLOG_NOTICE, "%s: %s%s\n", fn, buf, ok);
    }

exit:
    if (rc && msg != NULL)
	rpmlog(RPMLOG_ERR, "%s: %s\n", fn, msg);
    free(msg);
    free(buf);
    free(sigblob.ei);
    free(blob.ei);
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
    rpmVSFlags vsflags = 0;

    if (rpmcliQueryFlags & QUERY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (rpmcliQueryFlags & QUERY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;

    while ((arg = *argv++) != NULL) {
	FD_t fd = Fopen(arg, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), 
		     arg, Fstrerror(fd));
	    res++;
	} else if (rpmpkgVerifySigs(keyring, vsflags, fd, arg)) {
	    res++;
	}

	Fclose(fd);
	rpmsqPoll();
    }
    rpmKeyringFree(keyring);
    return res;
}
