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
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "md5.h"
#include "messages.h"

int mdfile(char *fn, unsigned char *digest) {
    unsigned char buf[1024];
    unsigned char bindigest[16];
    FILE * fp;
    MD5_CTX ctx;
    int n;

    fp = fopen(fn, "r");
    if (!fp) {
	return 1;
    }

    MD5Init(&ctx);
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
	    MD5Update(&ctx, buf, n);
    MD5Final(bindigest, &ctx);
    if (ferror(fp)) {
	fclose(fp);
	return 1;
    }

    sprintf(digest, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
                    "%02x%02x%02x%02x%02x",
	    bindigest[0],  bindigest[1],  bindigest[2],  bindigest[3],
	    bindigest[4],  bindigest[5],  bindigest[6],  bindigest[7],
	    bindigest[8],  bindigest[9],  bindigest[10], bindigest[11],
	    bindigest[12], bindigest[13], bindigest[14], bindigest[15]);

    fclose(fp);

    return 0;
}
