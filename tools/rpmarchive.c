/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include "header.h"
#include "rpmlead.h"
#include "signature.h"

#include "debug.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    char buffer[1024];
    struct rpmlead lead;
    Header hd;
    int ct;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1) {
	fdi = Fopen("-", "r.ufdio");
    } else {
	fdi = Fopen(argv[1], "r.ufdio");
    }
    if (Ferror(fdi)) {
	fprintf(stderr, "%s: %s: %s\n", argv[0],
		(argc == 1 ? "<stdin>" : argv[1]), Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }

    readLead(fdi, &lead);
    rpmReadSignature(fdi, NULL, lead.signature_type);
    hd = headerRead(fdi, (lead.major >= 3) ?
		    HEADER_MAGIC_YES : HEADER_MAGIC_NO);

    fdo = Fopen("-", "w.ufdio");
    while ((ct = Fread(buffer, sizeof(buffer), 1, fdi))) {
	Fwrite(buffer, ct, 1, fdo);
    }
    
    return 0;
}
