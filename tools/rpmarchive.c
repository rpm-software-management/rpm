/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include "rpmlead.h"
#include "signature.h"
#include "header.h"
#include "intl.h"

int main(int argc, char **argv)
{
    int fd;
    char buffer[1024];
    struct rpmlead lead;
    Header hd;
    int ct;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    readLead(fd, &lead);
    rpmReadSignature(fd, NULL, lead.signature_type);
    hd = headerRead(fd, (lead.major >= 3) ?
		    HEADER_MAGIC_YES : HEADER_MAGIC_NO);

    while ((ct = read(fd, &buffer, 1024))) {
	write(1, &buffer, ct);
    }
    
    return 0;
}
