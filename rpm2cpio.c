/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include "rpmlib.h"

char *zlib_err [] = {
   "No",
   "Unix",
   "Data",
   "Memory",
   "Buffer",
   "Version"
};

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    Header hd;
    int rc, isSource;
    char buffer[1024];
    int ct;
    gzFile stream;
    
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
    if (rc == 1) {
	fprintf(stderr, _("argument is not an RPM package\n"));
	exit(EXIT_FAILURE);
    } else if (rc) {
	fprintf(stderr, _("error reading header from package\n"));
	exit(EXIT_FAILURE);
    }

    stream = gzdopen(fdFileno(fdi), "r");

    while ((ct = gzread(stream, &buffer, 1024)) > 0) {
	fdWrite(fdo, &buffer, ct);
    }
    if (ct < 0){
        int zerror;
       
        gzerror (stream, &zerror);
        if (zerror == Z_ERRNO){
	    perror ("While uncompressing");
	    gzclose(stream);
	    return 1;
	}
        fprintf (stderr, "rpm2cpio: zlib: %s error\n", zlib_err [-zerror - 1]);
    }

    gzclose(stream);

    return 0;
}
