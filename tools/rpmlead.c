/* rpmlead: spit out the lead portion of a package */

#include "system.h"

#include "rpmlead.h"

int main(int argc, char **argv)
{
    int fd;
    struct rpmlead lead;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    readLead(fd, &lead);
    writeLead(1, &lead);
    
    return 0;
}
