/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include "rpmlib.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    Header h;
    char * rpmio_flags;
    int rc, isSource;
    FD_t gzdi;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1) {
	fdi = fdDup(STDIN_FILENO);
    } else {
	fdi = Fopen(argv[1], "r.ufdio");
    }

    if (fdi == NULL || Ferror(fdi)) {
	fprintf(stderr, _("cannot open package: %s\n"), Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }
    fdo = fdDup(STDOUT_FILENO);

    rc = rpmReadPackageHeader(fdi, &h, &isSource, NULL, NULL);
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

    /* Retrieve type of payload compression. */
    {	const char * payload_compressor = NULL;
	char * t;

	if (!headerGetEntry(h, RPMTAG_PAYLOADCOMPRESSOR, NULL,
			    (void **) &payload_compressor, NULL))
	    payload_compressor = "gzip";
	rpmio_flags = t = alloca(sizeof("r.gzdio"));
	*t++ = 'r';
	if (!strcmp(payload_compressor, "gzip"))
	    t = stpcpy(t, ".gzdio");
	if (!strcmp(payload_compressor, "bzip2"))
	    t = stpcpy(t, ".bzdio");
    }

    gzdi = Fdopen(fdi, rpmio_flags);	/* XXX gzdi == fdi */
    if (gzdi == NULL) {
	fprintf(stderr, _("cannot re-open payload: %s\n"), Fstrerror(gzdi));
	exit(EXIT_FAILURE);
    }

#ifdef	DYING
  { char buffer[BUFSIZ];
    int ct;
    while ((ct = Fread(buffer, sizeof(buffer[0]), sizeof(buffer), gzdi)) > 0) {
	Fwrite(buffer, sizeof(buffer[0]), ct, fdo);
    }

    if (ct < 0) {
        fprintf (stderr, "rpm2cpio: zlib: %s\n", Fstrerror(gzdi));
	rc = EXIT_FAILURE;
    } else {
	rc = EXIT_SUCCESS;
    }
  }
#else
    rc = ufdCopy(gzdi, fdo);
    rc = (rc <= 0) ? EXIT_FAILURE : EXIT_SUCCESS;
    Fclose(fdo);
#endif

    Fclose(gzdi);	/* XXX gzdi == fdi */

    return rc;
}
