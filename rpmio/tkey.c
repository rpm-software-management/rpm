/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

static int _debug = 0;

#include "system.h"
#include "rpmio.h"
#include "rpmpgp.h"
#include "base64.h"
#include "debug.h"

#include <stdio.h>

static int doit(const char *sig)
{
    const char *s, *t;
    unsigned char * dec;
    unsigned char * d;
    size_t declen;
    char * enc;
    int rc;
    int len = 0;
    int i;

if (_debug)
fprintf(stderr, "*** sig is\n%s\n", sig);

    if ((rc = b64decode(sig, (void **)&dec, &declen)) != 0) {
	fprintf(stderr, "*** B64decode returns %d\n", rc);
	exit(rc);
    }

    for (d = dec; d < (dec + declen); d += len) {
	len = pgpPrtPkt(d);
	if (len <= 0)
	    exit(len);
    }

    if ((enc = b64encode(dec, declen)) == NULL) {
	fprintf(stderr, "*** B64encode returns %d\n", rc);
	exit(4);
    }

if (_debug)
fprintf(stderr, "*** enc is\n%s\n", enc);

rc = 0;
for (i = 0, s = sig, t = enc; *s & *t; i++, s++, t++) {
    if (*s == '\n') s++;
    if (*t == '\n') t++;
    if (*s == *t) continue;
fprintf(stderr, "??? %5d %02x != %02x '%c' != '%c'\n", i, (*s & 0xff), (*t & 0xff), *s, *t);
    rc = 5;
}

    return rc;
}

int
main (int argc, char *argv[])
{
    int rc;

fprintf(stderr, "============================================== RPM-GPG-KEY\n");
    if ((rc = doit(redhatPubKeyDSA)) != 0)
	return rc;

fprintf(stderr, "============================================== RPM-PGP-KEY\n");
    if ((rc = doit(redhatPubKeyRSA)) != 0)
	return rc;

    return rc;
}
