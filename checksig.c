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
	if (readSignature(fd, &sig, lead.signature_type)) {
	    fprintf(stderr, "%s: readSignature failed\n", rpm);
	    exit(1);
	}
	if (add != ADD_SIGNATURE) {
	    freeSignature(sig);
	}

	/* Write the rest to a temp file */
	strcpy(sigtarget, tmpnam(NULL));
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
	sigtype = sigLookupType();
	message(MESS_VERBOSE, "Generating signature: %d\n", sigtype);
	if (add != ADD_SIGNATURE) {
	    sig = newSignature();
	    addSignature(sig, sigtarget, SIGTAG_SIZE, passPhrase);
	    addSignature(sig, sigtarget, SIGTAG_MD5, passPhrase);
	}
	if (sigtype>0) {
	    addSignature(sig, sigtarget, sigtype, passPhrase);
	}
	if (writeSignature(ofd, sig)) {
	    close(ofd);
	    unlink(sigtarget);
	    unlink(tmprpm);
	    freeSignature(sig);
	    exit(1);
	}
	freeSignature(sig);

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

int doCheckSig(int pgp, char **argv)
{
    int fd, ofd, res, res2;
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
	if (readSignature(fd, &sig, lead.signature_type)) {
	    fprintf(stderr, "%s: readSignature failed\n", rpm);
	    res++;
	    continue;
	}
	if (! sig) {
	    fprintf(stderr, "%s: No signature available\n", rpm);
	    res++;
	    continue;
	}
	/* Write the rest to a temp file */
	strcpy(sigtarget, tmpnam(NULL));
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

	sigIter = initIterator(sig);
	res2 = 0;
	if (isVerbose()) {
	    sprintf(buffer, "%s:\n", rpm);
	} else {
	    sprintf(buffer, "%s: ", rpm);
	}
	while (nextIterator(sigIter, &tag, &type, &ptr, &count)) {
	    if ((tag == SIGTAG_PGP) && !pgp) {
		continue;
	    }
	    if (verifySignature(sigtarget, tag, ptr, count, result)) {
		if (isVerbose()) {
		    strcat(buffer, result);
		} else {
		    switch (tag) {
		      case SIGTAG_SIZE:
			strcat(buffer, "SIZE ");
			break;
		      case SIGTAG_MD5:
			strcat(buffer, "MD5 ");
			break;
		      case SIGTAG_PGP:
			strcat(buffer, "PGP ");
			break;
		      default:
			strcat(buffer, "!!! ");
		    }
		}
		res2 = 1;
	    } else {
		if (isVerbose()) {
		    strcat(buffer, result);
		} else {
		    switch (tag) {
		      case SIGTAG_SIZE:
			strcat(buffer, "size ");
			break;
		      case SIGTAG_MD5:
			strcat(buffer, "md5 ");
			break;
		      case SIGTAG_PGP:
			strcat(buffer, "pgp ");
			break;
		      default:
			strcat(buffer, "??? ");
		    }
		}
	    }
	}
	freeIterator(sigIter);
	res += res2;

	if (res2) {
	    if (isVerbose()) {
		fprintf(stderr, "%s", buffer);
	    } else {
		fprintf(stderr, "%sNOT OK", buffer);
	    }
	} else {
	    if (isVerbose()) {
		printf("%s", buffer);
	    } else {
		printf("%sOK\n", buffer);
	    }
	}
    }

    return res;
}
