/* rpmarchive: spit out the main archive portion of a package */

#include "system.h"

#include <rpmlib.h>
#include <rpmpgp.h>

#include "depends.c"

#include "debug.h"

int main(int argc, char **argv)
{
    FD_t fdi, fdo;
    Header h;
    char * rpmio_flags;
    rpmRC rc;
    FD_t gzdi;
    
    setprogname(argv[0]);	/* Retrofit glibc __progname */
    if (argc == 1)
	fdi = fdDup(STDIN_FILENO);
    else
	fdi = Fopen(argv[1], "r.ufdio");

    if (Ferror(fdi)) {
	fprintf(stderr, "%s: %s: %s\n", argv[0],
		(argc == 1 ? "<stdin>" : argv[1]), Fstrerror(fdi));
	exit(EXIT_FAILURE);
    }
    fdo = fdDup(STDOUT_FILENO);

#ifdef	DYING
    rc = rpmReadPackageHeader(fdi, &h, NULL, NULL, NULL);
#else
    {	const char * rootDir = "";
	rpmdb db = NULL;
	rpmTransactionSet ts = rpmtransCreateSet(db, rootDir);

	ts->need_payload = 1;
	/*@-mustmod@*/      /* LCL: segfault */
	rc = rpmReadPackageFile(ts, fdi, "rpm2cpio", &h);
	/*@=mustmod@*/
	ts->need_payload = 0;

	ts = rpmtransFree(ts);
    }
#endif

    switch (rc) {
    case RPMRC_BADSIZE:
    case RPMRC_OK:
	break;
    case RPMRC_BADMAGIC:
	fprintf(stderr, _("argument is not an RPM package\n"));
	exit(EXIT_FAILURE);
	break;
    case RPMRC_FAIL:
    case RPMRC_SHORTREAD:
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

    rc = ufdCopy(gzdi, fdo);
    rc = (rc <= 0) ? EXIT_FAILURE : EXIT_SUCCESS;
    Fclose(fdo);

    Fclose(gzdi);	/* XXX gzdi == fdi */

    return rc;
}
