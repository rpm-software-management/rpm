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

/*@access Header@*/		/* XXX compared with NULL */
/*@access FD_t@*/		/* XXX compared with NULL */
/*@access rpmDigest@*/

static int manageFile(FD_t *fdp, const char **fnp, int flags,
		/*@unused@*/ int rc)
	/*@modifies *fdp, *fnp, fileSystem @*/
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
	fdFree(fd, "manageFile return");
	return 0;
    }

    /* no operation */
    if (*fdp && fnp && *fnp) {
	return 0;
    }

    /* XXX never reached */
    return 1;
}

static int copyFile(FD_t *sfdp, const char **sfnp,
	FD_t *tfdp, const char **tfnp, rpmDigest dig)
	/*@modifies *sfdp, *sfnp, *tfdp, *tfnp, *dig, fileSystem @*/
{
    unsigned char buffer[BUFSIZ];
    ssize_t count;
    int rc = 1;

    if (manageFile(sfdp, sfnp, O_RDONLY, 0))
	goto exit;
    if (manageFile(tfdp, tfnp, O_WRONLY|O_CREAT|O_TRUNC, 0))
	goto exit;

    if (dig != NULL)
	(void) fdInitSHA1(*sfdp, 0);

    while ((count = Fread(buffer, sizeof(buffer[0]), sizeof(buffer), *sfdp)) > 0) {
	/* XXX calculate MD5 sum here, always (well there's --nomd5) needed. */
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

    if (dig != NULL) {
	dig->ctx = _free(dig->ctx);
	dig->ctx = (*sfdp)->digest;
	(*sfdp)->digest = NULL;
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
	    /*@notreached@*/ break;
	case 2:
	    rpmError(RPMERR_BADSIGTYPE, _("%s: Can't re-sign v2.0 RPM\n"), rpm);
	    goto exit;
	    /*@notreached@*/ break;
	default:
	    break;
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
	if (copyFile(&fd, &rpm, &ofd, &sigtarget, NULL))
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
	if (copyFile(&fd, &sigtarget, &ofd, &trpm, NULL))
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

int rpmCheckSig(rpmCheckSigFlags flags, const char ** argv)
{
    FD_t fd = NULL;
    FD_t ofd = NULL;
    int res2, res3;
    struct rpmlead lead, *l = &lead;
    const char *pkgfn = NULL;
    char result[1024];
    const char * sigtarget = NULL;
    unsigned char buffer[8192];
    unsigned char missingKeys[7164];
    unsigned char untrustedKeys[7164];
    Header sig;
    HeaderIterator hi;
    int_32 tag, type, count;
    const void * ptr;
    int res = 0;
    rpmDigest dig = alloca(sizeof(*dig));
    rpmRC rc;

    if (argv == NULL) return res;

    memset(dig, 0, sizeof(*dig));

{   static const char * pubkey = NULL;
    static unsigned int pklen = 0;

    /* XXX retrieve by keyid from signature. */
    if (pubkey == NULL) {
	(void) b64decode(redhatPubKeyDSA, (void **)&pubkey, &pklen);
if (rpmIsVerbose())
fprintf(stderr, "========================= Red Hat DSA Public Key\n");
	    (void) pgpPrtPkts(pubkey, pklen, NULL, rpmIsVerbose());
    }
    (void) pgpPrtPkts(pubkey, pklen, dig, 0);
}

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
	    /*@notreached@*/ break;
	default:
	    break;
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

	/* Write the header and archive to a temp file */
	/* ASSERT: ofd == NULL && sigtarget == NULL */
	if (copyFile(&fd, &pkgfn, &ofd, &sigtarget, dig)) {
	    res++;
	    goto bottom;
	}
	/* Both fd and ofd are now closed. sigtarget contains tempfile name. */
	/* ASSERT: fd == NULL && ofd == NULL */

	res2 = 0;
	missingKeys[0] = '\0';
	untrustedKeys[0] = '\0';
	sprintf(buffer, "%s:%c", pkgfn, (rpmIsVerbose() ? '\n' : ' ') );

	for (hi = headerInitIterator(sig);
	    headerNextIterator(hi, &tag, &type, &ptr, &count);
	    ptr = headerFreeData(ptr, type))
	{
	    if (ptr == NULL) continue;	/* XXX can't happen */
	    switch (tag) {
	    case RPMSIGTAG_PGP5:	/* XXX legacy */
	    case RPMSIGTAG_PGP:
		if (!(flags & CHECKSIG_PGP)) 
		     continue;
if (rpmIsVerbose())
fprintf(stderr, "========================= Package RSA Signature\n");
		(void) pgpPrtPkts(ptr, count, dig, rpmIsVerbose());
		break;
	    case RPMSIGTAG_GPG:
		if (!(flags & CHECKSIG_GPG)) 
		     continue;
if (rpmIsVerbose())
fprintf(stderr, "========================= Package DSA Signature\n");
		(void) pgpPrtPkts(ptr, count, dig, rpmIsVerbose());
	    {	DIGEST_CTX ctx = rpmDigestDup(dig->ctx);
		const char * digest = NULL;
		size_t digestlen = 0;
		rpmDigestUpdate(ctx, &dig->sig.v3.sigtype, dig->sig.v3.hashlen);
		rpmDigestFinal(ctx, (void **)&digest, &digestlen, 1);
		mp32nzero(&dig->hm);mp32nsethex(&dig->hm, digest);
		digest = _free(digest);
	    }	break;
	    case RPMSIGTAG_LEMD5_2:
	    case RPMSIGTAG_LEMD5_1:
	    case RPMSIGTAG_MD5:
		if (!(flags & CHECKSIG_MD5)) 
		     continue;
		break;
	    default:
		continue;
		/*@notreached@*/ break;
	    }
	    if (ptr == NULL) continue;	/* XXX can't happen */

	    if ((res3 = rpmVerifySignature(sigtarget, tag, ptr, count, 
					   dig, result))) {
		if (rpmIsVerbose()) {
		    strcat(buffer, result);
		    res2 = 1;
		} else {
		    char *tempKey;
		    switch (tag) {
		      case RPMSIGTAG_SIZE:
			strcat(buffer, "SIZE ");
			res2 = 1;
			break;
		      case RPMSIGTAG_LEMD5_2:
		      case RPMSIGTAG_LEMD5_1:
		      case RPMSIGTAG_MD5:
			strcat(buffer, "MD5 ");
			res2 = 1;
			break;
		      case RPMSIGTAG_PGP5:	/* XXX legacy */
		      case RPMSIGTAG_PGP:
			switch (res3) {
			/* Do not consider these a failure */
			case RPMSIG_NOKEY:
			case RPMSIG_NOTTRUSTED:
			{   int offset = 7;
			    strcat(buffer, "(PGP) ");
			    tempKey = strstr(result, "Key ID");
			    if (tempKey == NULL) {
			        tempKey = strstr(result, "keyid:");
				offset = 9;
			    }
			    if (tempKey) {
			      if (res3 == RPMSIG_NOKEY) {
				strcat(missingKeys, " PGP#");
				strncat(missingKeys, tempKey + offset, 8);
			      } else {
			        strcat(untrustedKeys, " PGP#");
				strncat(untrustedKeys, tempKey + offset, 8);
			      }
			    }
			}   break;
			default:
			    strcat(buffer, "PGP ");
			    res2 = 1;
			    break;
			}
			break;
		      case RPMSIGTAG_GPG:
			/* Do not consider this a failure */
			switch (res3) {
			case RPMSIG_NOKEY:
			    strcat(buffer, "(GPG) ");
			    strcat(missingKeys, " GPG#");
			    tempKey = strstr(result, "key ID");
			    if (tempKey)
				strncat(missingKeys, tempKey+7, 8);
			    break;
			default:
			    strcat(buffer, "GPG ");
			    res2 = 1;
			    break;
			}
			break;
		      default:
			strcat(buffer, "?UnknownSignatureType? ");
			res2 = 1;
			break;
		    }
		}
	    } else {
		if (rpmIsVerbose()) {
		    strcat(buffer, result);
		} else {
		    switch (tag) {
		    case RPMSIGTAG_SIZE:
			strcat(buffer, "size ");
			break;
		    case RPMSIGTAG_LEMD5_2:
		    case RPMSIGTAG_LEMD5_1:
		    case RPMSIGTAG_MD5:
			strcat(buffer, "md5 ");
			break;
		    case RPMSIGTAG_PGP5:	/* XXX legacy */
		    case RPMSIGTAG_PGP:
			strcat(buffer, "pgp ");
			break;
		    case RPMSIGTAG_GPG:
			strcat(buffer, "gpg ");
			break;
		    default:
			strcat(buffer, "??? ");
			break;
		    }
		}
	    }
	}
	hi = headerFreeIterator(hi);

	res += res2;
	(void) unlink(sigtarget);
	sigtarget = _free(sigtarget);

	if (res2) {
	    if (rpmIsVerbose()) {
		rpmError(RPMERR_SIGVFY, "%s", (char *)buffer);
	    } else {
		rpmError(RPMERR_SIGVFY, "%s%s%s%s%s%s%s%s\n", (char *)buffer,
			_("NOT OK"),
			(missingKeys[0] != '\0') ? _(" (MISSING KEYS:") : "",
			(char *)missingKeys,
			(missingKeys[0] != '\0') ? _(") ") : "",
			(untrustedKeys[0] != '\0') ? _(" (UNTRUSTED KEYS:") : "",
			(char *)untrustedKeys,
			(untrustedKeys[0] != '\0') ? _(")") : "");

	    }
	} else {
	    if (rpmIsVerbose()) {
		rpmError(RPMERR_SIGVFY, "%s", (char *)buffer);
	    } else {
		rpmError(RPMERR_SIGVFY, "%s%s%s%s%s%s%s%s\n", (char *)buffer,
			_("OK"),
			(missingKeys[0] != '\0') ? _(" (MISSING KEYS:") : "",
			(char *)missingKeys,
			(missingKeys[0] != '\0') ? _(") ") : "",
			(untrustedKeys[0] != '\0') ? _(" (UNTRUSTED KEYS:") : "",
			(char *)untrustedKeys,
			(untrustedKeys[0] != '\0') ? _(")") : "");
	    }
	}

    bottom:
	if (fd)		(void) manageFile(&fd, NULL, 0, 0);
	if (ofd)	(void) manageFile(&ofd, NULL, 0, 0);
	if (sigtarget) {
	    (void) unlink(sigtarget);
	    sigtarget = _free(sigtarget);
	}
	dig->data = _free(dig->data);
	dig->ctx = _free(dig->ctx);
	mp32nfree(&dig->hm);
	mp32nfree(&dig->r);
	mp32nfree(&dig->s);
    }

    dig->data = _free(dig->data);
    dig->ctx = _free(dig->ctx);
    mp32bfree(&dig->p);
    mp32bfree(&dig->q);
    mp32nfree(&dig->g);
    mp32nfree(&dig->y);
    mp32nfree(&dig->hm);
    mp32nfree(&dig->r);
    mp32nfree(&dig->s);
    return res;
}
