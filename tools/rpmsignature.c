/* rpmsignature: spit out the signature portion of a package */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "rpmlead.h"
#include "signature.h"

int main(int argc, char **argv)
{
    int fd, length;
    struct rpmlead lead;
    unsigned char *sig;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    readLead(fd, &lead);
    readSignature(fd, lead.signature_type, (void **) &sig);
    switch (lead.signature_type) {
    case RPMSIG_NONE:
	fprintf(stderr, "No signature available.\n");
	break;
    case RPMSIG_PGP262_1024:
	write(1, sig, 152);
	break;
    case RPMSIG_MD5:
	write(1, sig, 16);
	break;
    case RPMSIG_MD5_PGP:
	length = sig[16] * 256 + sig[17];
	write(1, sig, 18 + length);
	break;
    default:
	fprintf(stderr, "Unknown signature type: %d\n", lead.signature_type);
	exit(1);
    }
    
    return 0;
}
