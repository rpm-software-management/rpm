/* rpmlead: spit out the lead portion of a package */

#include "system.h"

#include "rpmio.h"
#include "rpmlead.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    struct rpmlead lead;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1) {
	fdi = Fopen("-", "r.ufdio");
    } else {
	fdi = Fopen(argv[1], "r.ufdio");
    }
    if (fdi == NULL || Ferror(fdi)) {
	perror(argv[1]);
	exit(EXIT_FAILURE);
    }

    readLead(fdi, &lead);
    fdo = Fopen("-", "w.ufdio");
    writeLead(fdo, &lead);
    
    return 0;
}
