/* checksig.c: verify the signature of an RPM */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

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
    
    while (*argv) {
	rpm = *argv++;
	if ((fd = open(rpm, O_RDONLY, 0644)) < 0) {
	    fprintf(stderr, "Open failed: %s\n", rpm);
	    res = -1;
	}
	if (readLead(fd, &lead)) {
	    fprintf(stderr, "readLead failed: %s\n", rpm);
	    res = -1;
	}
	if (!readSignature(fd, lead.signature_type, (void **) &sig)) {
	    fprintf(stderr, "readSignature failed: %s\n", rpm);
	    res = -1;
	}
	res = verifySignature(fd, lead.signature_type, sig, result);
	if (res) {
	    printf("%s: %s", rpm, result);
	    printf("Signature OK.\n");
	} else {
	    fprintf(stderr, "%s: %s", rpm, result);
	    fprintf(stderr, "Signature NOT OK!\n");
	    res = -1;
	}
	close(fd);
    }

    return res;
}
