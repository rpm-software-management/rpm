/* rpmheader: spit out the header portion of a package */

#include "system.h"

#include "rpmlead.h"
#include "signature.h"
#include "header.h"

int main(int argc, char **argv)
{
    int fd;
    struct rpmlead lead;
    Header hd;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    readLead(fd, &lead);
    rpmReadSignature(fd, NULL, lead.signature_type);
    hd = headerRead(fd, (lead.major >= 3) ?
		    HEADER_MAGIC_YES : HEADER_MAGIC_NO);
    headerWrite(1, hd, HEADER_MAGIC_YES);
    
    return 0;
}
