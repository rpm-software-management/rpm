/* checksig.c: verify the signature of an RPM */

#include "system.h"

#include "build/rpmbuild.h"

#include "checksig.h"
#include "rpmlead.h"
#include "signature.h"

int doReSign(int add, char *passPhrase, char **argv)
{
    FD_t fd, ofd;
    int count;
    struct rpmlead lead;
    unsigned short sigtype;
    char *rpm;
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
	sprintf(tmprpm, "%s.tmp", rpm);
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
	sigtype = rpmLookupSignatureType();
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

int doCheckSig(int flags, char **argv)
{
    FD_t fd, ofd;
    int res, res2, res3, missingKeys;
    struct rpmlead lead;
    char *rpm;
    char result[1024];
    const char * sigtarget;
    unsigned char buffer[8192];
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

	sigIter = headerInitIterator(sig);
	res2 = 0;
	missingKeys = 0;
	if (rpmIsVerbose()) {
	    sprintf(buffer, "%s:\n", rpm);
	} else {
	    sprintf(buffer, "%s: ", rpm);
	}
	while (headerNextIterator(sigIter, &tag, &type, &ptr, &count)) {
	    if ((tag == RPMSIGTAG_PGP) && !(flags & CHECKSIG_PGP)) 
		continue;
	    else if ((tag == RPMSIGTAG_MD5 || 
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
			if (res3 == RPMSIG_NOKEY) {
			    /* Do not consedier this a failure */
			    strcat(buffer, "(PGP) ");
			    missingKeys = 1;
			} else {
			    strcat(buffer, "PGP ");
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
			strcat(buffer, "pgp ");
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
		fprintf(stderr, "%s%s%s\n", (char *)buffer, _("NOT OK"),
			missingKeys ? _(" (MISSING KEYS)") : "");
	    }
	} else {
	    if (rpmIsVerbose()) {
		fprintf(stdout, "%s", (char *)buffer);
	    } else {
		fprintf(stdout, "%s%s%s\n", (char *)buffer, _("OK"),
		       missingKeys ? _(" (MISSING KEYS)") : "");
	    }
	}
    }

    return res;
}
