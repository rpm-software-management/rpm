/*
 * md5sum.c	- Generate/check MD5 Message Digests
 *
 * Compile and link with md5.c.  If you don't have getopt() in your library
 * also include getopt.c.  For MSDOS you can also link with the wildcard
 * initialization function (wildargs.obj for Turbo C and setargv.obj for MSC)
 * so that you can use wildcards on the commandline.
 *
 * Written March 1993 by Branko Lankester
 * Modified June 1993 by Colin Plumb for altered md5.c.
 * Modified October 1995 by Erik Troan for RPM
 */
#include "system.h"

#include "md5.h"

static int domd5(const char * fn, unsigned char * digest, int asAscii,
		 int brokenEndian) {
    unsigned char buf[1024];
    unsigned char bindigest[16];
    FILE * fp;
    MD5_CTX ctx;
    int n;

    fp = fopen(fn, "r");
    if (!fp) {
	return 1;
    }

    rpmMD5Init(&ctx, brokenEndian);
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
	    rpmMD5Update(&ctx, buf, n);
    rpmMD5Final(bindigest, &ctx);
    if (ferror(fp)) {
	fclose(fp);
	return 1;
    }

    if (!asAscii) {
	memcpy(digest, bindigest, 16);
    } else {
	sprintf(digest, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x",
		bindigest[0],  bindigest[1],  bindigest[2],  bindigest[3],
		bindigest[4],  bindigest[5],  bindigest[6],  bindigest[7],
		bindigest[8],  bindigest[9],  bindigest[10], bindigest[11],
		bindigest[12], bindigest[13], bindigest[14], bindigest[15]);

    }
    fclose(fp);

    return 0;
}

int mdbinfile(const char *fn, unsigned char *bindigest) {
    return domd5(fn, bindigest, 0, 0);
}

int mdbinfileBroken(const char *fn, unsigned char *bindigest) {
    return domd5(fn, bindigest, 0, 1);
}

int mdfile(const char *fn, unsigned char *digest) {
    return domd5(fn, digest, 1, 0);
}

int mdfileBroken(const char *fn, unsigned char *digest) {
    return domd5(fn, digest, 1, 1);
}
