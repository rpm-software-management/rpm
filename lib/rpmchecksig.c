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
#include <rpm/rpmkeyring.h>

#include "rpmio/digest.h"
#include "rpmio/rpmio_internal.h" 	/* fdSetBundle() */
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
	fd = Fopen(fn, (flags & O_ACCMODE) == O_WRONLY ? "w.ufdio" : "r.ufdio");
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
    struct rpmtd_s pkt;
    int rc = 1;

    if (headerGet(sig, sigtag, &pkt, HEADERGET_DEFAULT) && pkt.data != NULL) {
	pgpDig dig = pgpNewDig();

	if (!pgpPrtPkts(pkt.data, pkt.count, dig, 0)) {
	    memcpy(signid, dig->signature.signid, sizeof(dig->signature.signid));
	    rc = 0;
	}
     
	dig = pgpFreeDig(dig);
	rpmtdFreeData(&pkt);
    }
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
    int res = -1; /* assume failure */
    int deleting = (qva->qva_mode == RPMSIGN_DEL_SIGNATURE);
    int xx;
    
    if (argv)
    while ((rpm = *argv++) != NULL)
    {
    	rpmRC rc;
	struct rpmtd_s utd;

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
	if (headerGet(sigh, RPMTAG_HEADERSIGNATURES, &utd, HEADERGET_DEFAULT)) {
	    HeaderIterator hi;
	    struct rpmtd_s copytd;
	    Header oh;
	    Header nh;

	    nh = headerNew();
	    if (nh == NULL) {
		rpmtdFreeData(&utd);
		goto exit;
	    }

	    oh = headerCopyLoad(utd.data);
	    hi = headerInitIterator(oh);
	    while (headerNext(hi, &copytd)) {
		if (copytd.data)
		    xx = headerPut(nh, &copytd, HEADERPUT_DEFAULT);
		rpmtdFreeData(&copytd);
	    }
	    hi = headerFreeIterator(hi);
	    oh = headerFree(oh);

	    sigh = headerFree(sigh);
	    sigh = headerLink(nh);
	    nh = headerFree(nh);
	}

	/* Eliminate broken digest values. */
	xx = headerDel(sigh, RPMSIGTAG_BADSHA1_1);
	xx = headerDel(sigh, RPMSIGTAG_BADSHA1_2);

	/* Toss and recalculate header+payload size and digests. */
	{
	    rpmSigTag const sigs[] = { 	RPMSIGTAG_SIZE, 
					RPMSIGTAG_MD5,
				  	RPMSIGTAG_SHA1,
				     };
	    int nsigs = sizeof(sigs) / sizeof(rpmSigTag);
	    for (int i = 0; i < nsigs; i++) {
		(void) headerDel(sigh, sigs[i]);
		if (rpmAddSignature(sigh, sigtarget, sigs[i], qva->passPhrase))
		    goto exit;
	    }
	}

	if (deleting) {	/* Nuke all the signature tags. */
	    xx = headerDel(sigh, RPMSIGTAG_GPG);
	    xx = headerDel(sigh, RPMSIGTAG_DSA);
	    xx = headerDel(sigh, RPMSIGTAG_PGP5);
	    xx = headerDel(sigh, RPMSIGTAG_PGP);
	    xx = headerDel(sigh, RPMSIGTAG_RSA);
	} else		/* If gpg/pgp is configured, replace the signature. */
	if ((sigtag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0) {
	    pgpKeyID_t oldsignid, newsignid;

	    /* Grab the old signature fingerprint (if any) */
	    memset(oldsignid, 0, sizeof(oldsignid));
	    xx = getSignid(sigh, sigtag, oldsignid);

	    switch (sigtag) {
	    case RPMSIGTAG_DSA:
		xx = headerDel(sigh, RPMSIGTAG_GPG);
		break;
	    case RPMSIGTAG_RSA:
		xx = headerDel(sigh, RPMSIGTAG_PGP);
		break;
	    case RPMSIGTAG_GPG:
		xx = headerDel(sigh, RPMSIGTAG_DSA);
	    case RPMSIGTAG_PGP5:
	    case RPMSIGTAG_PGP:
		xx = headerDel(sigh, RPMSIGTAG_RSA);
		break;
	    default:
		break;
	    }

	    xx = headerDel(sigh, sigtag);
	    if (rpmAddSignature(sigh, sigtarget, sigtag, qva->passPhrase)) {
		goto exit;
	    }

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

	/* Move final target into place, restore file permissions. */
	{
	    struct stat st;
	    xx = stat(rpm, &st);
	    xx = unlink(rpm);
	    xx = rename(trpm, rpm);
	    xx = chmod(rpm, st.st_mode);
	}
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

/**
 * @todo If the GPG key was known available, the md5 digest could be skipped.
 */
static int readFile(FD_t fd, const char * fn, pgpDig dig,
		    rpmDigestBundle plbundle, rpmDigestBundle hdrbundle)
{
    unsigned char buf[4*BUFSIZ];
    ssize_t count;
    int rc = 1;
    Header h = NULL;

    /* Read the header from the package. */
    if ((h = headerRead(fd, HEADER_MAGIC_YES)) == NULL) {
	rpmlog(RPMLOG_ERR, _("%s: headerRead failed\n"), fn);
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
    headerFree(h);
    return rc;
}

/* Parse the parameters from the OpenPGP packets that will be needed. */
/* XXX TODO: unify with similar parsePGP() in package.c */
static rpmRC parsePGP(rpmtd sigtd, const char *fn, pgpDig dig)
{
    rpmRC rc = RPMRC_FAIL;
    int debug = (_print_pkts & rpmIsDebug());
    if ((pgpPrtPkts(sigtd->data, sigtd->count, dig, debug) == 0) &&
	 (dig->signature.version == 3 || dig->signature.version == 4)) {
	rc = RPMRC_OK;
    } else {
	rpmlog(RPMLOG_ERR,
	    _("skipping package %s with unverifiable V%u signature\n"), fn,
	    dig->signature.version);
    }
    return rc;
}

/* 
 * Figure best available signature. 
 * XXX TODO: Similar detection in rpmReadPackageFile(), unify these.
 */
static rpmSigTag bestSig(Header sigh, int nosignatures, int nodigests)
{
    rpmSigTag sigtag = 0;
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
static void formatResult(rpmSigTag sigtag, rpmRC sigres, const char *result,
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
    rpmTag sigtag;
    pgpDig dig = NULL;
    pgpDigParams sigp;
    Header sigh = NULL;
    HeaderIterator hi = NULL;
    char * msg = NULL;
    int res = 1; /* assume failure */
    int xx;
    rpmRC rc;
    int failed = 0;
    int nodigests = !(flags & VERIFY_DIGEST);
    int nosignatures = !(flags & VERIFY_SIGNATURE);
    rpmDigestBundle plbundle = rpmDigestBundleNew();
    rpmDigestBundle hdrbundle = rpmDigestBundleNew();

    rpmlead lead = rpmLeadNew();
    if ((rc = rpmLeadRead(fd, lead)) == RPMRC_OK) {
	const char *lmsg = NULL;
	rc = rpmLeadCheck(lead, &lmsg);
	if (rc != RPMRC_OK) 
	    rpmlog(RPMLOG_ERR, "%s: %s\n", fn, lmsg);
    }
    lead = rpmLeadFree(lead);

    if (rc != RPMRC_OK) {
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

    dig = pgpNewDig();
    sigp = &dig->signature;

    /* XXX RSA needs the hash_algo, so decode early. */
    if (sigtag == RPMSIGTAG_RSA || sigtag == RPMSIGTAG_PGP ||
		sigtag == RPMSIGTAG_DSA || sigtag == RPMSIGTAG_GPG) {
	xx = headerGet(sigh, sigtag, &sigtd, HEADERGET_DEFAULT);
	xx = pgpPrtPkts(sigtd.data, sigtd.count, dig, 0);
	rpmtdFreeData(&sigtd);
	/* XXX assume same hash_algo in header-only and header+payload */
	rpmDigestBundleAdd(plbundle, sigp->hash_algo, RPMDIGEST_NONE);
	rpmDigestBundleAdd(hdrbundle, sigp->hash_algo, RPMDIGEST_NONE);
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
    if (readFile(fd, fn, dig, plbundle, hdrbundle)) {
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
	pgpCleanDig(dig);

	switch (sigtd.tag) {
	case RPMSIGTAG_GPG:
	case RPMSIGTAG_PGP5:	/* XXX legacy */
	case RPMSIGTAG_PGP:
	    havekey = 1;
	case RPMSIGTAG_RSA:
	case RPMSIGTAG_DSA:
	    if (nosignatures)
		 continue;
	    if (parsePGP(&sigtd, fn, dig) != RPMRC_OK) {
		goto exit;
	    }
	    ctx = rpmDigestBundleDupCtx(havekey ? plbundle : hdrbundle,
					dig->signature.hash_algo);
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

	rc = rpmVerifySignature(keyring, &sigtd, dig, ctx, &result);
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
    pgpFreeDig(dig);
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

int rpmcliSign(rpmts ts, QVA_t qva, ARGV_const_t argv)
{
    const char * arg;
    int res = 0;
    int xx;
    rpmKeyring keyring = NULL;

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

    keyring = rpmtsGetKeyring(ts, 1);
    while ((arg = *argv++) != NULL) {
	FD_t fd;

	fd = Fopen(arg, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), 
		     arg, Fstrerror(fd));
	    res++;
	} else if (rpmpkgVerifySigs(keyring, qva->qva_flags, fd, arg)) {
	    res++;
	}

	if (fd != NULL) xx = Fclose(fd);
	rpmdbCheckSignals();
    }
    rpmKeyringFree(keyring);

    return res;
}
