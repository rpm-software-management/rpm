/* rpmsignature: spit out the signature portion of a package */

#include "system.h"

#include "rpmlead.h"
#include "signature.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    struct rpmlead lead;
    Header sig;
    
    if (argc == 1) {
	fdi = fdDup(STDIN_FILENO);
    } else {
	fdi = fdOpen(argv[1], O_RDONLY, 0644);
	if (fdFileno(fdi) < 0) {
	    perror(argv[1]);
	    exit(1);
	}
    }

    readLead(fdi, &lead);
    rpmReadSignature(fdi, &sig, lead.signature_type);
    switch (lead.signature_type) {
      case RPMSIG_NONE:
	fprintf(stderr, _("No signature available.\n"));
	break;
      default:
	fdo = fdDup(STDOUT_FILENO);
	rpmWriteSignature(fdo, sig);
    }
    
    return 0;
}
