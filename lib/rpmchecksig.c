/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmcli.h>
#include "rpmpgp.h"

#include "rpmts.h"

#include "rpmlead.h"
#include "signature.h"
#include "misc.h"	/* XXX for makeTempFile() */
#include "debug.h"

/*@access rpmTransactionSet @*/	/* ts->rpmdb, ts->id, ts->dig et al */
/*?access Header @*/		/* XXX compared with NULL */
/*@access FD_t @*/		/* XXX stealing digests */
/*@access pgpDig @*/

/*@unchecked@*/
static int _print_pkts = 0;

/**
 */
static int manageFile(FD_t *fdp, const char **fnp, int flags,
		/*@unused@*/ int rc)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *fdp, *fnp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    const char *fn;
    FD_t fd;

    if (fdp == NULL) {	/* programmer error */
	return 1;
    }

    /* close and reset *fdp to NULL */
    if (*fdp && (fnp == NULL || *fnp == NULL)) {
	(void) Fclose(*fdp);
	*fdp = NULL;
	return 0;
    }

    /* open a file and set *fdp */
    if (*fdp == NULL && fnp && *fnp) {
	fd = Fopen(*fnp, ((flags & O_WRONLY) ? "w.ufdio" : "r.ufdio"));
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("%s: open failed: %s\n"), *fnp,
		Fstrerror(fd));
	    return 1;
	}
	*fdp = fd;
	return 0;
    }

    /* open a temp file */
    if (*fdp == NULL && (fnp == NULL || *fnp == NULL)) {
	fn = NULL;
	if (makeTempFile(NULL, (fnp ? &fn : NULL), &fd)) {
	    rpmError(RPMERR_MAKETEMP, _("makeTempFile failed\n"));
	    return 1;
	}
	if (fnp)
	    *fnp = fn;
	*fdp = fdLink(fd, "manageFile return");
	fd = fdFree(fd, "manageFile return");
	return 0;
    }

    /* no operation */
    if (*fdp && fnp && *fnp) {
	return 0;
    }

    /* XXX never reached */
    return 1;
}

/**
 * Copy header+payload, calculating digest(s) on the fly.
 */
static int copyFile(FD_t *sfdp, const char **sfnp,
		FD_t *tfdp, const char **tfnp)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *sfdp, *sfnp, *tfdp, *tfnp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    unsigned char buf[BUFSIZ];
    ssize_t count;
    int rc = 1;

    if (manageFile(sfdp, sfnp, O_RDONLY, 0))
	goto exit;
    if (manageFile(tfdp, tfnp, O_WRONLY|O_CREAT|O_TRUNC, 0))
	goto exit;

    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), *sfdp)) > 0)
    {
	if (Fwrite(buf, sizeof(buf[0]), count, *tfdp) != count) {
	    rpmError(RPMERR_FWRITE, _("%s: Fwrite failed: %s\n"), *tfnp,
		Fstrerror(*tfdp));
	    goto exit;
	}
    }
    if (count < 0) {
	rpmError(RPMERR_FREAD, _("%s: Fread failed: %s\n"), *sfnp, Fstrerror(*sfdp));
	goto exit;
    }

    rc = 0;

exit:
    if (*sfdp)	(void) manageFile(sfdp, NULL, 0, rc);
    if (*tfdp)	(void) manageFile(tfdp, NULL, 0, rc);
    return rc;
}

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param ts		transaction set
 * @param qva		mode flags and parameters
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success
 */
static int rpmReSign(/*@unused@*/ rpmTransactionSet ts,
		QVA_t qva, const char ** argv)
        /*@globals rpmGlobalMacroContext,
                fileSystem, internalState @*/
        /*@modifies rpmGlobalMacroContext,
                fileSystem, internalState @*/
{
    FD_t fd = NULL;
    FD_t ofd = NULL;
    struct rpmlead lead, *l = &lead;
    int_32 sigtag;
    const char *rpm, *trpm;
    const char *sigtarget = NULL;
    char tmprpm[1024+1];
    Header sig = NULL;
    void * uh = NULL;
    int_32 uht, uhc;
    int res = EXIT_FAILURE;
    rpmRC rc;
    int xx;
    
    tmprpm[0] = '\0';
    /*@-branchstate@*/
    if (argv)
    while ((rpm = *argv++) != NULL) {

	fprintf(stdout, "%s:\n", rpm);

	if (manageFile(&fd, &rpm, O_RDONLY, 0))
	    goto exit;

	memset(l, 0, sizeof(*l));
	if (readLead(fd, l)) {
	    rpmError(RPMERR_READLEAD, _("%s: readLead failed\n"), rpm);
	    goto exit;
	}
	switch (l->major) {
	case 1:
	    rpmError(RPMERR_BADSIGTYPE, _("%s: Can't sign v1 packaging\n"), rpm);
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case 2:
	    rpmError(RPMERR_BADSIGTYPE, _("%s: Can't re-sign v2 packaging\n"), rpm);
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}

	rc = rpmReadSignature(fd, &sig, l->signature_type);
	if (!(rc == RPMRC_OK || rc == RPMRC_BADSIZE)) {
	    rpmError(RPMERR_SIGGEN, _("%s: rpmReadSignature failed\n"), rpm);
	    goto exit;
	}
	if (sig == NULL) {
	    rpmError(RPMERR_SIGGEN, _("%s: No signature available\n"), rpm);
	    goto exit;
	}

	/* Write the header and archive to a temp file */
	/* ASSERT: ofd == NULL && sigtarget == NULL */
	if (copyFile(&fd, &rpm, &ofd, &sigtarget))
	    goto exit;
	/* Both fd and ofd are now closed. sigtarget contains tempfile name. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Dump the immutable region (if present). */
	if (headerGetEntry(sig, RPMTAG_HEADERSIGNATURES, &uht, &uh, &uhc)) {
	    HeaderIterator hi;
	    int_32 tag, type, count;
	    hPTR_t ptr;
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
	    oh = headerFree(oh, NULL);

	    sig = headerFree(sig, NULL);
	    sig = headerLink(nh, NULL);
	    nh = headerFree(nh, NULL);
	}

	/* Eliminate broken digest values. */
	xx = headerRemoveEntry(sig, RPMSIGTAG_LEMD5_1);
	xx = headerRemoveEntry(sig, RPMSIGTAG_LEMD5_2);
	xx = headerRemoveEntry(sig, RPMSIGTAG_BADSHA1_1);
	xx = headerRemoveEntry(sig, RPMSIGTAG_BADSHA1_2);

	/* Toss and recalculate header+payload size and digests. */
	xx = headerRemoveEntry(sig, RPMSIGTAG_SIZE);
	xx = rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, qva->passPhrase);
	xx = headerRemoveEntry(sig, RPMSIGTAG_MD5);
	xx = rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, qva->passPhrase);
	xx = headerRemoveEntry(sig, RPMSIGTAG_SHA1);
	xx = rpmAddSignature(sig, sigtarget, RPMSIGTAG_SHA1, qva->passPhrase);

	/* If gpg/pgp is configured, replace the signature. */
	if ((sigtag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0) {
	    switch (sigtag) {
	    case RPMSIGTAG_GPG:
		xx = headerRemoveEntry(sig, RPMSIGTAG_DSA);
		/*@fallthrough@*/
	    case RPMSIGTAG_PGP5:
	    case RPMSIGTAG_PGP:
		xx = headerRemoveEntry(sig, RPMSIGTAG_RSA);
		/*@switchbreak@*/ break;
	    }
	    xx = headerRemoveEntry(sig, sigtag);
	    xx = rpmAddSignature(sig, sigtarget, sigtag, qva->passPhrase);
	}

	/* Reallocate the signature into one contiguous region. */
	sig = headerReload(sig, RPMTAG_HEADERSIGNATURES);
	if (sig == NULL)	/* XXX can't happen */
	    goto exit;

	/* Write the lead/signature of the output rpm */
	strcpy(tmprpm, rpm);
	strcat(tmprpm, ".XXXXXX");
	(void) mktemp(tmprpm);
	trpm = tmprpm;

	if (manageFile(&ofd, &trpm, O_WRONLY|O_CREAT|O_TRUNC, 0))
	    goto exit;

	l->signature_type = RPMSIGTYPE_HEADERSIG;
	if (writeLead(ofd, l)) {
	    rpmError(RPMERR_WRITELEAD, _("%s: writeLead failed: %s\n"), trpm,
		Fstrerror(ofd));
	    goto exit;
	}

	if (rpmWriteSignature(ofd, sig)) {
	    rpmError(RPMERR_SIGGEN, _("%s: rpmWriteSignature failed: %s\n"), trpm,
		Fstrerror(ofd));
	    goto exit;
	}

	/* Append the header and archive from the temp file */
	/* ASSERT: fd == NULL && ofd != NULL */
	if (copyFile(&fd, &sigtarget, &ofd, &trpm))
	    goto exit;
	/* Both fd and ofd are now closed. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Clean up intermediate target */
	xx = unlink(sigtarget);
	sigtarget = _free(sigtarget);

	/* Move final target into place. */
	xx = unlink(rpm);
	xx = rename(trpm, rpm);
	tmprpm[0] = '\0';
    }
    /*@=branchstate@*/

    res = 0;

exit:
    if (fd)	(void) manageFile(&fd, NULL, 0, res);
    if (ofd)	(void) manageFile(&ofd, NULL, 0, res);

    sig = rpmFreeSignature(sig);

    if (sigtarget) {
	xx = unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }
    if (tmprpm[0] != '\0') {
	xx = unlink(tmprpm);
	tmprpm[0] = '\0';
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
static int rpmImportPubkey(rpmTransactionSet ts,
		/*@unused@*/ QVA_t qva,
		/*@null@*/ const char ** argv)
	/*@globals RPMVERSION, fileSystem, internalState @*/
	/*@modifies ts, fileSystem, internalState @*/
{
    const char * fn;
    int res = 0;
    const char * afmt = "%{pubkeys:armor}";
    const char * group = "Public Keys";
    const char * license = "pubkey";
    const char * buildhost = "localhost";
    int_32 pflags = (RPMSENSE_KEYRING|RPMSENSE_EQUAL);
    int_32 zero = 0;
    pgpDig dig = NULL;
    struct pgpDigParams_s *digp = NULL;
    int rc, xx;

    if (argv == NULL) return res;

    /*@-branchstate@*/
    while ((fn = *argv++) != NULL) {
	const char * d = NULL;
	const char * enc = NULL;
	const char * n = NULL;
	const char * u = NULL;
	const char * v = NULL;
	const char * r = NULL;
	const char * evr = NULL;
	const byte * pkt = NULL;
	char * t;
	ssize_t pktlen = 0;
	Header h = NULL;

	/* Read pgp packet. */
	if ((rc =  pgpReadPkts(fn, &pkt, &pktlen)) <= 0
	 ||  rc != PGPARMOR_PUBKEY
	 || (enc = b64encode(pkt, pktlen)) == NULL)
	{
	    res++;
	    goto bottom;
	}

	dig = pgpNewDig();

	/* Build header elements. */
	(void) pgpPrtPkts(pkt, pktlen, dig, 0);
	digp = &dig->pubkey;

	v = t = xmalloc(16+1);
	t = stpcpy(t, pgpHexStr(digp->signid, sizeof(digp->signid)));

	r = t = xmalloc(8+1);
	t = stpcpy(t, pgpHexStr(digp->time, sizeof(digp->time)));

	n = t = xmalloc(sizeof("gpg()")+8);
	t = stpcpy( stpcpy( stpcpy(t, "gpg("), v+8), ")");

	/*@-nullpass@*/ /* FIX: digp->userid may be NULL */
	u = t = xmalloc(sizeof("gpg()")+strlen(digp->userid));
	t = stpcpy( stpcpy( stpcpy(t, "gpg("), digp->userid), ")");
	/*@=nullpass@*/

	evr = t = xmalloc(sizeof("4X:-")+strlen(v)+strlen(r));
	t = stpcpy(t, (digp->version == 4 ? "4:" : "3:"));
	t = stpcpy( stpcpy( stpcpy(t, v), "-"), r);

	/* Check for pre-existing header. */

	/* Build pubkey header. */
	h = headerNew();

	xx = headerAddOrAppendEntry(h, RPMTAG_PUBKEYS,
			RPM_STRING_ARRAY_TYPE, &enc, 1);

	d = headerSprintf(h, afmt, rpmTagTable, rpmHeaderFormats, NULL);
	if (d == NULL) {
	    res++;
	    goto bottom;
	}

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

	xx = headerAddEntry(h, RPMTAG_INSTALLTIME, RPM_INT32_TYPE, &ts->id, 1);
	
	/* XXX W2DO: tag value inheirited from parent? */
	xx = headerAddEntry(h, RPMTAG_BUILDTIME, RPM_INT32_TYPE, &ts->id, 1);

#ifdef	NOTYET
	/* XXX W2DO: tag value inheirited from parent? */
	xx = headerAddEntry(h, RPMTAG_SOURCERPM, RPM_STRING_TYPE, fn, 1);
#endif

	/* Add header to database. */
	xx = rpmdbAdd(ts->rpmdb, ts->id, h);

bottom:
	/* Clean up. */
	h = headerFree(h, "ImportPubkey");
	dig = pgpFreeDig(dig);
	pkt = _free(pkt);
	n = _free(n);
	u = _free(u);
	v = _free(v);
	r = _free(r);
	evr = _free(evr);
	enc = _free(enc);
	d = _free(d);
    }
    /*@=branchstate@*/
    
    return res;
}

/*@unchecked@*/
static unsigned char header_magic[8] = {
        0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/**
 * @todo If the GPG key was known available, the md5 digest could be skipped.
 */
static int readFile(FD_t fd, const char * fn, pgpDig dig)
	/*@globals fileSystem, internalState @*/
	/*@modifies fd, *dig, fileSystem, internalState @*/
{
    byte buf[4*BUFSIZ];
    ssize_t count;
    int rc = 1;
    int i;

    dig->nbytes = 0;

    /* Read the header from the package. */
    {	Header h = headerRead(fd, HEADER_MAGIC_YES);

	if (h == NULL) {
	    rpmError(RPMERR_FREAD, _("%s: headerRead failed\n"), fn);
	    goto exit;
	}

	dig->nbytes += headerSizeof(h, HEADER_MAGIC_YES);

	if (headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	    void * uh;
	    int_32 uht, uhc;
	
	    if (!headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc)
	    ||   uh == NULL)
	    {
		h = headerFree(h, NULL);
		rpmError(RPMERR_FREAD, _("%s: headerGetEntry failed\n"), fn);
		goto exit;
	    }
	    dig->hdrsha1ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	    (void) rpmDigestUpdate(dig->hdrsha1ctx, header_magic, sizeof(header_magic));
	    (void) rpmDigestUpdate(dig->hdrsha1ctx, uh, uhc);
	    uh = headerFreeData(uh, uht);
	}
	h = headerFree(h, NULL);
    }

    /* Read the payload from the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) > 0)
	dig->nbytes += count;
    if (count < 0) {
	rpmError(RPMERR_FREAD, _("%s: Fread failed: %s\n"), fn, Fstrerror(fd));
	goto exit;
    }

    /* XXX Steal the digest-in-progress from the file handle. */
    for (i = fd->ndigests - 1; i >= 0; i--) {
	FDDIGEST_t fddig = fd->digests + i;
	if (fddig->hashctx == NULL)
	    continue;
	if (fddig->hashalgo == PGPHASHALGO_MD5) {
assert(dig->md5ctx == NULL);
	    dig->md5ctx = fddig->hashctx;
	    fddig->hashctx = NULL;
	    continue;
	}
	if (fddig->hashalgo == PGPHASHALGO_SHA1) {
assert(dig->sha1ctx == NULL);
	    dig->sha1ctx = fddig->hashctx;
	    fddig->hashctx = NULL;
	    continue;
	}
    }

    rc = 0;

exit:
    return rc;
}

int rpmVerifySignatures(QVA_t qva, rpmTransactionSet ts, FD_t fd,
		const char * fn)
{
    int res2, res3;
    struct rpmlead lead, *l = &lead;
    char result[1024];
    char buf[8192], * b;
    char missingKeys[7164], * m;
    char untrustedKeys[7164], * u;
    int_32 sigtag;
    Header sig;
    HeaderIterator hi;
    int res = 0;
    int xx;
    rpmRC rc;
    int nodigests = !(qva->qva_flags & VERIFY_DIGEST);
    int nosignatures = !(qva->qva_flags & VERIFY_SIGNATURE);

    {
	memset(l, 0, sizeof(*l));
	if (readLead(fd, l)) {
	    rpmError(RPMERR_READLEAD, _("%s: readLead failed\n"), fn);
	    res++;
	    goto bottom;
	}
	switch (l->major) {
	case 1:
	    rpmError(RPMERR_BADSIGTYPE, _("%s: No signature available (v1.0 RPM)\n"), fn);
	    res++;
	    goto bottom;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}

	rc = rpmReadSignature(fd, &sig, l->signature_type);
	if (!(rc == RPMRC_OK || rc == RPMRC_BADSIZE)) {
	    rpmError(RPMERR_SIGGEN, _("%s: rpmReadSignature failed\n"), fn);
	    res++;
	    goto bottom;
	}
	if (sig == NULL) {
	    rpmError(RPMERR_SIGGEN, _("%s: No signature available\n"), fn);
	    res++;
	    goto bottom;
	}

	/* Grab a hint of what needs doing to avoid duplication. */
	sigtag = 0;
	if (sigtag == 0 && !nosignatures) {
	    if (headerIsEntry(sig, RPMSIGTAG_DSA))
		sigtag = RPMSIGTAG_DSA;
	    else if (headerIsEntry(sig, RPMSIGTAG_RSA))
		sigtag = RPMSIGTAG_RSA;
	    else if (headerIsEntry(sig, RPMSIGTAG_GPG))
		sigtag = RPMSIGTAG_GPG;
	    else if (headerIsEntry(sig, RPMSIGTAG_PGP))
		sigtag = RPMSIGTAG_PGP;
	}
	if (sigtag == 0 && !nodigests) {
	    if (headerIsEntry(sig, RPMSIGTAG_MD5))
		sigtag = RPMSIGTAG_MD5;
	    else if (headerIsEntry(sig, RPMSIGTAG_SHA1))
		sigtag = RPMSIGTAG_SHA1;	/* XXX never happens */
	}

	if (headerIsEntry(sig, RPMSIGTAG_PGP)
	||  headerIsEntry(sig, RPMSIGTAG_PGP5)
	||  headerIsEntry(sig, RPMSIGTAG_MD5))
	    fdInitDigest(fd, PGPHASHALGO_MD5, 0);
	if (headerIsEntry(sig, RPMSIGTAG_GPG))
	    fdInitDigest(fd, PGPHASHALGO_SHA1, 0);

	ts->dig = pgpNewDig();

	/* Read the file, generating digest(s) on the fly. */
	if (readFile(fd, fn, ts->dig)) {
	    res++;
	    goto bottom;
	}

	res2 = 0;
	b = buf;		*b = '\0';
	m = missingKeys;	*m = '\0';
	u = untrustedKeys;	*u = '\0';
	sprintf(b, "%s:%c", fn, (rpmIsVerbose() ? '\n' : ' ') );
	b += strlen(b);

	for (hi = headerInitIterator(sig);
	    headerNextIterator(hi, &ts->sigtag, &ts->sigtype, &ts->sig, &ts->siglen);
	    ts->sig = headerFreeData(ts->sig, ts->sigtype))
	{

	    if (ts->sig == NULL) /* XXX can't happen */
		continue;

	    /* Clean up parameters from previous sigtag. */
	    pgpCleanDig(ts->dig);

	    switch (ts->sigtag) {
	    case RPMSIGTAG_RSA:
	    case RPMSIGTAG_DSA:
	    case RPMSIGTAG_GPG:
	    case RPMSIGTAG_PGP5:	/* XXX legacy */
	    case RPMSIGTAG_PGP:
		if (nosignatures)
		     continue;
		xx = pgpPrtPkts(ts->sig, ts->siglen, ts->dig,
			(_print_pkts & rpmIsDebug()));

		/* XXX only V3 signatures for now. */
		if (ts->dig->signature.version != 3) {
		    rpmError(RPMERR_SIGVFY,
		_("only V3 signatures can be verified, skipping V%u signature"),
			ts->dig->signature.version);
		    continue;
		}
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_SHA1:
		if (nodigests)
		     continue;
		/* XXX Don't bother with header sha1 if header dsa. */
		if (!nosignatures && sigtag == RPMSIGTAG_DSA)
		    continue;
		/*@switchbreak@*/ break;
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
		/*@switchbreak@*/ break;
	    default:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    }

	    res3 = rpmVerifySignature(ts, result);
	    if (res3) {
		if (rpmIsVerbose()) {
		    b = stpcpy(b, "    ");
		    b = stpcpy(b, result);
		    res2 = 1;
		} else {
		    char *tempKey;
		    switch (ts->sigtag) {
		    case RPMSIGTAG_SIZE:
			b = stpcpy(b, "SIZE ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_SHA1:
			b = stpcpy(b, "SHA1 ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_LEMD5_2:
		    case RPMSIGTAG_LEMD5_1:
		    case RPMSIGTAG_MD5:
			b = stpcpy(b, "MD5 ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_RSA:
			b = stpcpy(b, "RSA ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_PGP5:	/* XXX legacy */
		    case RPMSIGTAG_PGP:
			switch (res3) {
			case RPMSIG_NOKEY:
			    res2 = 1;
			    /*@fallthrough@*/
			case RPMSIG_NOTTRUSTED:
			{   int offset = 6;
			    b = stpcpy(b, "(MD5) (PGP) ");
			    tempKey = strstr(result, "ey ID");
			    if (tempKey == NULL) {
			        tempKey = strstr(result, "keyid:");
				offset = 9;
			    }
			    if (tempKey) {
			      if (res3 == RPMSIG_NOKEY) {
				m = stpcpy(m, " PGP#");
				m = stpncpy(m, tempKey + offset, 8);
			      } else {
			        u = stpcpy(u, " PGP#");
				u = stpncpy(u, tempKey + offset, 8);
			      }
			    }
			}   /*@innerbreak@*/ break;
			default:
			    b = stpcpy(b, "MD5 PGP ");
			    res2 = 1;
			    /*@innerbreak@*/ break;
			}
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_DSA:
			b = stpcpy(b, "(SHA1) DSA ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_GPG:
			/* Do not consider this a failure */
			switch (res3) {
			case RPMSIG_NOKEY:
			    b = stpcpy(b, "(GPG) ");
			    m = stpcpy(m, " GPG#");
			    tempKey = strstr(result, "ey ID");
			    if (tempKey)
				m = stpncpy(m, tempKey+6, 8);
			    res2 = 1;
			    /*@innerbreak@*/ break;
			default:
			    b = stpcpy(b, "GPG ");
			    res2 = 1;
			    /*@innerbreak@*/ break;
			}
			/*@switchbreak@*/ break;
		    default:
			b = stpcpy(b, "?UnknownSignatureType? ");
			res2 = 1;
			/*@switchbreak@*/ break;
		    }
		}
	    } else {
		if (rpmIsVerbose()) {
		    b = stpcpy(b, "    ");
		    b = stpcpy(b, result);
		} else {
		    switch (ts->sigtag) {
		    case RPMSIGTAG_SIZE:
			b = stpcpy(b, "size ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_SHA1:
			b = stpcpy(b, "sha1 ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_LEMD5_2:
		    case RPMSIGTAG_LEMD5_1:
		    case RPMSIGTAG_MD5:
			b = stpcpy(b, "md5 ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_RSA:
			b = stpcpy(b, "rsa ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_PGP5:	/* XXX legacy */
		    case RPMSIGTAG_PGP:
			b = stpcpy(b, "(md5) pgp ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_DSA:
			b = stpcpy(b, "(sha1) dsa ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_GPG:
			b = stpcpy(b, "gpg ");
			/*@switchbreak@*/ break;
		    default:
			b = stpcpy(b, "??? ");
			/*@switchbreak@*/ break;
		    }
		}
	    }
	}
	hi = headerFreeIterator(hi);

	res += res2;

	if (res2) {
	    if (rpmIsVerbose()) {
		rpmError(RPMERR_SIGVFY, "%s", buf);
	    } else {
		rpmError(RPMERR_SIGVFY, "%s%s%s%s%s%s%s%s\n", buf,
			_("NOT OK"),
			(missingKeys[0] != '\0') ? _(" (MISSING KEYS:") : "",
			missingKeys,
			(missingKeys[0] != '\0') ? _(") ") : "",
			(untrustedKeys[0] != '\0') ? _(" (UNTRUSTED KEYS:") : "",
			untrustedKeys,
			(untrustedKeys[0] != '\0') ? _(")") : "");

	    }
	} else {
	    if (rpmIsVerbose()) {
		rpmError(RPMERR_SIGVFY, "%s", buf);
	    } else {
		rpmError(RPMERR_SIGVFY, "%s%s%s%s%s%s%s%s\n", buf,
			_("OK"),
			(missingKeys[0] != '\0') ? _(" (MISSING KEYS:") : "",
			missingKeys,
			(missingKeys[0] != '\0') ? _(") ") : "",
			(untrustedKeys[0] != '\0') ? _(" (UNTRUSTED KEYS:") : "",
			untrustedKeys,
			(untrustedKeys[0] != '\0') ? _(")") : "");
	    }
	}

    bottom:
	ts->dig = pgpFreeDig(ts->dig);
    }

    return res;
}

int rpmcliSign(rpmTransactionSet ts, QVA_t qva, const char ** argv)
{
    const char * arg;
    int dbmode = (qva->qva_mode != RPMSIGN_IMPORT_PUBKEY)
		? O_RDONLY : (O_RDWR | O_CREAT);
    int res = 0;
    int xx;

    if (argv == NULL) return res;

    xx = rpmtsOpenDB(ts, dbmode);
    if (xx != 0)
	return -1;

    switch (qva->qva_mode) {
    case RPMSIGN_CHK_SIGNATURE:
	break;
    case RPMSIGN_IMPORT_PUBKEY:
	return rpmImportPubkey(ts, qva, argv);
	/*@notreached@*/ break;
    case RPMSIGN_NEW_SIGNATURE:
    case RPMSIGN_ADD_SIGNATURE:
	return rpmReSign(ts, qva, argv);
	/*@notreached@*/ break;
    case RPMSIGN_NONE:
    default:
	return -1;
	/*@notreached@*/ break;
    }

    while ((arg = *argv++) != NULL) {
	FD_t fd;

	if ((fd = Fopen(arg, "r.ufdio")) == NULL
	 || Ferror(fd)
	 || rpmVerifySignatures(qva, ts, fd, arg))
	    res++;

	if (fd != NULL) xx = Fclose(fd);
    }

    return res;
}
