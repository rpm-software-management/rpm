/* rpmchecksig: verify the signature of an RPM */

#include "system.h"

#include "rpmlib.h"

#include "rpmlead.h"
#include "signature.h"

int main(int argc, char **argv)
{
    int fd;
    struct rpmlead lead;
    char *sig;
    char result[1024];
    int res;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    /* Need this for any PGP settings */
    if (rpmReadConfigFiles(NULL, NULL, NULL, 0))
	exit(-1);

    readLead(fd, &lead);
    rpmReadSignature(fd, lead.signature_type, (void **) &sig);
    res = verifySignature(fd, lead.signature_type, sig, result, 1);
    fprintf(stdout, "%s", result);
    if (res) {
	fprintf(stdout, "Signature OK.\n");
	return 0;
    } else {
	fprintf(stdout, "Signature NOT OK!\n");
	return 1;
    }
}
