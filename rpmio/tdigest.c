#include "system.h"
#include "rpmio_internal.h"
#include "popt.h"
#include "debug.h"

static rpmDigestFlags flags = RPMDIGEST_MD5;
extern int _rpmio_debug;

static struct poptOption optionsTable[] = {
 { "md5", '\0', POPT_BIT_SET, 	&flags, RPMDIGEST_MD5,	NULL, NULL },
 { "sha1",'\0', POPT_BIT_SET, 	&flags, RPMDIGEST_SHA1,	NULL, NULL },
 { "native",'\0', POPT_BIT_SET, &flags, RPMDIGEST_NATIVE,	NULL, NULL },
 { "debug",'d', POPT_ARG_VAL, &_rpmio_debug, -1,	NULL, NULL },
  POPT_TABLEEND
};

int
main(int argc, const char *argv[])
{
    poptContext optCon;
    const char ** args;
    const char * ifn;
    const char * ofn = "/dev/null";
    int rc;

    optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    while ((rc = poptGetNextOpt(optCon)) > 0)
	;

    args = poptGetArgs(optCon);
    rc = 0;
    while ((ifn = *args++) != NULL) {
	FD_t ifd;
	FD_t ofd;
	unsigned char buf[BUFSIZ];
	ssize_t nb;
	DIGEST_CTX ctx;
	const char * idigest;
	const char * odigest;
	const char * digest;
	size_t digestlen;

	ifd = Fopen(ifn, "r.ufdio");
	if (ifd == NULL || Ferror(ifd)) {
	    fprintf(stderr, _("cannot open %s: %s\n"), ifn, Fstrerror(ifd));
	    if (ifd) Fclose(ifd);
	    rc++;
	    continue;
	}
	idigest = NULL;
	(flags & RPMDIGEST_SHA1) ? fdInitSHA1(ifd) : fdInitMD5(ifd, 0);

	ofd = Fopen(ofn, "w.ufdio");
	if (ofd == NULL || Ferror(ofd)) {
	    fprintf(stderr, _("cannot open %s: %s\n"), ofn, Fstrerror(ofd));
	    if (ifd) Fclose(ifd);
	    if (ofd) Fclose(ofd);
	    rc++;
	    continue;
	}
	odigest = NULL;
	(flags & RPMDIGEST_SHA1) ? fdInitSHA1(ofd) : fdInitMD5(ofd, 0);

	ctx = rpmDigestInit(flags);

	while ((nb = Fread(buf, 1, sizeof(buf), ifd)) > 0) {
	    rpmDigestUpdate(ctx, buf, nb);
	    (void) Fwrite(buf, 1, nb, ofd);
	}

	fdFiniMD5(ifd, (void **)&idigest, NULL, 1);
	Fclose(ifd);

	Fflush(ofd);
	fdFiniMD5(ofd, (void **)&odigest, NULL, 1);
	Fclose(ofd);

	rpmDigestFinal(ctx, (void **)&digest, &digestlen, 1);

	if (digest) {
	    fprintf(stdout, "%s     %s\n", digest, ifn);
	    fflush(stdout);
	    free((void *)digest);
	}
	if (idigest) {
	    fprintf(stdout, "%s in  %s\n", idigest, ifn);
	    fflush(stdout);
	    free((void *)idigest);
	}
	if (odigest) {
	    fprintf(stdout, "%s out %s\n", odigest, ofn);
	    fflush(stdout);
	    free((void *)odigest);
	}
    }
    return rc;
}
