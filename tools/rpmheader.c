/* rpmheader: spit out the header portion of a package */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

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
    readSignature(fd, NULL, lead.signature_type);
    hd = readHeader(fd, (lead.major >= 3) ?
		    HEADER_MAGIC : NO_HEADER_MAGIC);
    writeHeader(1, hd, HEADER_MAGIC);
    
    return 0;
}
