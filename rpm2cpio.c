/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include "rpmlib.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    Header hd;
    int rc, isSource;
    char buffer[1024];
    int ct;
    FD_t gzdi;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1) {
	fdi = fdDup(STDIN_FILENO);
    } else {
	fdi = Fopen(argv[1], "r.ufdio");
    }

    if (Fileno(fdi) < 0) {
	perror("cannot open package");
	exit(EXIT_FAILURE);
    }
    fdo = fdDup(STDOUT_FILENO);

    rc = rpmReadPackageHeader(fdi, &hd, &isSource, NULL, NULL);
    switch (rc) {
    case 0:
	break;
    case 1:
	fprintf(stderr, _("argument is not an RPM package\n"));
	exit(EXIT_FAILURE);
	break;
    default:
	fprintf(stderr, _("error reading header from package\n"));
	exit(EXIT_FAILURE);
	break;
    }

#if 0
    gzdi = gzdFdopen(fdi, "r");	/* XXX gzdi == fdi */
#else
    gzdi = Fdopen(fdi, "r.gzdio");	/* XXX gzdi == fdi */
#endif

    while ((ct = Fread(buffer, sizeof(buffer), 1, gzdi)) > 0) {
	Fwrite(buffer, ct, 1, fdo);
    }

    if (ct < 0) {
        fprintf (stderr, "rpm2cpio: zlib: %s\n", Fstrerror(gzdi));
	rc = EXIT_FAILURE;
    } else {
	rc = EXIT_SUCCESS;
    }

    Fclose(gzdi);	/* XXX gzdi == fdi */

    return rc;
}
