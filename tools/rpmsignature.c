/* rpmsignature: spit out the signature portion of a package */

#include "system.h"

#include "rpmlead.h"
#include "signature.h"
#include "debug.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    struct rpmlead lead;
    Header sig;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1) {
	fdi = Fopen("-", "r.ufdio");
    } else {
	fdi = Fopen(argv[1], "r.ufdio");
    }
    if (Ferror(fdi)) {
	fprintf(stderr, "%s: %s: %s\n", argv[0],
		(argc == 1 ? "<stdin>" : argv[1]), Fstrerror(fdi));
	exit(1);
    }

    readLead(fdi, &lead);
    rpmReadSignature(fdi, &sig, lead.signature_type);
    switch (lead.signature_type) {
    case RPMSIGTYPE_NONE:
	fprintf(stderr, _("No signature available.\n"));
	break;
    default:
	fdo = Fopen("-", "w.ufdio");
	rpmWriteSignature(fdo, sig);
	break;
    }
    
    return 0;
}
