/* rpmheader: spit out the header portion of a package */

#include "system.h"

#include "rpmlead.h"
#include "signature.h"
#include "header.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    struct rpmlead lead;
    Header hd;
    
    if (argc == 1) {
	fdi = fdDup(0);
    } else {
	fdi = fdOpen(argv[1], O_RDONLY, 0644);
    }

    readLead(fdi, &lead);
    rpmReadSignature(fdi, NULL, lead.signature_type);
    hd = headerRead(fdi, (lead.major >= 3) ?
		    HEADER_MAGIC_YES : HEADER_MAGIC_NO);
    fdo = fdDup(1);
    headerWrite(fdo, hd, HEADER_MAGIC_YES);
    
    return 0;
}
