/* checksig.c: verify the signature of an RPM */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "checksig.h"
#include "rpmlib.h"
#include "rpmlead.h"
#include "signature.h"

int doReSign(char *passPhrase, char **argv)
{
    int fd, ofd, count;
    struct rpmlead lead;
    unsigned short sigtype;
    char *sig, *rpm, sigtarget[1024];
    char tmprpm[1024];
    unsigned char buffer[8192];
    
    /* Figure out the signature type */
    if ((sigtype = sigLookupType()) == RPMSIG_BAD) {
	fprintf(stderr, "Bad signature type in rpmrc\n");
	exit(1);
    }

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
	if (!readSignature(fd, lead.signature_type, (void **) &sig)) {
	    fprintf(stderr, "%s: readSignature failed\n", rpm);
	    exit(1);
	}
	if (sig) {
	    free(sig);
	}

	/* Write the rest to a temp file */
	strcpy(sigtarget, tmpnam(NULL));
	ofd = open(sigtarget, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	while ((count = read(fd, buffer, sizeof(buffer))) > 0) {
	    if (count == -1) {
		perror("Couldn't read the header/archvie");
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
	lead.signature_type = sigtype;
	if (writeLead(ofd, &lead)) {
	    perror("writeLead()");
	    close(ofd);
	    unlink(sigtarget);
	    unlink(tmprpm);
	    exit(1);
	}

	/* Generate the signature */
	if (makeSignature(sigtarget, sigtype, ofd, passPhrase)) {
	    fprintf(stderr, "makeSignature() failed\n");
	    close(ofd);
	    unlink(sigtarget);
	    unlink(tmprpm);
	    exit(1);
	}

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
    int fd;
    struct rpmlead lead;
    char *sig, *rpm;
    char result[1024];
    int res = 0;
    int xres;
    
    while (*argv) {
	rpm = *argv++;
	if ((fd = open(rpm, O_RDONLY, 0644)) < 0) {
	    fprintf(stderr, "%s: Open failed\n", rpm);
	    res = -1;
	    continue;
	}
	if (readLead(fd, &lead)) {
	    fprintf(stderr, "%s: readLead failed\n", rpm);
	    res = -1;
	    continue;
	}
	if (lead.major == 1) {
	    fprintf(stderr, "%s: No signature available (v1.0 RPM)\n", rpm);
	    res = -1;
	    continue;
	}
	if (!readSignature(fd, lead.signature_type, (void **) &sig)) {
	    fprintf(stderr, "%s: readSignature failed\n", rpm);
	    res = -1;
	    continue;
	}
	
	xres = verifySignature(fd, lead.signature_type, sig, result, pgp);
	if (xres == RPMSIG_SIGOK) {
	    if (isVerbose()) {
		printf("%s: %s", rpm, result);
	    }
	    switch (lead.signature_type) {
	    case RPMSIG_PGP262_1024:
		printf("%s: (Old PGP) Signature OK.\n", rpm);
		break;
	    case RPMSIG_MD5:
		printf("%s: (MD5) Signature OK.\n", rpm);
		break;
	    case RPMSIG_MD5_PGP:
		if (pgp) {
		    printf("%s: (MD5 PGP) Signature OK.\n", rpm);
		} else {
		    printf("%s: (MD5 [PGP skipped]) Signature OK.\n", rpm);
		}
		break;
	    }
	} else {
	    if (isVerbose()) {
		fprintf(stderr, "%s: %s", rpm, result);
	    }
	    if (xres & RPMSIG_NOSIG) {
		fprintf(stderr, "%s: No signature available.\n", rpm);
	    } else if (xres & RPMSIG_UNKNOWNSIG) {
		fprintf(stderr, "%s: Unknown signature type.\n", rpm);
	    } else if (xres & RPMSIG_BADMD5) {
		fprintf(stderr, "%s: (MD5) Signature NOT OK!\n", rpm);
	    } else if (xres & RPMSIG_BADPGP) {
		fprintf(stderr, "%s: (PGP) Signature NOT OK!\n", rpm);
	    } else {
		fprintf(stderr, "%s: (Internal error) Signature NOT OK!\n",
			rpm);
	    }
	    res = -1;
	}
	close(fd);
    }

    return res;
}
