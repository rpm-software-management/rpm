#include "system.h"
#include "rpmio_internal.h"
#include "popt.h"
#include "debug.h"


static rpmDigestFlags flags = RPMDIGEST_MD5;
extern int _rpmio_debug;

static int fips = 0;

const char * FIPSAdigest = "a9993e364706816aba3e25717850c26c9cd0d89d";
const char * FIPSBdigest = "84983e441c3bd26ebaae4aa1f95129e5e54670f1";
const char * FIPSCdigest = "34aa973cd4c4daa4f61eeb2bdbad27316534016f";

static struct poptOption optionsTable[] = {
 { "md5", '\0', POPT_BIT_SET, 	&flags, RPMDIGEST_MD5,	NULL, NULL },
 { "sha1",'\0', POPT_BIT_SET, 	&flags, RPMDIGEST_SHA1,	NULL, NULL },
#ifdef	DYING
 { "reverse",'\0', POPT_BIT_SET, &flags, RPMDIGEST_REVERSE,	NULL, NULL },
#endif
 { "fipsa",'\0', POPT_BIT_SET, &fips, 1,	NULL, NULL },
 { "fipsb",'\0', POPT_BIT_SET, &fips, 2,	NULL, NULL },
 { "fipsc",'\0', POPT_BIT_SET, &fips, 3,	NULL, NULL },
 { "debug",'d', POPT_ARG_VAL, &_rpmio_debug, -1,	NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

#define	SHA1_CMD	"/usr/bin/sha1sum"
#define	MD5_CMD		"/usr/bin/md5sum"

int
main(int argc, const char *argv[])
{
    poptContext optCon;
    const char ** args;
    const char * ifn;
    const char * ofn = "/dev/null";
    DIGEST_CTX ctx;
    const char * idigest;
    const char * odigest;
    const char * sdigest;
    const char * digest;
    size_t digestlen;
    int asAscii = 1;
    int reverse = 0;
    int rc;
    char appendix;
    int i;

    optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    while ((rc = poptGetNextOpt(optCon)) > 0)
	;

    if (flags & RPMDIGEST_SHA1) flags &= ~RPMDIGEST_MD5;
#ifdef	DYING
    reverse = (flags & RPMDIGEST_REVERSE);
#endif
    if (fips) {
	flags &= ~RPMDIGEST_MD5;
	flags |= RPMDIGEST_SHA1;
	ctx = rpmDigestInit(flags);
	ifn = NULL;
	appendix = ' ';
	sdigest = NULL;
	switch (fips) {
	case 1:
	    ifn = "abc";
	    rpmDigestUpdate(ctx, ifn, strlen(ifn));
	    sdigest = FIPSAdigest;
	    appendix = 'A';
	    break;
	case 2:
	    ifn = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	    rpmDigestUpdate(ctx, ifn, strlen(ifn));
	    sdigest = FIPSBdigest;
	    appendix = 'B';
	    break;
	case 3:
	    ifn = "aaaaaaaaaaa ...";
	    for (i = 0; i < 1000000; i++)
		rpmDigestUpdate(ctx, ifn, 1);
	    sdigest = FIPSCdigest;
	    appendix = 'C';
	    break;
	}
	if (ifn == NULL)
	    return 1;
	rpmDigestFinal(ctx, (void **)&digest, &digestlen, asAscii);

	if (digest) {
	    fprintf(stdout, "%s     %s\n", digest, ifn);
	    fflush(stdout);
	    free((void *)digest);
	}
	if (sdigest) {
	    fprintf(stdout, "%s     FIPS PUB 180-1 Appendix %c\n", sdigest,
		appendix);
	    fflush(stdout);
	}
	return 0;
    }

    args = poptGetArgs(optCon);
    rc = 0;
    if (args)
    while ((ifn = *args++) != NULL) {
	FD_t ifd;
	FD_t ofd;
	unsigned char buf[BUFSIZ];
	ssize_t nb;

	sdigest = NULL;
	{   char *se;
	    FILE * sfp;

	    se = buf;
	    *se = '\0';
	    se = stpcpy(se, ((flags & RPMDIGEST_SHA1) ? SHA1_CMD : MD5_CMD));
	    *se++ = ' ';
	    se = stpcpy(se, ifn);
	    if ((sfp = popen(buf, "r")) != NULL) {
		fgets(buf, sizeof(buf), sfp);
		if ((se = strchr(buf, ' ')) != NULL)
		    *se = '\0';
		sdigest = xstrdup(buf);
		pclose(sfp);
	    }
	}

	ifd = Fopen(ifn, "r.ufdio");
	if (ifd == NULL || Ferror(ifd)) {
	    fprintf(stderr, _("cannot open %s: %s\n"), ifn, Fstrerror(ifd));
	    if (ifd) Fclose(ifd);
	    rc++;
	    continue;
	}
	idigest = NULL;
	(flags & RPMDIGEST_SHA1) ? fdInitSHA1(ifd, reverse) : fdInitMD5(ifd, reverse);

	ofd = Fopen(ofn, "w.ufdio");
	if (ofd == NULL || Ferror(ofd)) {
	    fprintf(stderr, _("cannot open %s: %s\n"), ofn, Fstrerror(ofd));
	    if (ifd) Fclose(ifd);
	    if (ofd) Fclose(ofd);
	    rc++;
	    continue;
	}
	odigest = NULL;
	(flags & RPMDIGEST_SHA1) ? fdInitSHA1(ofd, reverse) : fdInitMD5(ofd, reverse);

	ctx = rpmDigestInit(flags);

	while ((nb = Fread(buf, 1, sizeof(buf), ifd)) > 0) {
	    rpmDigestUpdate(ctx, buf, nb);
	    (void) Fwrite(buf, 1, nb, ofd);
	}

	(flags & RPMDIGEST_SHA1)
	    ? fdFiniSHA1(ifd, (void **)&idigest, NULL, asAscii)
	    : fdFiniMD5(ifd, (void **)&idigest, NULL, asAscii);
	Fclose(ifd);

	Fflush(ofd);
	(flags & RPMDIGEST_SHA1)
	    ? fdFiniSHA1(ofd, (void **)&odigest, NULL, asAscii)
	    : fdFiniMD5(ofd, (void **)&odigest, NULL, asAscii);
	Fclose(ofd);

	rpmDigestFinal(ctx, (void **)&digest, &digestlen, asAscii);

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
	if (sdigest) {
	    fprintf(stdout, "%s cmd %s\n", sdigest, ifn);
	    fflush(stdout);
	    free((void *)sdigest);
	}
    }
    return rc;
}
