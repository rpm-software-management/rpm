/* rpmsignature: spit out the signature portion of a package */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "rpmlead.h"
#include "signature.h"

int main(int argc, char **argv)
{
    int fd;
    struct rpmlead lead;
    char *sig;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    readLead(fd, &lead);
    readSignature(fd, lead.signature_type, (void **) &sig);
    switch (lead.signature_type) {
    case RPMSIG_NONE:
	break;
    case RPMSIG_PGP262_1024:
	write(1, sig, 152);
    }
    
    return 0;
}
