/* rpmlead: spit out the lead portion of a package */

#include "system.h"

#include "rpmio.h"
#include "rpmlead.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    struct rpmlead lead;
    
    if (argc == 1) {
	fdi = fdDup(0);
    } else {
	fdi = fdOpen(argv[1], O_RDONLY, 0644);
    }

    readLead(fdi, &lead);
    fdo = fdDup(1);
    writeLead(fdo, &lead);
    
    return 0;
}
