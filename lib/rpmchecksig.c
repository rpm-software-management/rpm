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

static int readFile(FD_t fd, char **msg)
{
    unsigned char buf[4*BUFSIZ];
    ssize_t count;

    /* Read the payload from the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0) {}
    if (count < 0)
	rasprintf(msg, _("Fread failed: %s"), Fstrerror(fd));

    return (count != 0);
}

static rpmRC formatVerbose(struct rpmsinfo_s *sinfo, rpmRC sigres, const char *result, void *cbdata)
{
    rpmlog(RPMLOG_NOTICE, "    %s\n", result);
    return sigres;
}

/* Failures are uppercase, in parenthesis if NOKEY. Otherwise lowercase. */
static rpmRC formatDefault(struct rpmsinfo_s *sinfo, rpmRC sigres, const char *result, void *cbdata)
{
    const char *signame;
    int upper = sigres != RPMRC_OK;

    switch (sinfo->tag) {
    case RPMSIGTAG_SIZE:
	signame = (upper ? "SIZE" : "size");
	break;
    case RPMSIGTAG_SHA1:
	signame = (upper ? "SHA1" : "sha1");
	break;
    case RPMSIGTAG_SHA256:
	signame = (upper ? "SHA256" : "sha256");
	break;
    case RPMSIGTAG_MD5:
	signame = (upper ? "MD5" : "md5");
	break;
    case RPMSIGTAG_RSA:
	signame = (upper ? "RSA" : "rsa");
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	signame = (upper ? "PGP" : "pgp");
	break;
    case RPMSIGTAG_DSA:
	signame = (upper ? "DSA" : "dsa");
	break;
    case RPMSIGTAG_GPG:
	signame = (upper ? "GPG" : "gpg");
	break;
    case RPMTAG_PAYLOADDIGEST:
	signame = (upper ? "PAYLOAD" : "payload");
	break;
    default:
	signame = (upper ? "?UnknownSigatureType?" : "???");
	break;
    }
    rpmlog(RPMLOG_NOTICE, ((sigres == RPMRC_NOKEY) ? "(%s) " : "%s "), signame);
    return sigres;
}

static rpmRC handleResult(struct rpmsinfo_s *sinfo, rpmRC sigres, const char *result, void *cbdata)
{
    if (sigres == RPMRC_NOTFOUND) {
	sigres = RPMRC_OK;
    } else {
	if (rpmIsVerbose())
	    sigres = formatVerbose(sinfo, sigres, result, cbdata);
	else
	    sigres = formatDefault(sinfo, sigres, result, cbdata);
    }
    return sigres;
}

static void rpmvsInitDigests(struct rpmvs_s *sis, int range,
		       rpmDigestBundle bundle)
{
    for (int i = 0; i < sis->nsigs; i++) {
	struct rpmsinfo_s *sinfo = &sis->sigs[i];
	if (sinfo->range & range) {
	    if (sis->rcs[i] == RPMRC_OK)
		rpmDigestBundleAddID(bundle, sinfo->hashalgo, sinfo->id, 0);
	}
    }
}

static int rpmvsVerifyItems(struct rpmvs_s *sis, int range,
		       rpmDigestBundle bundle,
		       rpmKeyring keyring, rpmsinfoCb cb, void *cbdata)
{
    int failed = 0;

    for (int i = 0; i < sis->nsigs; i++) {
	struct rpmsinfo_s *sinfo = &sis->sigs[i];

	if (sinfo->range == range) {
	    if (sis->rcs[i] == RPMRC_OK) {
		DIGEST_CTX ctx = rpmDigestBundleDupCtx(bundle, sinfo->id);
		sis->results[i] = _free(sis->results[i]);
		sis->rcs[i] = rpmVerifySignature(keyring, sinfo, ctx, &sis->results[i]);
		rpmDigestFinal(ctx, NULL, NULL, 0);
		rpmDigestBundleFinal(bundle, sinfo->id, NULL, NULL, 0);
	    }

	    if (cb)
		sis->rcs[i] = cb(sinfo, sis->rcs[i], sis->results[i], cbdata);

	    if (sis->rcs[i] != RPMRC_OK)
		failed++;
	}
    }

    return failed;
}

rpmRC rpmpkgVerifySignatures(rpmKeyring keyring, rpmVSFlags flags, FD_t fd,
			    rpmsinfoCb cb, void *cbdata)
{

    char * msg = NULL;
    rpmRC rc = RPMRC_FAIL; /* assume failure */
    int failed = 0;
    struct hdrblob_s sigblob, blob;
    struct rpmvs_s *sigset = NULL;
    rpmDigestBundle bundle = fdGetBundle(fd, 1); /* freed with fd */

    memset(&blob, 0, sizeof(blob));
    memset(&sigblob, 0, sizeof(sigblob));

    if (rpmLeadRead(fd, NULL, &msg))
	goto exit;

    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERSIGNATURES, &sigblob, &msg))
	goto exit;

    sigset = rpmvsCreate(&sigblob, flags);

    /* Initialize digests ranging over the header */
    rpmvsInitDigests(sigset, RPMSIG_HEADER, bundle);

    /* Read the header from the package. */
    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERIMMUTABLE, &blob, &msg))
	goto exit;

    /* Fish interesting tags from the main header. This is a bit hacky... */
    if (!(flags & RPMVSF_NOPAYLOAD))
	rpmvsAppend(sigset, &blob, RPMTAG_PAYLOADDIGEST);

    /* Initialize digests ranging over the payload only */
    rpmvsInitDigests(sigset, RPMSIG_PAYLOAD, bundle);

    /* Verify header signatures and digests */
    failed += rpmvsVerifyItems(sigset, (RPMSIG_HEADER), bundle, keyring, cb, cbdata);

    /* Read the file, generating digest(s) on the fly. */
    if (readFile(fd, &msg))
	goto exit;

    /* Verify signatures and digests ranging over the payload */
    failed += rpmvsVerifyItems(sigset, (RPMSIG_PAYLOAD), bundle,
			keyring, cb, cbdata);
    failed += rpmvsVerifyItems(sigset, (RPMSIG_HEADER|RPMSIG_PAYLOAD), bundle,
			keyring, cb, cbdata);

    if (failed == 0)
	rc = RPMRC_OK;

exit:
    if (rc && msg != NULL)
	rpmlog(RPMLOG_ERR, "%s: %s\n", Fdescr(fd), msg);
    free(msg);
    free(sigblob.ei);
    free(blob.ei);
    rpmvsFree(sigset);
    return rc;
}

static int rpmpkgVerifySigs(rpmKeyring keyring, rpmVSFlags flags,
			   FD_t fd, const char *fn)
{
    int rc;
    if (rpmIsVerbose()) {
	rpmlog(RPMLOG_NOTICE, "%s:\n", fn);
	rc = rpmpkgVerifySignatures(keyring, flags, fd, handleResult, NULL);
    } else {
	rpmlog(RPMLOG_NOTICE, "%s: ", fn);
	rc = rpmpkgVerifySignatures(keyring, flags, fd, handleResult, NULL);
	rpmlog(RPMLOG_NOTICE, "%s\n", rc ? _("NOT OK") : _("OK"));
    }
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
