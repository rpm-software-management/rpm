/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmcli.h>
#include "rpmpgp.h"

#include "depends.h"

#include "rpmlead.h"
#include "signature.h"
#include "misc.h"	/* XXX for makeTempFile() */
#include "debug.h"

/*@access rpmTransactionSet @*/	/* ts->rpmdb, ts->id */
/*@access Header @*/		/* XXX compared with NULL */
/*@access FD_t @*/		/* XXX compared with NULL */
/*@access DIGEST_CTX @*/	/* XXX compared with NULL */
/*@access pgpDig @*/

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
	/*@-type@*/ /* FIX: cast? */
	*fdp = fdLink(fd, "manageFile return");
	fdFree(fd, "manageFile return");
	/*@=type@*/
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
    int sigtype;
    const char *rpm, *trpm;
    const char *sigtarget = NULL;
    char tmprpm[1024+1];
    Header sig = NULL;
    int res = EXIT_FAILURE;
    rpmRC rc;
    
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
	    rpmError(RPMERR_BADSIGTYPE, _("%s: Can't sign v1.0 RPM\n"), rpm);
	    goto exit;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case 2:
	    rpmError(RPMERR_BADSIGTYPE, _("%s: Can't re-sign v2.0 RPM\n"), rpm);
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

	/* Toss the current signatures and recompute if not --addsign. */
	if (qva->qva_mode != RPMSIGN_ADD_SIGNATURE) {
	    sig = rpmFreeSignature(sig);
	    sig = rpmNewSignature();
	    (void) rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, qva->passPhrase);
	    (void) rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, qva->passPhrase);
	}

	if ((sigtype = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0)
	    (void) rpmAddSignature(sig, sigtarget, sigtype, qva->passPhrase);

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
	(void) unlink(sigtarget);
	sigtarget = _free(sigtarget);

	/* Move final target into place. */
	(void) unlink(rpm);
	(void) rename(trpm, rpm);
	tmprpm[0] = '\0';
    }
    /*@=branchstate@*/

    res = 0;

exit:
    if (fd)	(void) manageFile(&fd, NULL, 0, res);
    if (ofd)	(void) manageFile(&ofd, NULL, 0, res);

    sig = rpmFreeSignature(sig);

    if (sigtarget) {
	(void) unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }
    if (tmprpm[0] != '\0') {
	(void) unlink(tmprpm);
	tmprpm[0] = '\0';
    }

    return res;
}

/**
 */
static int readFile(FD_t *sfdp, const char **sfnp, pgpDig dig)
	/*@globals fileSystem, internalState @*/
	/*@modifies *sfdp, *dig,
		fileSystem, internalState @*/
{
    byte buf[4*BUFSIZ];
    ssize_t count;
    int rc = 1;
    int i;

    /*@-type@*/ /* FIX: cast? */
    fdInitDigest(*sfdp, PGPHASHALGO_MD5, 0);
    fdInitDigest(*sfdp, PGPHASHALGO_SHA1, 0);
    /*@=type@*/
    dig->nbytes = 0;

    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), *sfdp)) > 0)
    {
	dig->nbytes += count;
    }

    if (count < 0) {
	rpmError(RPMERR_FREAD, _("%s: Fread failed: %s\n"), *sfnp, Fstrerror(*sfdp));
	goto exit;
    } else
	dig->nbytes += count;

    /*@-type@*/ /* FIX: cast? */
    for (i = (*sfdp)->ndigests - 1; i >= 0; i--) {
	FDDIGEST_t fddig = (*sfdp)->digests + i;
	if (fddig->hashctx == NULL)
	    continue;
	if (fddig->hashalgo == PGPHASHALGO_MD5) {
	    /*@-branchstate@*/
	    if (dig->md5ctx != NULL)
		(void) rpmDigestFinal(dig->md5ctx, NULL, NULL, 0);
	    /*@=branchstate@*/
	    dig->md5ctx = fddig->hashctx;
	    fddig->hashctx = NULL;
	    continue;
	}
	if (fddig->hashalgo == PGPHASHALGO_SHA1) {
	    /*@-branchstate@*/
	    if (dig->sha1ctx != NULL)
		(void) rpmDigestFinal(dig->sha1ctx, NULL, NULL, 0);
	    /*@=branchstate@*/
	    dig->sha1ctx = fddig->hashctx;
	    fddig->hashctx = NULL;
	    continue;
	}
    }
    /*@=type@*/

    rc = 0;

exit:
    return rc;
}

/** \ingroup rpmcli
 * Import public key(s).
 * @param ts		transaction set
 * @param qva		mode flags and parameters
 * @param argv		array of pubkey file names (NULL terminated)
 * @return		0 on success
 */
static int rpmImportPubkey(rpmTransactionSet ts,
		/*@unused@*/ QVA_t qva,
		/*@null@*/ const char ** argv)
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
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
	/*@-mods@*/ /* FIX: ts->rpmdb is modified */
	xx = rpmdbAdd(ts->rpmdb, ts->id, h);
	/*@=mods@*/

bottom:
	/* Clean up. */
	h = headerFree(h);
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

int rpmVerifySignatures(QVA_t qva, rpmTransactionSet ts, FD_t fd,
		const char * fn)
{
    int res2, res3;
    struct rpmlead lead, *l = &lead;
    char result[1024];
    char buf[8192], * b;
    char missingKeys[7164], * m;
    char untrustedKeys[7164], * u;
    Header sig;
    HeaderIterator hi;
    int res = 0;
    int xx;
    rpmRC rc;

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

	ts->dig = pgpNewDig();

	/* Read the file, generating digest(s) on the fly. */
	/*@-mods@*/ /* FIX: double indirection */
	if (readFile(&fd, &fn, ts->dig)) {
	    res++;
	    goto bottom;
	}
	/*@=mods@*/

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
	    switch (ts->sigtag) {
	    case RPMSIGTAG_PGP5:	/* XXX legacy */
	    case RPMSIGTAG_PGP:
		if (!(qva->qva_flags & VERIFY_SIGNATURE)) 
		     continue;
if (rpmIsDebug())
fprintf(stderr, "========================= Package RSA Signature\n");
		xx = pgpPrtPkts(ts->sig, ts->siglen, ts->dig, rpmIsDebug());

#ifdef	DYING

		/* XXX sanity check on ts->sigtag and signature agreement. */

    /*@-type@*/ /* FIX: cast? */
	    /*@-nullpass@*/ /* FIX: ts->dig->md5ctx may be null */
	    {	DIGEST_CTX ctx = rpmDigestDup(ts->dig->md5ctx);
		struct pgpDigParams_s * dsig = &ts->dig->signature;

		xx = rpmDigestUpdate(ctx, dsig->hash, dsig->hashlen);
		xx = rpmDigestFinal(ctx, (void **)&ts->dig->md5, &ts->dig->md5len, 1);

		/* XXX compare leading 16 bits of digest for quick check. */
	    }
	    /*@=nullpass@*/
    /*@=type@*/

	    {	const char * prefix = "3020300c06082a864886f70d020505000410";
		unsigned int nbits = 1024;
		unsigned int nb = (nbits + 7) >> 3;
		const char * hexstr;
		char * t;

		hexstr = t = xmalloc(2 * nb + 1);
		memset(t, 'f', (2 * nb));
		t[0] = '0'; t[1] = '0';
		t[2] = '0'; t[3] = '1';
		t += (2 * nb) - strlen(prefix) - strlen(ts->dig->md5) - 2;
		*t++ = '0'; *t++ = '0';
		t = stpcpy(t, prefix);
		t = stpcpy(t, ts->dig->md5);
		
		mp32nzero(&ts->dig->rsahm);	mp32nsethex(&ts->dig->rsahm, hexstr);

		hexstr = _free(hexstr);
	    }
#endif
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_GPG:
		if (!(qva->qva_flags & VERIFY_SIGNATURE)) 
		     continue;
if (rpmIsDebug())
fprintf(stderr, "========================= Package DSA Signature\n");
		xx = pgpPrtPkts(ts->sig, ts->siglen, ts->dig, rpmIsDebug());

#ifdef	DYING
		/* XXX sanity check on ts->sigtag and signature agreement. */

    /*@-type@*/ /* FIX: cast? */
	    /*@-nullpass@*/ /* FIX: ts->dig->sha1ctx may be null */
	    {	DIGEST_CTX ctx = rpmDigestDup(ts->dig->sha1ctx);
		struct pgpDigParams_s * dsig = &ts->dig->signature;

		xx = rpmDigestUpdate(ctx, dsig->hash, dsig->hashlen);
		xx = rpmDigestFinal(ctx, (void **)&ts->dig->sha1, &ts->dig->sha1len, 1);
		mp32nzero(&ts->dig->hm);	mp32nsethex(&ts->dig->hm, ts->dig->sha1);
	    }
	    /*@=nullpass@*/
    /*@=type@*/
#endif
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_LEMD5_2:
	    case RPMSIGTAG_LEMD5_1:
	    case RPMSIGTAG_MD5:
		if (!(qva->qva_flags & VERIFY_DIGEST)) 
		     continue;
		/*@switchbreak@*/ break;
	    default:
		continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    }
	    if (ts->sig == NULL) /* XXX can't happen */
		continue;

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
		      case RPMSIGTAG_LEMD5_2:
		      case RPMSIGTAG_LEMD5_1:
		      case RPMSIGTAG_MD5:
			b = stpcpy(b, "MD5 ");
			res2 = 1;
			/*@switchbreak@*/ break;
		      case RPMSIGTAG_PGP5:	/* XXX legacy */
		      case RPMSIGTAG_PGP:
			switch (res3) {
			/* Do not consider these a failure */
			case RPMSIG_NOKEY:
			case RPMSIG_NOTTRUSTED:
			{   int offset = 6;
			    b = stpcpy(b, "(PGP) ");
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
			    b = stpcpy(b, "PGP ");
			    res2 = 1;
			    /*@innerbreak@*/ break;
			}
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
		    case RPMSIGTAG_LEMD5_2:
		    case RPMSIGTAG_LEMD5_1:
		    case RPMSIGTAG_MD5:
			b = stpcpy(b, "md5 ");
			/*@switchbreak@*/ break;
		    case RPMSIGTAG_PGP5:	/* XXX legacy */
		    case RPMSIGTAG_PGP:
			b = stpcpy(b, "pgp ");
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

int rpmcliSign(QVA_t qva, const char ** argv)
{
    const char * rootDir = "/";
    rpmdb db = NULL;
    rpmTransactionSet ts;
    const char * arg;
    int res = 0;
    int xx;

    if (argv == NULL) return res;

    db = NULL;
    xx = rpmdbOpen(rootDir, &db,
	((qva->qva_mode == RPMSIGN_IMPORT_PUBKEY) ? O_RDWR : O_RDONLY), 0644);
    if (xx != 0)
	return -1;
    ts = rpmtransCreateSet(db, rootDir);

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

    ts->dig = pgpFreeDig(ts->dig);	/* XXX just in case */
    ts = rpmtransFree(ts);

    return res;
}
