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
#include "lib/header_internal.h"
#include "lib/rpmvs.h"

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

struct vfydata_s {
    int seen;
    int bad;
};

static rpmRC formatVerbose(struct rpmsinfo_s *sinfo, rpmRC sigres, const char *result, void *cbdata)
{
    char *vsmsg = rpmsinfoMsg(sinfo, sigres, result);
    rpmlog(RPMLOG_NOTICE, "    %s\n", vsmsg);
    free(vsmsg);
    return sigres;
}

/* Failures are uppercase, in parenthesis if NOKEY. Otherwise lowercase. */
static rpmRC formatDefault(struct rpmsinfo_s *sinfo, rpmRC sigres, const char *result, void *cbdata)
{
    struct vfydata_s *vd = cbdata;
    vd->seen |= sinfo->type;
    if (sigres != RPMRC_OK)
	vd->bad |= sinfo->type;
    return sigres;
}

rpmRC rpmpkgRead(rpmKeyring keyring, rpmVSFlags flags, FD_t fd,
			    rpmsinfoCb cb, void *cbdata, Header *hdrp)
{

    char * msg = NULL;
    rpmRC xx, rc = RPMRC_FAIL; /* assume failure */
    int failed = 0;
    int leadtype = -1;
    struct hdrblob_s sigblob, blob;
    struct rpmvs_s *sigset = NULL;
    Header h = NULL;
    Header sigh = NULL;
    rpmDigestBundle bundle = fdGetBundle(fd, 1); /* freed with fd */

    memset(&blob, 0, sizeof(blob));
    memset(&sigblob, 0, sizeof(sigblob));

    if ((xx = rpmLeadRead(fd, &leadtype, &msg)) != RPMRC_OK) {
	/* Avoid message spew on manifests */
	if (xx == RPMRC_NOTFOUND)
	    msg = _free(msg);
	rc = xx;
	goto exit;
    }

    /* Read the signature header. Might not be in a contiguous region. */
    if (hdrblobRead(fd, 1, 0, RPMTAG_HEADERSIGNATURES, &sigblob, &msg))
	goto exit;

    sigset = rpmvsCreate(&sigblob, flags);

    /* Initialize digests ranging over the header */
    rpmvsInitDigests(sigset, RPMSIG_HEADER, bundle);

    /* Read the header from the package. */
    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERIMMUTABLE, &blob, &msg))
	goto exit;

    /* Fish interesting tags from the main header. This is a bit hacky... */
    if (!(flags & (RPMVSF_NOPAYLOAD|RPMVSF_NEEDPAYLOAD)))
	rpmvsAppend(sigset, &blob, RPMTAG_PAYLOADDIGEST);

    /* Initialize digests ranging over the payload only */
    rpmvsInitDigests(sigset, RPMSIG_PAYLOAD, bundle);

    /* Verify header signatures and digests */
    failed += rpmvsVerifyItems(sigset, (RPMSIG_HEADER), bundle, keyring, cb, cbdata);

    /* Unless disabled, read the file, generating digest(s) on the fly. */
    if (!(flags & RPMVSF_NEEDPAYLOAD)) {
	if (readFile(fd, &msg))
	    goto exit;
    }

    /* Verify signatures and digests ranging over the payload */
    failed += rpmvsVerifyItems(sigset, (RPMSIG_PAYLOAD), bundle,
			keyring, cb, cbdata);
    failed += rpmvsVerifyItems(sigset, (RPMSIG_HEADER|RPMSIG_PAYLOAD), bundle,
			keyring, cb, cbdata);

    if (failed == 0) {
	/* Finally import the headers and do whatever required retrofits etc */
	if (hdrp) {
	    if (hdrblobImport(&sigblob, 0, &sigh, &msg))
		goto exit;
	    if (hdrblobImport(&blob, 0, &h, &msg))
		goto exit;

	    /* Append (and remap) signature tags to the metadata. */
	    headerMergeLegacySigs(h, sigh);
	    applyRetrofits(h, leadtype);

	    /* Bump reference count for return. */
	    *hdrp = headerLink(h);
	}
	rc = RPMRC_OK;
    }

exit:
    if (rc && msg != NULL)
	rpmlog(RPMLOG_ERR, "%s: %s\n", Fdescr(fd), msg);
    free(msg);
    free(sigblob.ei);
    free(blob.ei);
    headerFree(h);
    headerFree(sigh);
    rpmvsFree(sigset);
    return rc;
}

static int rpmpkgVerifySigs(rpmKeyring keyring, rpmVSFlags flags,
			   FD_t fd, const char *fn)
{
    int rc;
    if (rpmIsVerbose()) {
	rpmlog(RPMLOG_NOTICE, "%s:\n", fn);
	rc = rpmpkgRead(keyring, flags, fd, formatVerbose, NULL, NULL);
    } else {
	struct vfydata_s vd = { 0, 0 };
	rpmlog(RPMLOG_NOTICE, "%s:", fn);
	rc = rpmpkgRead(keyring, flags, fd, formatDefault, &vd, NULL);
	if (vd.seen & RPMSIG_DIGEST_TYPE) {
	    rpmlog(RPMLOG_NOTICE, " %s", (vd.bad & RPMSIG_DIGEST_TYPE) ?
					_("DIGESTS") : _("digests"));
	}
	if (vd.seen & RPMSIG_SIGNATURE_TYPE) {
	    rpmlog(RPMLOG_NOTICE, " %s", (vd.bad & RPMSIG_SIGNATURE_TYPE) ?
					_("SIGNATURES") : _("signatures"));
	}
	rpmlog(RPMLOG_NOTICE, " %s\n", rc ? _("NOT OK") : _("OK"));
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

    vsflags |= rpmcliVSFlags;

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
