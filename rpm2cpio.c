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
	fdi = fdOpen(argv[1], O_RDONLY, 0644);
    }

    if (fdFileno(fdi) < 0) {
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

    gzdi = gzdFdopen(fdi, "r");	/* XXX gzdi == fdi */

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
