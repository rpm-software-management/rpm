/* checksig.c: verify the signature of an RPM */

#include "system.h"

#ifdef	DYING
#include "build/rpmbuild.h"
#endif
#include <rpmlib.h>

#include "rpmlead.h"
#include "signature.h"

int rpmReSign(int add, char *passPhrase, const char **argv)
{
    FD_t fd, ofd;
    int count;
    struct rpmlead lead;
    unsigned short sigtype;
    const char *rpm;
    const char *sigtarget;
    char tmprpm[1024];
    unsigned char buffer[8192];
    Header sig;
    
    while (*argv) {
	rpm = *argv++;
	fprintf(stdout, "%s:\n", rpm);
	if (fdFileno(fd = fdOpen(rpm, O_RDONLY, 0644)) < 0) {
	    fprintf(stderr, _("%s: Open failed\n"), rpm);
	    exit(EXIT_FAILURE);
	}
	if (readLead(fd, &lead)) {
	    fprintf(stderr, _("%s: readLead failed\n"), rpm);
	    exit(EXIT_FAILURE);
	}
	if (lead.major == 1) {
	    fprintf(stderr, _("%s: Can't sign v1.0 RPM\n"), rpm);
	    exit(EXIT_FAILURE);
	}
	if (lead.major == 2) {
	    fprintf(stderr, _("%s: Can't re-sign v2.0 RPM\n"), rpm);
	    exit(EXIT_FAILURE);
	}
	if (rpmReadSignature(fd, &sig, lead.signature_type)) {
	    fprintf(stderr, _("%s: rpmReadSignature failed\n"), rpm);
	    exit(EXIT_FAILURE);
	}
	if (add != ADD_SIGNATURE) {
	    rpmFreeSignature(sig);
	}

	/* Write the rest to a temp file */
	if (makeTempFile(NULL, &sigtarget, &ofd))
	    exit(EXIT_FAILURE);

	while ((count = fdRead(fd, buffer, sizeof(buffer))) > 0) {
	    if (count == -1) {
		perror(_("Couldn't read the header/archive"));
		fdClose(ofd);
		unlink(sigtarget);
		xfree(sigtarget);
		exit(EXIT_FAILURE);
	    }
	    if (fdWrite(ofd, buffer, count) < 0) {
		perror(_("Couldn't write header/archive to temp file"));
		fdClose(ofd);
		unlink(sigtarget);
		xfree(sigtarget);
		exit(EXIT_FAILURE);
	    }
	}
	fdClose(fd);
	fdClose(ofd);

	/* Start writing the new RPM */
	strcpy(tmprpm, rpm);
	strcat(tmprpm, ".XXXXXX");
	mktemp(tmprpm);

	ofd = fdOpen(tmprpm, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	lead.signature_type = RPMSIG_HEADERSIG;
	if (writeLead(ofd, &lead)) {
	    perror("writeLead()");
	    fdClose(ofd);
	    unlink(sigtarget);
	    unlink(tmprpm);
	    xfree(sigtarget);
	    exit(EXIT_FAILURE);
	}

	/* Generate the signature */
	sigtype = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY);
	rpmMessage(RPMMESS_VERBOSE, _("Generating signature: %d\n"), sigtype);
	if (add != ADD_SIGNATURE) {
	    sig = rpmNewSignature();
	    rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
	    rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
	}
	if (sigtype>0) {
	    rpmAddSignature(sig, sigtarget, sigtype, passPhrase);
	}
	if (rpmWriteSignature(ofd, sig)) {
	    fdClose(ofd);
	    unlink(sigtarget);
	    unlink(tmprpm);
	    xfree(sigtarget);
	    rpmFreeSignature(sig);
	    exit(EXIT_FAILURE);
	}
	rpmFreeSignature(sig);

	/* Append the header and archive */
	fd = fdOpen(sigtarget, O_RDONLY, 0);
	while ((count = fdRead(fd, buffer, sizeof(buffer))) > 0) {
	    if (count == -1) {
		perror(_("Couldn't read sigtarget"));
		fdClose(ofd);
		fdClose(fd);
		unlink(sigtarget);
		unlink(tmprpm);
		xfree(sigtarget);
		exit(EXIT_FAILURE);
	    }
	    if (fdWrite(ofd, buffer, count) < 0) {
		perror(_("Couldn't write package"));
		fdClose(ofd);
		fdClose(fd);
		unlink(sigtarget);
		unlink(tmprpm);
		xfree(sigtarget);
		exit(EXIT_FAILURE);
	    }
	}
	fdClose(fd);
	fdClose(ofd);
	unlink(sigtarget);
	xfree(sigtarget);

	/* Move it in to place */
	unlink(rpm);
	rename(tmprpm, rpm);
    }

    return 0;
}

int rpmCheckSig(int flags, const char **argv)
{
    FD_t fd, ofd;
    int res, res2, res3;
    struct rpmlead lead;
    const char *rpm;
    char result[1024];
    const char * sigtarget;
    unsigned char buffer[8192];
    unsigned char missingKeys[7164];
    unsigned char untrustedKeys[7164];
    Header sig;
    HeaderIterator sigIter;
    int_32 tag, type, count;
    void *ptr;

    res = 0;
    while (*argv) {
	rpm = *argv++;
	if (fdFileno(fd = fdOpen(rpm, O_RDONLY, 0644)) < 0) {
	    fprintf(stderr, _("%s: Open failed\n"), rpm);
	    res++;
	    continue;
	}
	if (readLead(fd, &lead)) {
	    fprintf(stderr, _("%s: readLead failed\n"), rpm);
	    res++;
	    continue;
	}
	if (lead.major == 1) {
	    fprintf(stderr, _("%s: No signature available (v1.0 RPM)\n"), rpm);
	    res++;
	    continue;
	}
	if (rpmReadSignature(fd, &sig, lead.signature_type)) {
	    fprintf(stderr, _("%s: rpmReadSignature failed\n"), rpm);
	    res++;
	    continue;
	}
	if (sig == NULL) {
	    fprintf(stderr, _("%s: No signature available\n"), rpm);
	    res++;
	    continue;
	}
	/* Write the rest to a temp file */
	if (makeTempFile(NULL, &sigtarget, &ofd))
	    exit(EXIT_FAILURE);
	while ((count = fdRead(fd, buffer, sizeof(buffer))) > 0) {
	    if (count == -1) {
		perror(_("Couldn't read the header/archive"));
		fdClose(ofd);
		unlink(sigtarget);
		xfree(sigtarget);
		exit(EXIT_FAILURE);
	    }
	    if (fdWrite(ofd, buffer, count) < 0) {
		fprintf(stderr, _("Unable to write %s"), sigtarget);
		perror("");
		fdClose(ofd);
		unlink(sigtarget);
		xfree(sigtarget);
		exit(EXIT_FAILURE);
	    }
	}
	fdClose(fd);
	fdClose(ofd);

	res2 = 0;
	missingKeys[0] = '\0';
	untrustedKeys[0] = '\0';
	sprintf(buffer, "%s:%c", rpm, (rpmIsVerbose() ? '\n' : ' ') );

	sigIter = headerInitIterator(sig);
	while (headerNextIterator(sigIter, &tag, &type, &ptr, &count)) {
	    if ((tag == RPMSIGTAG_PGP || tag == RPMSIGTAG_PGP5)
	    	    && !(flags & CHECKSIG_PGP)) 
		continue;
	    if ((tag == RPMSIGTAG_GPG) && !(flags & CHECKSIG_GPG))
		continue;
	    if ((tag == RPMSIGTAG_MD5 || 
		      tag == RPMSIGTAG_LEMD5_2 ||
		      tag == RPMSIGTAG_LEMD5_1) 
		      && !(flags & CHECKSIG_MD5)) 
		continue;

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
		      case RPMSIGTAG_MD5:
		      case RPMSIGTAG_LEMD5_1:
		      case RPMSIGTAG_LEMD5_2:
			strcat(buffer, "MD5 ");
			res2 = 1;
			break;
		      case RPMSIGTAG_PGP:
		      case RPMSIGTAG_PGP5:
			if (res3 == RPMSIG_NOKEY || res3 == RPMSIG_NOTTRUSTED) {
			    /* Do not consider these a failure */
			    int offset = 7;
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
			} else {
			    strcat(buffer, "PGP ");
			    res2 = 1;
			}
			break;
		      case RPMSIGTAG_GPG:
			if (res3 == RPMSIG_NOKEY) {
			    /* Do not consider this a failure */
			    strcat(buffer, "(GPG) ");
			    strcat(missingKeys, " GPG#");
			    tempKey = strstr(result, "key ID");
			    if (tempKey)
				strncat(missingKeys, tempKey+7, 8);
			} else {
			    strcat(buffer, "GPG ");
			    res2 = 1;
			}
			break;
		      default:
			strcat(buffer, "?UnknownSignatureType? ");
			res2 = 1;
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
		      case RPMSIGTAG_MD5:
		      case RPMSIGTAG_LEMD5_1:
		      case RPMSIGTAG_LEMD5_2:
			strcat(buffer, "md5 ");
			break;
		      case RPMSIGTAG_PGP:
		      case RPMSIGTAG_PGP5:
			strcat(buffer, "pgp ");
			break;
		      case RPMSIGTAG_GPG:
			strcat(buffer, "gpg ");
			break;
		      default:
			strcat(buffer, "??? ");
		    }
		}
	    }
	}
	headerFreeIterator(sigIter);
	res += res2;
	unlink(sigtarget);
	xfree(sigtarget);

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
    }

    return res;
}
