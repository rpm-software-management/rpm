/* checksig.c: verify the signature of an RPM */

#include "system.h"

#include <rpmlib.h>

#include "rpmlead.h"
#include "signature.h"
#include "misc.h"	/* XXX for makeTempFile() */

static int manageFile(FD_t *fdp, const char **fnp, int flags, int rc)
{
    const char *fn;
    FD_t fd;

    if (fdp == NULL) {	/* programmer error */
	return 1;
    }

    /* close and reset *fdp to NULL */
    if (*fdp && (fnp == NULL || *fnp == NULL)) {
	Fclose(*fdp);
	*fdp = NULL;
	return 0;
    }

    /* open a file and set *fdp */
    if (*fdp == NULL && fnp && *fnp) {
	fd = Fopen(*fnp, ((flags & O_RDONLY) ? "r.ufdio" : "w.ufdio"));
	if (fd == NULL || Ferror(fd)) {
	    fprintf(stderr, _("%s: open failed: %s\n"), *fnp,
		Fstrerror(fd));
	    return 1;
	}
	*fdp = fd;
	return 0;
    }

    /* open a temp file */
    if (*fdp == NULL && (fnp == NULL || *fnp == NULL)) {
	if (makeTempFile(NULL, (fnp ? &fn : NULL), &fd)) {
	    fprintf(stderr, _("makeTempFile failed\n"));
	    return 1;
	}
	if (fnp)
		*fnp = fn;
	*fdp = fd;
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
	FD_t *tfdp, const char **tfnp)
{
    unsigned char buffer[BUFSIZ];
    ssize_t count;
    int rc = 1;

    if (manageFile(sfdp, sfnp, O_RDONLY, 0))
	goto exit;
    if (manageFile(tfdp, tfnp, O_WRONLY|O_CREAT|O_TRUNC, 0))
	goto exit;

    while ((count = Fread(buffer, sizeof(buffer[0]), sizeof(buffer), *sfdp)) > 0) {
	if (Fwrite(buffer, sizeof(buffer[0]), count, *tfdp) < 0) {
	    fprintf(stderr, _("%s: Fwrite failed: %s\n"), *tfnp,
		Fstrerror(*tfdp));
	    goto exit;
	}
    }
    if (count < 0) {
	fprintf(stderr, _("%s: Fread failed: %s\n"), *sfnp, Fstrerror(*sfdp));
	goto exit;
    }

    rc = 0;

exit:
    if (*sfdp)	manageFile(sfdp, NULL, 0, rc);
    if (*tfdp)	manageFile(tfdp, NULL, 0, rc);
    return rc;
}

int rpmReSign(int add, char *passPhrase, const char **argv)
{
    FD_t fd = NULL;
    FD_t ofd = NULL;
    struct rpmlead lead;
    unsigned short sigtype;
    const char *rpm, *trpm;
    const char *sigtarget = NULL;
    char tmprpm[1024+1];
    Header sig = NULL;
    int rc = EXIT_FAILURE;
    
    tmprpm[0] = '\0';
    while ((rpm = *argv++) != NULL) {

	fprintf(stdout, "%s:\n", rpm);

	if (manageFile(&fd, &rpm, O_RDONLY, 0))
	    goto exit;

	if (readLead(fd, &lead)) {
	    fprintf(stderr, _("%s: readLead failed\n"), rpm);
	    goto exit;
	}
	switch (lead.major) {
	case 1:
	    fprintf(stderr, _("%s: Can't sign v1.0 RPM\n"), rpm);
	    goto exit;
	    /*@notreached@*/ break;
	case 2:
	    fprintf(stderr, _("%s: Can't re-sign v2.0 RPM\n"), rpm);
	    goto exit;
	    /*@notreached@*/ break;
	default:
	    break;
	}

	if (rpmReadSignature(fd, &sig, lead.signature_type)) {
	    fprintf(stderr, _("%s: rpmReadSignature failed\n"), rpm);
	    goto exit;
	}
	if (sig == NULL) {
	    fprintf(stderr, _("%s: No signature available\n"), rpm);
	    goto exit;
	}

	/* Write the header and archive to a temp file */
	/* ASSERT: ofd == NULL && sigtarget == NULL */
	if (copyFile(&fd, &rpm, &ofd, &sigtarget))
	    goto exit;
	/* Both fd and ofd are now closed. sigtarget contains tempfile name. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Generate the new signatures */
	if (add != ADD_SIGNATURE) {
	    rpmFreeSignature(sig);
	    sig = rpmNewSignature();
	    rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
	    rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
	}

	if ((sigtype = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0)
	    rpmAddSignature(sig, sigtarget, sigtype, passPhrase);

	/* Write the lead/signature of the output rpm */
	strcpy(tmprpm, rpm);
	strcat(tmprpm, ".XXXXXX");
	/*@-unrecog@*/ mktemp(tmprpm) /*@=unrecog@*/;
	trpm = tmprpm;

	if (manageFile(&ofd, &trpm, O_WRONLY|O_CREAT|O_TRUNC, 0))
	    goto exit;

	lead.signature_type = RPMSIG_HEADERSIG;
	if (writeLead(ofd, &lead)) {
	    fprintf(stderr, _("%s: writeLead failed: %s\n"), trpm,
		Fstrerror(ofd));
	    goto exit;
	}

	if (rpmWriteSignature(ofd, sig)) {
	    fprintf(stderr, _("%s: rpmWriteSignature failed: %s\n"), trpm,
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
	unlink(sigtarget);
	xfree(sigtarget);	sigtarget = NULL;

	/* Move final target into place. */
	unlink(rpm);
	rename(trpm, rpm);	tmprpm[0] = '\0';
    }

    rc = 0;

exit:
    if (fd)	manageFile(&fd, NULL, 0, rc);
    if (ofd)	manageFile(&ofd, NULL, 0, rc);

    if (sig) {
	rpmFreeSignature(sig);
	sig = NULL;
    }
    if (sigtarget) {
	unlink(sigtarget);
	xfree(sigtarget);
	sigtarget = NULL;
    }
    if (tmprpm[0] != '\0') {
	unlink(tmprpm);
	tmprpm[0] = '\0';
    }

    return rc;
}

int rpmCheckSig(int flags, const char **argv)
{
    FD_t fd = NULL;
    FD_t ofd = NULL;
    int res2, res3;
    struct rpmlead lead;
    const char *rpm = NULL;
    char result[1024];
    const char * sigtarget = NULL;
    unsigned char buffer[8192];
    unsigned char missingKeys[7164];
    unsigned char untrustedKeys[7164];
    Header sig;
    HeaderIterator sigIter;
    int_32 tag, type, count;
    void *ptr;
    int res = 0;

    while ((rpm = *argv++) != NULL) {

	if (manageFile(&fd, &rpm, O_RDONLY, 0)) {
	    res++;
	    goto bottom;
	}

	if (readLead(fd, &lead)) {
	    fprintf(stderr, _("%s: readLead failed\n"), rpm);
	    res++;
	    goto bottom;
	}
	switch (lead.major) {
	case 1:
	    fprintf(stderr, _("%s: No signature available (v1.0 RPM)\n"), rpm);
	    res++;
	    goto bottom;
	    /*@notreached@*/ break;
	default:
	    break;
	}
	if (rpmReadSignature(fd, &sig, lead.signature_type)) {
	    fprintf(stderr, _("%s: rpmReadSignature failed\n"), rpm);
	    res++;
	    goto bottom;
	}
	if (sig == NULL) {
	    fprintf(stderr, _("%s: No signature available\n"), rpm);
	    res++;
	    goto bottom;
	}
	/* Write the header and archive to a temp file */
	/* ASSERT: ofd == NULL && sigtarget == NULL */
	if (copyFile(&fd, &rpm, &ofd, &sigtarget)) {
	    res++;
	    goto bottom;
	}
	/* Both fd and ofd are now closed. sigtarget contains tempfile name. */
	/* ASSERT: fd == NULL && ofd == NULL */

	res2 = 0;
	missingKeys[0] = '\0';
	untrustedKeys[0] = '\0';
	sprintf(buffer, "%s:%c", rpm, (rpmIsVerbose() ? '\n' : ' ') );

	for (sigIter = headerInitIterator(sig);
	    headerNextIterator(sigIter, &tag, &type, &ptr, &count);
	    ptr = ((type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
		? xfree(ptr), NULL : NULL))
	{
	    switch (tag) {
	    case RPMSIGTAG_PGP5:	/* XXX legacy */
	    case RPMSIGTAG_PGP:
		if (!(flags & CHECKSIG_PGP)) 
		     continue;
		break;
	    case RPMSIGTAG_GPG:
		if (!(flags & CHECKSIG_GPG)) 
		     continue;
		break;
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

	    if ((res3 = rpmVerifySignature(sigtarget, tag, ptr, count, 
					   result))) {
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
	headerFreeIterator(sigIter);
	res += res2;
	unlink(sigtarget);
	xfree(sigtarget);	sigtarget = NULL;

	if (res2) {
	    if (rpmIsVerbose()) {
		fprintf(stderr, "%s", (char *)buffer);
	    } else {
		fprintf(stderr, "%s%s%s%s%s%s%s%s\n", (char *)buffer,
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
		fprintf(stdout, "%s", (char *)buffer);
	    } else {
		fprintf(stdout, "%s%s%s%s%s%s%s%s\n", (char *)buffer,
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
	if (fd)		manageFile(&fd, NULL, 0, 0);
	if (ofd)	manageFile(&ofd, NULL, 0, 0);
	if (sigtarget) {
	    unlink(sigtarget);
	    xfree(sigtarget);	sigtarget = NULL;
	}
    }

    return res;
}
