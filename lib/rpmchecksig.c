/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include <rpm/rpmlib.h>			/* RPMSIGTAG & related */
#include <rpm/rpmpgp.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmfileutil.h>	/* rpmMkTemp() */
#include <rpm/rpmdb.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>

#include "rpmio/digest.h"
#include "lib/rpmlead.h"
#include "lib/signature.h"

#include "debug.h"

int _print_pkts = 0;

static int closeFile(FD_t *fdp)
{
    if (fdp == NULL || *fdp == NULL)
	return 1;

    /* close and reset *fdp to NULL */
    (void) Fclose(*fdp);
    *fdp = NULL;
    return 0;
}

/**
 */
static int manageFile(FD_t *fdp, const char *fn, int flags)
{
    FD_t fd;

    if (fdp == NULL || fn == NULL)	/* programmer error */
	return 1;

    /* open a file and set *fdp */
    if (*fdp == NULL && fn != NULL) {
	fd = Fopen(fn, ((flags & O_WRONLY) ? "w.ufdio" : "r.ufdio"));
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), fn,
		Fstrerror(fd));
	    return 1;
	}
	*fdp = fd;
	return 0;
    }

    /* no operation */
    if (*fdp != NULL && fn != NULL)
	return 0;

    /* XXX never reached */
    return 1;
}

/**
 * Copy header+payload, calculating digest(s) on the fly.
 */
static int copyFile(FD_t *sfdp, const char *sfnp,
		FD_t *tfdp, const char *tfnp)
{
    unsigned char buf[BUFSIZ];
    ssize_t count;
    int rc = 1;

    if (manageFile(sfdp, sfnp, O_RDONLY))
	goto exit;
    if (manageFile(tfdp, tfnp, O_WRONLY|O_CREAT|O_TRUNC))
	goto exit;

    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), *sfdp)) > 0)
    {
	if (Fwrite(buf, sizeof(buf[0]), count, *tfdp) != count) {
	    rpmlog(RPMLOG_ERR, _("%s: Fwrite failed: %s\n"), tfnp,
		Fstrerror(*tfdp));
	    goto exit;
	}
    }
    if (count < 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fread failed: %s\n"), sfnp, Fstrerror(*sfdp));
	goto exit;
    }
    if (Fflush(*tfdp) != 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fflush failed: %s\n"), tfnp,
	    Fstrerror(*tfdp));
    }

    rc = 0;

exit:
    if (*sfdp)	(void) closeFile(sfdp);
    if (*tfdp)	(void) closeFile(tfdp);
    return rc;
}

/**
 * Retrieve signer fingerprint from an OpenPGP signature tag.
 * @param sig		signature header
 * @param sigtag	signature tag
 * @retval signid	signer fingerprint
 * @return		0 on success
 */
static int getSignid(Header sig, rpmSigTag sigtag, pgpKeyID_t signid)
{
    rpm_data_t pkt = NULL;
    rpmTagType pkttyp = 0;
    rpm_count_t pktlen = 0;
    int rc = 1;

    if (headerGetEntry(sig, sigtag, &pkttyp, &pkt, &pktlen) && pkt != NULL) {
	pgpDig dig = pgpNewDig();

	if (!pgpPrtPkts(pkt, pktlen, dig, 0)) {
	    memcpy(signid, dig->signature.signid, sizeof(dig->signature.signid));
	    rc = 0;
	}
     
	dig = pgpFreeDig(dig);
    }
    pkt = headerFreeData(pkt, pkttyp);
    return rc;
}

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param ts		transaction set
 * @param qva		mode flags and parameters
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success, -1 on error
 */
static int rpmReSign(rpmts ts, QVA_t qva, ARGV_const_t argv)
{
    FD_t fd = NULL;
    FD_t ofd = NULL;
    rpmlead lead;
    rpmSigTag sigtag;
    const char *rpm;
    char *sigtarget = NULL, *trpm = NULL;
    Header sigh = NULL;
    char * msg;
    void * uh = NULL;
    rpmTagType uht;
    rpm_count_t uhc;
    int res = -1; /* assume failure */
    int deleting = (qva->qva_mode == RPMSIGN_DEL_SIGNATURE);
    int xx;
    
    if (argv)
    while ((rpm = *argv++) != NULL)
    {
    	rpmRC rc;

	fprintf(stdout, "%s:\n", rpm);

	if (manageFile(&fd, rpm, O_RDONLY))
	    goto exit;

	lead = rpmLeadNew();

	if ((rc = rpmLeadRead(fd, lead)) == RPMRC_OK) {
	    const char *lmsg = NULL;
	    rc = rpmLeadCheck(lead, &lmsg);
	    if (rc != RPMRC_OK) 
		rpmlog(RPMLOG_ERR, "%s: %s\n", rpm, lmsg);
	}

	if (rc != RPMRC_OK) {
	    lead = rpmLeadFree(lead);
	    goto exit;
	}

	msg = NULL;
	rc = rpmReadSignature(fd, &sigh, RPMSIGTYPE_HEADERSIG, &msg);
	switch (rc) {
	default:
	    rpmlog(RPMLOG_ERR, _("%s: rpmReadSignature failed: %s"), rpm,
			(msg && *msg ? msg : "\n"));
	    msg = _free(msg);
	    goto exit;
	    break;
	case RPMRC_OK:
	    if (sigh == NULL) {
		rpmlog(RPMLOG_ERR, _("%s: No signature available\n"), rpm);
		goto exit;
	    }
	    break;
	}
	msg = _free(msg);

	/* ASSERT: ofd == NULL && sigtarget == NULL */
	ofd = rpmMkTempFile(NULL, &sigtarget);
	if (ofd == NULL || Ferror(ofd)) {
	    rpmlog(RPMLOG_ERR, _("rpmMkTemp failed\n"));
	    goto exit;
	}
	/* Write the header and archive to a temp file */
	if (copyFile(&fd, rpm, &ofd, sigtarget))
	    goto exit;
	/* Both fd and ofd are now closed. sigtarget contains tempfile name. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Dump the immutable region (if present). */
	if (headerGetEntry(sigh, RPMTAG_HEADERSIGNATURES, &uht, &uh, &uhc)) {
	    HeaderIterator hi;
	    rpmTagType type;
	    rpmTag tag;
	    rpm_count_t count;
	    rpm_data_t ptr;
	    Header oh;
	    Header nh;

	    nh = headerNew();
	    if (nh == NULL) {
		uh = headerFreeData(uh, uht);
		goto exit;
	    }

	    oh = headerCopyLoad(uh);
	    for (hi = headerInitIterator(oh);
		headerNextIterator(hi, &tag, &type, &ptr, &count);
		ptr = headerFreeData(ptr, type))
	    {
		if (ptr)
		    xx = headerAddEntry(nh, tag, type, ptr, count);
	    }
	    hi = headerFreeIterator(hi);
	    oh = headerFree(oh);

	    sigh = headerFree(sigh);
	    sigh = headerLink(nh);
	    nh = headerFree(nh);
	}

	/* Eliminate broken digest values. */
	xx = headerRemoveEntry(sigh, RPMSIGTAG_LEMD5_1);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_LEMD5_2);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_BADSHA1_1);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_BADSHA1_2);

	/* Toss and recalculate header+payload size and digests. */
	xx = headerRemoveEntry(sigh, RPMSIGTAG_SIZE);
	xx = rpmAddSignature(sigh, sigtarget, RPMSIGTAG_SIZE, qva->passPhrase);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_MD5);
	xx = rpmAddSignature(sigh, sigtarget, RPMSIGTAG_MD5, qva->passPhrase);
	xx = headerRemoveEntry(sigh, RPMSIGTAG_SHA1);
	xx = rpmAddSignature(sigh, sigtarget, RPMSIGTAG_SHA1, qva->passPhrase);

	if (deleting) {	/* Nuke all the signature tags. */
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_GPG);
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_DSA);
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_PGP5);
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_PGP);
	    xx = headerRemoveEntry(sigh, RPMSIGTAG_RSA);
	} else		/* If gpg/pgp is configured, replace the signature. */
	if ((sigtag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0) {
	    pgpKeyID_t oldsignid, newsignid;

	    /* Grab the old signature fingerprint (if any) */
	    memset(oldsignid, 0, sizeof(oldsignid));
	    xx = getSignid(sigh, sigtag, oldsignid);

	    switch (sigtag) {
	    case RPMSIGTAG_DSA:
		xx = headerRemoveEntry(sigh, RPMSIGTAG_GPG);
		break;
	    case RPMSIGTAG_RSA:
		xx = headerRemoveEntry(sigh, RPMSIGTAG_PGP);
		break;
	    case RPMSIGTAG_GPG:
		xx = headerRemoveEntry(sigh, RPMSIGTAG_DSA);
	    case RPMSIGTAG_PGP5:
	    case RPMSIGTAG_PGP:
		xx = headerRemoveEntry(sigh, RPMSIGTAG_RSA);
		break;
	    /* shut up gcc */
	    case RPMSIGTAG_SHA1:
	    case RPMSIGTAG_MD5:
	    case RPMSIGTAG_LEMD5_1:
	    case RPMSIGTAG_LEMD5_2:
	    case RPMSIGTAG_BADSHA1_1:
	    case RPMSIGTAG_BADSHA1_2:
	    case RPMSIGTAG_PAYLOADSIZE:
	    case RPMSIGTAG_SIZE:
		break;
	    }

	    xx = headerRemoveEntry(sigh, sigtag);
	    xx = rpmAddSignature(sigh, sigtarget, sigtag, qva->passPhrase);

	    /* If package was previously signed, check for same signer. */
	    memset(newsignid, 0, sizeof(newsignid));
	    if (memcmp(oldsignid, newsignid, sizeof(oldsignid))) {

		/* Grab the new signature fingerprint */
		xx = getSignid(sigh, sigtag, newsignid);

		/* If same signer, skip resigning the package. */
		if (!memcmp(oldsignid, newsignid, sizeof(oldsignid))) {

		    char *signid = pgpHexStr(newsignid+4, sizeof(newsignid)-4);
		    rpmlog(RPMLOG_WARNING,
			_("%s: was already signed by key ID %s, skipping\n"),
			rpm, signid);
		    free(signid);

		    /* Clean up intermediate target */
		    xx = unlink(sigtarget);
		    sigtarget = _free(sigtarget);
		    continue;
		}
	    }
	}

	/* Reallocate the signature into one contiguous region. */
	sigh = headerReload(sigh, RPMTAG_HEADERSIGNATURES);
	if (sigh == NULL)	/* XXX can't happen */
	    goto exit;

	rasprintf(&trpm, "%s.XXXXXX", rpm);
	ofd = rpmMkTemp(trpm);
	if (ofd == NULL || Ferror(ofd)) {
	    rpmlog(RPMLOG_ERR, _("rpmMkTemp failed\n"));
	    goto exit;
	}

	/* Write the lead/signature of the output rpm */
	rc = rpmLeadWrite(ofd, lead);
	lead = rpmLeadFree(lead);
	if (rc != RPMRC_OK) {
	    rpmlog(RPMLOG_ERR, _("%s: writeLead failed: %s\n"), trpm,
		Fstrerror(ofd));
	    goto exit;
	}

	if (rpmWriteSignature(ofd, sigh)) {
	    rpmlog(RPMLOG_ERR, _("%s: rpmWriteSignature failed: %s\n"), trpm,
		Fstrerror(ofd));
	    goto exit;
	}

	/* Append the header and archive from the temp file */
	/* ASSERT: fd == NULL && ofd != NULL */
	if (copyFile(&fd, sigtarget, &ofd, trpm))
	    goto exit;
	/* Both fd and ofd are now closed. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Move final target into place. */
	xx = unlink(rpm);
	xx = rename(trpm, rpm);
	trpm = _free(trpm);

	/* Clean up intermediate target */
	xx = unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }

    res = 0;

exit:
    if (fd)	(void) closeFile(&fd);
    if (ofd)	(void) closeFile(&ofd);

    sigh = rpmFreeSignature(sigh);

    if (sigtarget) {
	xx = unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }
    if (trpm) {
	(void) unlink(trpm);
	free(trpm);
    }

    return res;
}

/** \ingroup rpmcli
 * Import public key(s).
 * @todo Implicit --update policy for gpg-pubkey headers.
 * @param ts		transaction set
 * @param qva		mode flags and parameters
 * @param argv		array of pubkey file names (NULL terminated)
 * @return		0 on success
 */
static int rpmcliImportPubkeys(const rpmts ts, QVA_t qva, ARGV_const_t argv)
{
    const char * fn;
    unsigned char * pkt = NULL;
    size_t pktlen = 0;
    char * t = NULL;
    int res = 0;
    rpmRC rpmrc;
    int rc;

    if (argv == NULL) return res;

    while ((fn = *argv++) != NULL) {

	rpmtsClean(ts);
	pkt = _free(pkt);
	t = _free(t);

	/* If arg looks like a keyid, then attempt keyserver retrieve. */
	if (fn[0] == '0' && fn[1] == 'x') {
	    const char * s;
	    int i;
	    for (i = 0, s = fn+2; *s && isxdigit(*s); s++, i++)
		{};
	    if (i == 8 || i == 16) {
		t = rpmExpand("%{_hkp_keyserver_query}", fn+2, NULL);
		if (t && *t != '%')
		    fn = t;
	    }
	}

	/* Read pgp packet. */
	if ((rc = pgpReadPkts(fn, &pkt, &pktlen)) <= 0) {
	    rpmlog(RPMLOG_ERR, _("%s: import read failed(%d).\n"), fn, rc);
	    res++;
	    continue;
	}
	if (rc != PGPARMOR_PUBKEY) {
	    rpmlog(RPMLOG_ERR, _("%s: not an armored public key.\n"), fn);
	    res++;
	    continue;
	}

	/* Import pubkey packet(s). */
	if ((rpmrc = rpmtsImportPubkey(ts, pkt, pktlen)) != RPMRC_OK) {
	    rpmlog(RPMLOG_ERR, _("%s: import failed.\n"), fn);
	    res++;
	    continue;
	}

    }
    
rpmtsClean(ts);
    pkt = _free(pkt);
    t = _free(t);
    return res;
}

static unsigned char const header_magic[8] = {
        0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/**
 * @todo If the GPG key was known available, the md5 digest could be skipped.
 */
static int readFile(FD_t fd, const char * fn, pgpDig dig)
{
    unsigned char buf[4*BUFSIZ];
    ssize_t count;
    int rc = 1;

    dig->nbytes = 0;

    /* Read the header from the package. */
    {	Header h = headerRead(fd, HEADER_MAGIC_YES);
	if (h == NULL) {
	    rpmlog(RPMLOG_ERR, _("%s: headerRead failed\n"), fn);
	    goto exit;
	}

	dig->nbytes += headerSizeof(h, HEADER_MAGIC_YES);

	if (headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	    void * uh;
	    rpmTagType uht;
	    rpm_count_t uhc;
	
	    if (!headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc)
	    ||   uh == NULL)
	    {
		h = headerFree(h);
		rpmlog(RPMLOG_ERR, _("%s: headerGetEntry failed\n"), fn);
		goto exit;
	    }
	    dig->hdrsha1ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	    (void) rpmDigestUpdate(dig->hdrsha1ctx, header_magic, sizeof(header_magic));
	    (void) rpmDigestUpdate(dig->hdrsha1ctx, uh, uhc);
	    dig->hdrmd5ctx = rpmDigestInit(dig->signature.hash_algo, RPMDIGEST_NONE);
	    (void) rpmDigestUpdate(dig->hdrmd5ctx, header_magic, sizeof(header_magic));
	    (void) rpmDigestUpdate(dig->hdrmd5ctx, uh, uhc);
	    uh = headerFreeData(uh, uht);
	}
	h = headerFree(h);
    }

    /* Read the payload from the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	dig->nbytes += count;
    if (count < 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fread failed: %s\n"), fn, Fstrerror(fd));
	goto exit;
    }
    fdStealDigest(fd, dig);

    rc = 0;

exit:
    return rc;
}

static const char *sigtagname(rpmSigTag sigtag, int upper)
{
    const char *n = NULL;

    switch (sigtag) {
    case RPMSIGTAG_SIZE:
	n = (upper ? "SIZE" : "size");
	break;
    case RPMSIGTAG_SHA1:
	n = (upper ? "SHA1" : "sha1");
	break;
    case RPMSIGTAG_LEMD5_2:
    case RPMSIGTAG_LEMD5_1:
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

int rpmVerifySignatures(QVA_t qva, rpmts ts, FD_t fd,
		const char * fn)
{
    char *buf = NULL, *b;
    char * missingKeys, *untrustedKeys;
    rpmTag sigtag;
    rpmTagType sigtype;
    rpm_data_t sig;
    pgpDig dig;
    pgpDigParams sigp;
    rpm_count_t siglen;
    Header sigh = NULL;
    HeaderIterator hi;
    char * msg;
    int res = 0;
    int xx;
    rpmRC rc, sigres;
    int failed;
    int nodigests = !(qva->qva_flags & VERIFY_DIGEST);
    int nosignatures = !(qva->qva_flags & VERIFY_SIGNATURE);

    {
	rpmlead lead = rpmLeadNew();
    	if ((rc = rpmLeadRead(fd, lead)) == RPMRC_OK) {
	    const char *lmsg = NULL;
	    rc = rpmLeadCheck(lead, &lmsg);
	    if (rc != RPMRC_OK) 
		rpmlog(RPMLOG_ERR, "%s: %s\n", fn, lmsg);
    	}
    	lead = rpmLeadFree(lead);

    	if (rc != RPMRC_OK) {
	    res++;
	    goto exit;
	}


	msg = NULL;
	rc = rpmReadSignature(fd, &sigh, RPMSIGTYPE_HEADERSIG, &msg);
	switch (rc) {
	default:
	    rpmlog(RPMLOG_ERR, _("%s: rpmReadSignature failed: %s"), fn,
			(msg && *msg ? msg : "\n"));
	    msg = _free(msg);
	    res++;
	    goto exit;
	    break;
	case RPMRC_OK:
	    if (sigh == NULL) {
		rpmlog(RPMLOG_ERR, _("%s: No signature available\n"), fn);
		res++;
		goto exit;
	    }
	    break;
	}
	msg = _free(msg);

	/* Grab a hint of what needs doing to avoid duplication. */
	sigtag = 0;
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

	dig = rpmtsDig(ts);
	sigp = rpmtsSignature(ts);

	/* XXX RSA needs the hash_algo, so decode early. */
	if (sigtag == RPMSIGTAG_RSA || sigtag == RPMSIGTAG_PGP) {
	    xx = headerGetEntry(sigh, sigtag, &sigtype, (rpm_data_t *)&sig, &siglen);
	    xx = pgpPrtPkts(sig, siglen, dig, 0);
	    sig = headerFreeData(sig, sigtype);
	    /* XXX assume same hash_algo in header-only and header+payload */
	    if ((headerIsEntry(sigh, RPMSIGTAG_PGP)
	      || headerIsEntry(sigh, RPMSIGTAG_PGP5))
	     && dig->signature.hash_algo != PGPHASHALGO_MD5)
		fdInitDigest(fd, dig->signature.hash_algo, 0);
	}

	if (headerIsEntry(sigh, RPMSIGTAG_PGP)
	||  headerIsEntry(sigh, RPMSIGTAG_PGP5)
	||  headerIsEntry(sigh, RPMSIGTAG_MD5))
	    fdInitDigest(fd, PGPHASHALGO_MD5, 0);
	if (headerIsEntry(sigh, RPMSIGTAG_GPG))
	    fdInitDigest(fd, PGPHASHALGO_SHA1, 0);

	/* Read the file, generating digest(s) on the fly. */
	if (dig == NULL || sigp == NULL || readFile(fd, fn, dig)) {
	    res++;
	    goto exit;
	}

	failed = 0;
	missingKeys = NULL;
	untrustedKeys = NULL;
	rasprintf(&buf, "%s:%c", fn, (rpmIsVerbose() ? '\n' : ' ') );

	for (hi = headerInitIterator(sigh);
	    headerNextIterator(hi, &sigtag, &sigtype, &sig, &siglen) != 0;
	    (void) rpmtsSetSig(ts, sigtag, sigtype, NULL, siglen))
	{
    	    char *result = NULL;
	    int havekey = 0;

	    if (sig == NULL) /* XXX can't happen */
		continue;

	    (void) rpmtsSetSig(ts, sigtag, sigtype, sig, siglen);

	    /* Clean up parameters from previous sigtag. */
	    pgpCleanDig(dig);

	    switch (sigtag) {
	    case RPMSIGTAG_GPG:
	    case RPMSIGTAG_PGP5:	/* XXX legacy */
	    case RPMSIGTAG_PGP:
		havekey = 1;
	    case RPMSIGTAG_RSA:
	    case RPMSIGTAG_DSA:
		if (nosignatures)
		     continue;
		xx = pgpPrtPkts(sig, siglen, dig,
			(_print_pkts & rpmIsDebug()));

		if (sigp->version != 3 && sigp->version != 4) {
		    rpmlog(RPMLOG_NOTICE,
		_("skipping package %s with unverifiable V%u signature\n"),
			fn, sigp->version);
		    res++;
		    goto exit;
		}
		break;
	    case RPMSIGTAG_SHA1:
		if (nodigests)
		     continue;
		/* XXX Don't bother with header sha1 if header dsa. */
		if (!nosignatures && sigtag == RPMSIGTAG_DSA)
		    continue;
		break;
	    case RPMSIGTAG_LEMD5_2:
	    case RPMSIGTAG_LEMD5_1:
	    case RPMSIGTAG_MD5:
		if (nodigests)
		     continue;
		/*
		 * Don't bother with md5 if pgp, as RSA/MD5 is more reliable
		 * than the -- now unsupported -- legacy md5 breakage.
		 */
		if (!nosignatures && sigtag == RPMSIGTAG_PGP)
		    continue;
		break;
	    default:
		continue;
		break;
	    }

	    sigres = rpmVerifySignature(ts, &result);
	    if (sigres != RPMRC_OK) {
		failed = 1;
	    }

	    /* 
 	     * In verbose mode, just dump it all. Otherwise ok signatures
 	     * are dumped lowercase, bad sigs uppercase and for PGP/GPG
 	     * if misssing/untrusted key it's uppercase in parenthesis
 	     * and stash the key id as <SIGTYPE>#<keyid>. Pfft.
 	     */
	    msg = NULL;
	    if (rpmIsVerbose()) {
		rasprintf(&msg, "    %s", result);
	    } else { 
		const char *signame;
		char ** keyprob = NULL;
 		signame = sigtagname(sigtag, (sigres == RPMRC_OK ? 0 : 1));

		/* 
 		 * Check for missing / untrusted keys in result. In theory
 		 * there could be several missing keys of which only
 		 * last is shown, in practise not.
 		 */
		 if (havekey && 
		     (sigres == RPMRC_NOKEY || sigres == RPMRC_NOTTRUSTED)) {
		     const char *tempKey = NULL;
		     char *keyid = NULL;
		     keyprob = (sigres == RPMRC_NOKEY ? 
				&missingKeys : &untrustedKeys);
		     if (*keyprob) free(*keyprob);
		     tempKey = strstr(result, "ey ID");
		     if (tempKey) 
			keyid = strndup(tempKey + 6, 8);
		     rasprintf(keyprob, "%s#%s", signame, keyid);
		     free(keyid);
		 }
		 rasprintf(&msg, (keyprob ? "(%s) " : "%s "), signame);
	    }
	    free(result);

	    rasprintf(&b, "%s%s", buf, msg);
	    free(buf);
	    free(msg);
	    buf = b;
	}
	hi = headerFreeIterator(hi);

	res += failed;

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
        free(buf);
        free(missingKeys);
        free(untrustedKeys);
    }

exit:
    sigh = rpmFreeSignature(sigh);
    rpmtsCleanDig(ts);
    return res;
}

int rpmcliSign(rpmts ts, QVA_t qva, ARGV_const_t argv)
{
    const char * arg;
    int res = 0;
    int xx;

    if (argv == NULL) return res;

    switch (qva->qva_mode) {
    case RPMSIGN_CHK_SIGNATURE:
	break;
    case RPMSIGN_IMPORT_PUBKEY:
	return rpmcliImportPubkeys(ts, qva, argv);
	break;
    case RPMSIGN_NEW_SIGNATURE:
    case RPMSIGN_ADD_SIGNATURE:
    case RPMSIGN_DEL_SIGNATURE:
	return rpmReSign(ts, qva, argv);
	break;
    case RPMSIGN_NONE:
    default:
	return -1;
	break;
    }

    while ((arg = *argv++) != NULL) {
	FD_t fd;

	fd = Fopen(arg, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), 
		     arg, Fstrerror(fd));
	    res++;
	} else if (rpmVerifySignatures(qva, ts, fd, arg)) {
	    res++;
	}

	if (fd != NULL) xx = Fclose(fd);
    }

    return res;
}
