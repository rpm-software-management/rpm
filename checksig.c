/* checksig.c: verify the signature of an RPM */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "checksig.h"
#include "rpmlib.h"
#include "rpmlead.h"
#include "signature.h"
#include "messages.h"

int doReSign(int add, char *passPhrase, char **argv)
{
    int fd, ofd, count;
    struct rpmlead lead;
    unsigned short sigtype;
    char *rpm, sigtarget[1024];
    char tmprpm[1024];
    unsigned char buffer[8192];
    Header sig;
    
    while (*argv) {
	rpm = *argv++;
	printf("%s:\n", rpm);
	if ((fd = open(rpm, O_RDONLY, 0644)) < 0) {
	    fprintf(stderr, "%s: Open failed\n", rpm);
	    exit(1);
	}
	if (readLead(fd, &lead)) {
	    fprintf(stderr, "%s: readLead failed\n", rpm);
	    exit(1);
	}
	if (lead.major == 1) {
	    fprintf(stderr, "%s: Can't sign v1.0 RPM\n", rpm);
	    exit(1);
	}
	if (lead.major == 2) {
	    fprintf(stderr, "%s: Can't re-sign v2.0 RPM\n", rpm);
	    exit(1);
	}
	if (rpmReadSignature(fd, &sig, lead.signature_type)) {
	    fprintf(stderr, "%s: rpmReadSignature failed\n", rpm);
	    exit(1);
	}
	if (add != ADD_SIGNATURE) {
	    rpmFreeSignature(sig);
	}

	/* Write the rest to a temp file */
	strcpy(sigtarget, tempnam(rpmGetVar(RPMVAR_TMPPATH), "rpmsigtarget"));
	ofd = open(sigtarget, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
	    if (count == -1) {
		perror("Couldn't read the header/archive");
		close(ofd);
		unlink(sigtarget);
		exit(1);
	    }
	    if (write(ofd, buffer, count) < 0) {
		perror("Couldn't write header/archive to temp file");
		close(ofd);
		unlink(sigtarget);
		exit(1);
	    }
	}
	close(fd);
	close(ofd);

	/* Start writing the new RPM */
	sprintf(tmprpm, "%s.tmp", rpm);
	ofd = open(tmprpm, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	lead.signature_type = RPMSIG_HEADERSIG;
	if (writeLead(ofd, &lead)) {
	    perror("writeLead()");
	    close(ofd);
	    unlink(sigtarget);
	    unlink(tmprpm);
	    exit(1);
	}

	/* Generate the signature */
	sigtype = rpmLookupSignatureType();
	rpmMessage(RPMMESS_VERBOSE, "Generating signature: %d\n", sigtype);
	if (add != ADD_SIGNATURE) {
	    sig = rpmNewSignature();
	    rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
	    rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
	}
	if (sigtype>0) {
	    rpmAddSignature(sig, sigtarget, sigtype, passPhrase);
	}
	if (rpmWriteSignature(ofd, sig)) {
	    close(ofd);
	    unlink(sigtarget);
	    unlink(tmprpm);
	    rpmFreeSignature(sig);
	    exit(1);
	}
	rpmFreeSignature(sig);

	/* Append the header and archive */
	fd = open(sigtarget, O_RDONLY);
	while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
	    if (count == -1) {
		perror("Couldn't read sigtarget");
		close(ofd);
		close(fd);
		unlink(sigtarget);
		unlink(tmprpm);
		exit(1);
	    }
	    if (write(ofd, buffer, count) < 0) {
		perror("Couldn't write package");
		close(ofd);
		close(fd);
		unlink(sigtarget);
		unlink(tmprpm);
		exit(1);
	    }
	}
	close(fd);
	close(ofd);
	unlink(sigtarget);

	/* Move it in to place */
	unlink(rpm);
	rename(tmprpm, rpm);
    }

    return 0;
}

int doCheckSig(int flags, char **argv)
{
    int fd, ofd, res, res2, res3, missingKeys;
    struct rpmlead lead;
    char *rpm;
    char result[1024], sigtarget[1024];
    unsigned char buffer[8192];
    Header sig;
    HeaderIterator sigIter;
    int_32 tag, type, count;
    void *ptr;

    res = 0;
    while (*argv) {
	rpm = *argv++;
	if ((fd = open(rpm, O_RDONLY, 0644)) < 0) {
	    fprintf(stderr, "%s: Open failed\n", rpm);
	    res++;
	    continue;
	}
	if (readLead(fd, &lead)) {
	    fprintf(stderr, "%s: readLead failed\n", rpm);
	    res++;
	    continue;
	}
	if (lead.major == 1) {
	    fprintf(stderr, "%s: No signature available (v1.0 RPM)\n", rpm);
	    res++;
	    continue;
	}
	if (rpmReadSignature(fd, &sig, lead.signature_type)) {
	    fprintf(stderr, "%s: rpmReadSignature failed\n", rpm);
	    res++;
	    continue;
	}
	if (! sig) {
	    fprintf(stderr, "%s: No signature available\n", rpm);
	    res++;
	    continue;
	}
	/* Write the rest to a temp file */
	strcpy(sigtarget, tempnam(rpmGetVar(RPMVAR_TMPPATH), "rpmsigtarget"));
	ofd = open(sigtarget, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
	    if (count == -1) {
		perror("Couldn't read the header/archive");
		close(ofd);
		unlink(sigtarget);
		exit(1);
	    }
	    if (write(ofd, buffer, count) < 0) {
		fprintf(stderr, "Unable to write %s", sigtarget);
		perror("");
		close(ofd);
		unlink(sigtarget);
		exit(1);
	    }
	}
	close(fd);
	close(ofd);

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
		      tag == RPMSIGTAG_LITTLEENDIANMD5) 
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
		      case RPMSIGTAG_LITTLEENDIANMD5:
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
			strcat(buffer, "!!! ");
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
		      case RPMSIGTAG_LITTLEENDIANMD5:
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

	if (res2) {
	    if (rpmIsVerbose()) {
		fprintf(stderr, "%s", buffer);
	    } else {
		fprintf(stderr, "%sNOT OK%s\n", buffer,
			missingKeys ? " (MISSING KEYS)" : "");
	    }
	} else {
	    if (rpmIsVerbose()) {
		printf("%s", buffer);
	    } else {
		printf("%sOK%s\n", buffer,
		       missingKeys ? " (MISSING KEYS)" : "");
	    }
	}
    }

    return res;
}
