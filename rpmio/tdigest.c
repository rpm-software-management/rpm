#include "system.h"
#include "rpmio_internal.h"
#include "popt.h"
#include "debug.h"

static rpmDigestFlags flags = RPMDIGEST_MD5;

#define _POPT_SET_BIT   (POPT_ARG_VAL|POPT_ARGFLAG_OR)

static struct poptOption optionsTable[] = {
 { "md5", '\0', _POPT_SET_BIT, 	&flags, RPMDIGEST_MD5,	NULL, NULL },
 { "sha1",'\0', _POPT_SET_BIT, 	&flags, RPMDIGEST_SHA1,	NULL, NULL },
 { "native",'\0', _POPT_SET_BIT, &flags, RPMDIGEST_NATIVE,	NULL, NULL },
 { NULL, '\0', 0, NULL, 0,	NULL, NULL }
};

int
main(int argc, const char *argv[])
{
    poptContext optCon;
    const char ** args;
    const char *fn;
    int rc;

    optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    while ((rc = poptGetNextOpt(optCon)) > 0)
	;

    args = poptGetArgs(optCon);
    rc = 0;
    while ((fn = *args++) != NULL) {
	FD_t fd = Fopen(fn, "r.ufdio");
	unsigned char buf[BUFSIZ];
	ssize_t nb;
	DIGEST_CTX ctx;
	const char * digest;
	size_t digestlen;

	if (fd == NULL || Ferror(fd)) {
	fprintf(stderr, _("cannot open %s: %s\n"), fn, Fstrerror(fd));
	    if (fd) Fclose(fd);
	    rc++;
	    continue;
	}

	ctx = rpmDigestInit(flags);

	while ((nb = Fread(buf, 1, sizeof(buf), fd)) > 0)
	    rpmDigestUpdate(ctx, buf, nb);
	Fclose(fd);

	rpmDigestFinal(ctx, (void **)&digest, &digestlen, 1);

	if (digest) {
	    fprintf(stdout, "%s  %s\n", digest, fn);
	    fflush(stdout);
	    free((void *)digest);
	}
    }
    return rc;
}
