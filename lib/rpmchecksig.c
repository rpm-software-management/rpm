/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmcli.h>
#include "rpmpgp.h"

#include "rpmlead.h"
#include "signature.h"
#include "misc.h"	/* XXX for makeTempFile() */
#include "debug.h"

/*@access Header @*/		/* XXX compared with NULL */
/*@access FD_t @*/		/* XXX compared with NULL */
/*@access DIGEST_CTX @*/	/* XXX compared with NULL */
/*@access rpmDigest @*/

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
    unsigned char buffer[BUFSIZ];
    ssize_t count;
    int rc = 1;

    if (manageFile(sfdp, sfnp, O_RDONLY, 0))
	goto exit;
    if (manageFile(tfdp, tfnp, O_WRONLY|O_CREAT|O_TRUNC, 0))
	goto exit;

    while ((count = Fread(buffer, sizeof(buffer[0]), sizeof(buffer), *sfdp)) > 0)
    {
	if (Fwrite(buffer, sizeof(buffer[0]), count, *tfdp) != count) {
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

int rpmReSign(rpmResignFlags flags, char * passPhrase, const char ** argv)
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

	/* Generate the new signatures */
	if (flags != RESIGN_ADD_SIGNATURE) {
	    sig = rpmFreeSignature(sig);
	    sig = rpmNewSignature();
	    (void) rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
	    (void) rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
	}

	if ((sigtype = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0)
	    (void) rpmAddSignature(sig, sigtarget, sigtype, passPhrase);

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
static /*@only@*/ /*@null@*/
rpmDigest freeDig(/*@only@*/ /*@null@*/ rpmDigest dig)
	/*@modifies dig @*/
{
    if (dig != NULL) {
	dig->signature.v3 = _free(dig->signature.v3);
	dig->pubkey.v3 = _free(dig->pubkey.v3);
	/*@-branchstate@*/
	if (dig->md5ctx != NULL)
	    (void) rpmDigestFinal(dig->md5ctx, NULL, NULL, 0);
	/*@=branchstate@*/
	dig->md5ctx = NULL;
	dig->md5 = _free(dig->md5);
	/*@-branchstate@*/
	if (dig->sha1ctx != NULL)
	    (void) rpmDigestFinal(dig->sha1ctx, NULL, NULL, 0);
	/*@=branchstate@*/
	dig->sha1ctx = NULL;
	dig->sha1 = _free(dig->sha1);
	dig->hash_data = _free(dig->hash_data);

	mp32nfree(&dig->hm);
	mp32nfree(&dig->r);
	mp32nfree(&dig->s);

	(void) rsapkFree(&dig->rsa_pk);
	mp32nfree(&dig->m);
	mp32nfree(&dig->c);
	mp32nfree(&dig->rsahm);
	dig = _free(dig);
    }
    return dig;
}

/**
 */
static /*@only@*/ rpmDigest newDig(void)
	/*@*/
{
    rpmDigest dig = xcalloc(1, sizeof(*dig));
    return dig;
}

/**
 */
static int readFile(FD_t *sfdp, const char **sfnp, rpmDigest dig)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *sfdp, *sfnp, *dig, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    byte buffer[4*BUFSIZ];
    ssize_t count;
    int rc = 1;
    int i, xx;

    if (manageFile(sfdp, sfnp, O_RDONLY, 0))
	goto exit;

    /*@-type@*/ /* FIX: cast? */
    fdInitDigest(*sfdp, PGPHASHALGO_MD5, 0);
    fdInitDigest(*sfdp, PGPHASHALGO_SHA1, 0);
    /*@=type@*/
    dig->nbytes = 0;

    while ((count = Fread(buffer, sizeof(buffer[0]), sizeof(buffer), *sfdp)) > 0)
    {
#ifdef	DYING
	/*@-type@*/ /* FIX: cast? */
	xx = rpmDigestUpdate(dig->sha1ctx, buffer, count);
	/*@=type@*/
#endif
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
    if (*sfdp)	xx = manageFile(sfdp, NULL, 0, rc);
    return rc;
}

int rpmCheckSig(rpmCheckSigFlags flags, const char ** argv)
{
    FD_t fd = NULL;
    int res2, res3;
    struct rpmlead lead, *l = &lead;
    const char *pkgfn = NULL;
    char result[1024];
    char buffer[8192], * b;
    char missingKeys[7164], * m;
    char untrustedKeys[7164], * u;
    Header sig;
    HeaderIterator hi;
    int_32 tag, type, count;
    const void * ptr;
    int res = 0;
    int xx;
#ifdef DYING
    const char * gpgpk = NULL;
    unsigned int gpgpklen = 0;
    const char * pgppk = NULL;
    unsigned int pgppklen = 0;
#endif
    rpmDigest dig = NULL;
    rpmRC rc;

    if (argv == NULL) return res;

    /*@-branchstate@*/
    while ((pkgfn = *argv++) != NULL) {

	if (manageFile(&fd, &pkgfn, O_RDONLY, 0)) {
	    res++;
	    goto bottom;
	}

	memset(l, 0, sizeof(*l));
	if (readLead(fd, l)) {
	    rpmError(RPMERR_READLEAD, _("%s: readLead failed\n"), pkgfn);
	    res++;
	    goto bottom;
	}
	switch (l->major) {
	case 1:
	    rpmError(RPMERR_BADSIGTYPE, _("%s: No signature available (v1.0 RPM)\n"), pkgfn);
	    res++;
	    goto bottom;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}

	rc = rpmReadSignature(fd, &sig, l->signature_type);
	if (!(rc == RPMRC_OK || rc == RPMRC_BADSIZE)) {
	    rpmError(RPMERR_SIGGEN, _("%s: rpmReadSignature failed\n"), pkgfn);
	    res++;
	    goto bottom;
	}
	if (sig == NULL) {
	    rpmError(RPMERR_SIGGEN, _("%s: No signature available\n"), pkgfn);
	    res++;
	    goto bottom;
	}

	dig = newDig();

	/* Read the file, generating digests. */
	if (readFile(&fd, &pkgfn, dig)) {
	    res++;
	    goto bottom;
	}

	res2 = 0;
	b = buffer;		*b = '\0';
	m = missingKeys;	*m = '\0';
	u = untrustedKeys;	*u = '\0';
	sprintf(b, "%s:%c", pkgfn, (rpmIsVerbose() ? '\n' : ' ') );
	b += strlen(b);

	for (hi = headerInitIterator(sig);
	    headerNextIterator(hi, &tag, &type, &ptr, &count);
	    ptr = headerFreeData(ptr, type))
	{
	    if (ptr == NULL) /* XXX can't happen */
		/*@innercontinue@*/ continue;
	    switch (tag) {
	    case RPMSIGTAG_PGP5:	/* XXX legacy */
	    case RPMSIGTAG_PGP:
		if (!(flags & CHECKSIG_PGP)) 
		     /*@innercontinue@*/ continue;
if (rpmIsDebug())
fprintf(stderr, "========================= Package RSA Signature\n");
		xx = pgpPrtPkts(ptr, count, dig, rpmIsDebug());
    /*@-type@*/ /* FIX: cast? */
	    /*@-nullpass@*/ /* FIX: dig->md5ctx may be null */
	    {	DIGEST_CTX ctx = rpmDigestDup(dig->md5ctx);
		pgpPktSigV3 dsig = dig->signature.v3;

		xx = rpmDigestUpdate(ctx, &dsig->sigtype, dsig->hashlen);
		xx = rpmDigestFinal(ctx, (void **)&dig->md5, &dig->md5len, 1);

		/* XXX compare leading 16 bits of digest for quick check. */
	    }
	    /*@=nullpass@*/
    /*@=type@*/
		/* XXX sanity check on tag and signature agreement. */
#ifdef	DYING
		/* XXX retrieve by keyid from signature. */
		if (pgppk == NULL) {
		    xx = b64decode(redhatPubKeyRSA, (void **)&pgppk, &pgppklen);
if (rpmIsDebug())
fprintf(stderr, "========================= Red Hat RSA Public Key\n");
		    xx = pgpPrtPkts(pgppk, pgppklen, NULL, rpmIsDebug());
		}

		xx = pgpPrtPkts(pgppk, pgppklen, dig, 0);
#endif

	    {	const char * prefix = "3020300c06082a864886f70d020505000410";
		unsigned int nbits = 1024;
		unsigned int nb = (nbits + 7) >> 3;
		const char * hexstr;
		char * t;

		hexstr = t = xmalloc(2 * nb + 1);
		memset(t, 'f', (2 * nb));
		t[0] = '0'; t[1] = '0';
		t[2] = '0'; t[3] = '1';
		t += (2 * nb) - strlen(prefix) - strlen(dig->md5) - 2;
		*t++ = '0'; *t++ = '0';
		t = stpcpy(t, prefix);
		t = stpcpy(t, dig->md5);
		
		mp32nzero(&dig->rsahm);	mp32nsethex(&dig->rsahm, hexstr);

		hexstr = _free(hexstr);
	    }
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_GPG:
		if (!(flags & CHECKSIG_GPG)) 
		     /*@innercontinue@*/ continue;
if (rpmIsDebug())
fprintf(stderr, "========================= Package DSA Signature\n");
		xx = pgpPrtPkts(ptr, count, dig, rpmIsDebug());
    /*@-type@*/ /* FIX: cast? */
	    /*@-nullpass@*/ /* FIX: dig->sha1ctx may be null */
	    {	DIGEST_CTX ctx = rpmDigestDup(dig->sha1ctx);
		pgpPktSigV3 dsig = dig->signature.v3;
		xx = rpmDigestUpdate(ctx, &dsig->sigtype, dsig->hashlen);
		xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 1);
		mp32nzero(&dig->hm);	mp32nsethex(&dig->hm, dig->sha1);
	    }
	    /*@=nullpass@*/
    /*@=type@*/
		/* XXX sanity check on tag and signature agreement. */
#ifdef	DYING
		/* XXX retrieve by keyid from signature. */
		if (gpgpk == NULL) {
		    xx = b64decode(redhatPubKeyDSA, (void **)&gpgpk, &gpgpklen);
if (rpmIsDebug())
fprintf(stderr, "========================= Red Hat DSA Public Key\n");
		    xx = pgpPrtPkts(gpgpk, gpgpklen, NULL, rpmIsDebug());
		}
		xx = pgpPrtPkts(gpgpk, gpgpklen, dig, 0);
#endif
		/*@switchbreak@*/ break;
	    case RPMSIGTAG_LEMD5_2:
	    case RPMSIGTAG_LEMD5_1:
	    case RPMSIGTAG_MD5:
		if (!(flags & CHECKSIG_MD5)) 
		     /*@innercontinue@*/ continue;
		/*@switchbreak@*/ break;
	    default:
		/*@innercontinue@*/ continue;
		/*@notreached@*/ /*@switchbreak@*/ break;
	    }
	    if (ptr == NULL) /* XXX can't happen */
		/*@innercontinue@*/ continue;

	    res3 = rpmVerifySignature(pkgfn, tag, ptr, count, dig, result);
	    if (res3) {
		if (rpmIsVerbose()) {
		    b = stpcpy(b, "    ");
		    b = stpcpy(b, result);
		    res2 = 1;
		} else {
		    char *tempKey;
		    switch (tag) {
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
			{   int offset = 7;
			    b = stpcpy(b, "(PGP) ");
			    tempKey = strstr(result, "Key ID");
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
			    tempKey = strstr(result, "key ID");
			    if (tempKey)
				m = stpncpy(m, tempKey+7, 8);
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
		    switch (tag) {
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
		rpmError(RPMERR_SIGVFY, "%s", buffer);
	    } else {
		rpmError(RPMERR_SIGVFY, "%s%s%s%s%s%s%s%s\n", buffer,
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
		rpmError(RPMERR_SIGVFY, "%s", buffer);
	    } else {
		rpmError(RPMERR_SIGVFY, "%s%s%s%s%s%s%s%s\n", buffer,
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
	if (fd)		xx = manageFile(&fd, NULL, 0, 0);
	dig = freeDig(dig);
    }
    /*@=branchstate@*/

    dig = freeDig(dig);

    return res;
}
