/* checksig.c: verify the signature of an RPM */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "checksig.h"
#include "rpmlib.h"
#include "rpmlead.h"
#include "signature.h"

int doCheckSig(char **argv)
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
	if (!readSignature(fd, lead.signature_type, (void **) &sig)) {
	    fprintf(stderr, "%s: readSignature failed\n", rpm);
	    res = -1;
	    continue;
	}
	
	xres = verifySignature(fd, lead.signature_type, sig, result);
	if (!xres) {
	    if (isVerbose()) {
		printf("%s: %s", rpm, result);
	    }
	    printf("%s: Signature OK.\n", rpm);
	} else {
	    if (isVerbose()) {
		fprintf(stderr, "%s: %s", rpm, result);
	    }
	    if (xres == RPMSIG_NOSIG) {
		fprintf(stderr, "%s: No signature available.\n", rpm);
	    } else if (xres == RPMSIG_UNKNOWNSIG) {
		fprintf(stderr, "%s: Unknown signature type.\n", rpm);
	    } else {
		fprintf(stderr, "%s: Signature NOT OK!\n", rpm);
	    }
	    res = -1;
	}
	close(fd);
    }

    return res;
}
