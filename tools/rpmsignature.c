/* rpmsignature: spit out the signature portion of a package */

#include "system.h"

#include "rpmlead.h"
#include "signature.h"

int main(int argc, char **argv)
{
    int fd;
    struct rpmlead lead;
    Header sig;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    readLead(fd, &lead);
    rpmReadSignature(fd, &sig, lead.signature_type);
    switch (lead.signature_type) {
      case RPMSIG_NONE:
	fprintf(stderr, _("No signature available.\n"));
	break;
      default:
	rpmWriteSignature(1, sig);
    }
    
    return 0;
}
