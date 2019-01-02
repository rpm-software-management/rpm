/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include <rpm/rpmlib.h>		/* rpmReadPackageFile .. */
#include <rpm/rpmtag.h>
#include <rpm/rpmio.h>
#include <rpm/rpmpgp.h>

#include <rpm/rpmts.h>
#include <unistd.h>

#include "debug.h"

int main(int argc, char *argv[])
{
    FD_t fdi, fdo;
    Header h;
    char * rpmio_flags = NULL;
    int rc;
    off_t payload_size;
    FD_t gzdi;
    
    xsetprogname(argv[0]); /* Portability call -- see system.h */

    rpmReadConfigFiles(NULL, NULL);
    if (argc == 1)
	fdi = fdDup(STDIN_FILENO);
    else {
	if (rstreq(argv[1], "-h") || rstreq(argv[1], "--help")) {
	    fprintf(stderr, "Usage: rpm2cpio file.rpm\n");
	    exit(EXIT_FAILURE);
	}
	fdi = Fopen(argv[1], "r.ufdio");
    }

    if (Ferror(fdi)) {
	fprintf(stderr, "%s: %s: %s\n", argv[0],
		(argc == 1 ? "<stdin>" : argv[1]), Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }
    if (isatty(STDOUT_FILENO)) {
	fprintf(stderr, "Error: refusing to output cpio data to a terminal.\n");
	exit(EXIT_FAILURE);
    }
    fdo = fdDup(STDOUT_FILENO);

    {	rpmts ts = rpmtsCreate();
	rpmVSFlags vsflags = 0;

	/* XXX retain the ageless behavior of rpm2cpio */
        vsflags |= RPMVSF_MASK_NODIGESTS;
        vsflags |= RPMVSF_MASK_NOSIGNATURES;
        vsflags |= RPMVSF_NOHDRCHK;
	(void) rpmtsSetVSFlags(ts, vsflags);

	rc = rpmReadPackageFile(ts, fdi, "rpm2cpio", &h);

	ts = rpmtsFree(ts);
    }

    switch (rc) {
    case RPMRC_OK:
    case RPMRC_NOKEY:
    case RPMRC_NOTTRUSTED:
	break;
    case RPMRC_NOTFOUND:
	fprintf(stderr, _("argument is not an RPM package\n"));
	exit(EXIT_FAILURE);
	break;
    case RPMRC_FAIL:
    default:
	fprintf(stderr, _("error reading header from package\n"));
	exit(EXIT_FAILURE);
	break;
    }

    if (headerIsEntry(h, RPMTAG_LONGFILESIZES)) {
	fprintf(stderr, _("files over 4GB not supported by cpio, use rpm2archive instead\n"));
	exit(EXIT_FAILURE);
    }

    /* Retrieve payload size and compression type. */
    {	const char *compr = headerGetString(h, RPMTAG_PAYLOADCOMPRESSOR);
	rpmio_flags = rstrscat(NULL, "r.", compr ? compr : "gzip", NULL);
	payload_size = headerGetNumber(h, RPMTAG_LONGARCHIVESIZE);
    }

    gzdi = Fdopen(fdi, rpmio_flags);	/* XXX gzdi == fdi */
    free(rpmio_flags);

    if (gzdi == NULL) {
	fprintf(stderr, _("cannot re-open payload: %s\n"), Fstrerror(gzdi));
	exit(EXIT_FAILURE);
    }

    rc = (ufdCopy(gzdi, fdo) == payload_size) ? EXIT_SUCCESS : EXIT_FAILURE;

    Fclose(fdo);

    Fclose(gzdi);	/* XXX gzdi == fdi */

    return rc;
}
