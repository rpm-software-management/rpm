/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

static int _debug = 0;

#include "system.h"
#include "rpmio_internal.h"
#include "rpmpgp.h"
#include "debug.h"

static int doit(const char *sig, pgpDig dig, int printing)
{
    const char *s, *t;
    unsigned char * dec;
    size_t declen;
    char * enc;
    int rc;
    int i;

if (_debug)
fprintf(stderr, "*** sig is\n%s\n", sig);

    if ((rc = b64decode(sig, (void **)&dec, &declen)) != 0) {
	fprintf(stderr, "*** b64decode returns %d\n", rc);
	return rc;
    }
    rc = pgpPrtPkts(dec, declen, dig, printing);
    if (rc < 0) {
	fprintf(stderr, "*** pgpPrtPkts returns %d\n", rc);
	return rc;
    }

    if ((enc = b64encode(dec, declen)) == NULL) {
	fprintf(stderr, "*** b64encode failed\n");
	return rc;
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

/* FIPS-186 test vectors. */
static const char * fips_p = "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80291";
static const char * fips_q = "c773218c737ec8ee993b4f2ded30f48edace915f";
static const char * fips_g = "626d027839ea0a13413163a55b4cb500299d5522956cefcb3bff10f399ce2c2e71cb9de5fa24babf58e5b79521925c9cc42e9f6f464b088cc572af53e6d78802";

static const char * fips_hm = "a9993e364706816aba3e25717850c26c9cd0d89d";

static const char * fips_y = "19131871d75b1612a819f29d78d1b0d7346f7aa77bb62a859bfd6c5675da9d212d3a36ef1672ef660b8c7c255cc0ec74858fba33f44c06699630a76b030ee333";

static const char * fips_r = "8bac1ab66410435cb7181f95b16ab97c92b341c0";
static const char * fips_s = "41e2345f1f56df2458f426d155b4ba2db6dcd8c8";

/* Secret key */
static const char * jbjSecretDSA = "
lQFvBDu6XHwRAwCTIHRgKeIlOFUIEZeJVYSrXn0eUrM5S8OF471tTc+IV7AwiXBR
zCFCan4lO1ipmoAipyN2A6ZX0HWOcWdYlWz2adxA7l8JNiZTzkemA562xwex2wLy
AQWVTtRN6jv0LccAoN4UWZkIvkT6tV918sEvDEggGARxAv9190RhrDq/GMqd+AHm
qWrRkrBRHDUBBL2fYEuU3gFekYrW5CDIN6s3Mcq/yUsvwHl7bwmoqbf2qabbyfnv
Y66ETOPKLcw67ggcptHXHcwlvpfJmHKpjK+ByzgauPXXbRAC+gKDjzXL0kAQxjmT
2D+16O4vI8Emlx2JVcGLlq/aWhspvQWIzN6PytA3iKZ6uzesrM7yXmqzgodZUsJh
1wwl/0K5OIJn/oD41UayU8RXNER8SzDYvDYsJymFRwE1s58lL/8DAwJUAllw1pdZ
WmBIoAvRiv7kE6hWfeCvZzdBVgrHYrp8ceUa3OdulGfYw/0sIzpEU0FfZmFjdG9y
OgAA30gJ4JMFKVfthnDCHHL+O8lNxykKBmrgVPLClue0KUplZmYgSm9obnNvbiAo
QVJTIE4zTlBRKSA8amJqQHJlZGhhdC5jb20+iFcEExECABcFAju6XHwFCwcKAwQD
FQMCAxYCAQIXgAAKCRCB0qVW2I6DmQU6AJ490bVWZuM4yCOh8MWj6qApCr1/gwCf
f3+QgXFXAeTyPtMmReyWxThABtE=
";

/* Public key */
static const char * jbjPublicDSA = "
mQFCBDu6XHwRAwCTIHRgKeIlOFUIEZeJVYSrXn0eUrM5S8OF471tTc+IV7AwiXBR
zCFCan4lO1ipmoAipyN2A6ZX0HWOcWdYlWz2adxA7l8JNiZTzkemA562xwex2wLy
AQWVTtRN6jv0LccAoN4UWZkIvkT6tV918sEvDEggGARxAv9190RhrDq/GMqd+AHm
qWrRkrBRHDUBBL2fYEuU3gFekYrW5CDIN6s3Mcq/yUsvwHl7bwmoqbf2qabbyfnv
Y66ETOPKLcw67ggcptHXHcwlvpfJmHKpjK+ByzgauPXXbRAC+gKDjzXL0kAQxjmT
2D+16O4vI8Emlx2JVcGLlq/aWhspvQWIzN6PytA3iKZ6uzesrM7yXmqzgodZUsJh
1wwl/0K5OIJn/oD41UayU8RXNER8SzDYvDYsJymFRwE1s58lL7QpSmVmZiBKb2hu
c29uIChBUlMgTjNOUFEpIDxqYmpAcmVkaGF0LmNvbT6IVwQTEQIAFwUCO7pcfAUL
BwoDBAMVAwIDFgIBAheAAAoJEIHSpVbYjoOZBToAn3TXaAI+bhg51EeyaiFip/6W
OVwBAJ44rTtNsgZBQxXISjB64CWxl4VaWQ==
";

/* Signature */
static const char * abcSignatureDSA = "
iD8DBQA7vII+gdKlVtiOg5kRAvg4AJ0fV3gDBADobAnK2HOkV88bfmFMEgCeNysO
nP3dWWJnp0Pnbor7pIob4Dk=
";

int
main (int argc, char *argv[])
{
    pgpDig dig;
    int printing = 1;
    int rc;

    dig = pgpNewDig();

    mp32bzero(&dig->p);	mp32bsethex(&dig->p, fips_p);
    mp32bzero(&dig->q);	mp32bsethex(&dig->q, fips_q);
    mp32nzero(&dig->g);	mp32nsethex(&dig->g, fips_g);
    mp32nzero(&dig->y);	mp32nsethex(&dig->y, fips_y);
    mp32nzero(&dig->r);	mp32nsethex(&dig->r, fips_r);
    mp32nzero(&dig->s);	mp32nsethex(&dig->s, fips_s);
    mp32nzero(&dig->hm);mp32nsethex(&dig->hm, fips_hm);

    rc = dsavrfy(&dig->p, &dig->q, &dig->g, &dig->hm,
		&dig->y, &dig->r, &dig->s);

fprintf(stderr, "=============================== DSA FIPS-186-1: rc %d\n", rc);

    mp32bfree(&dig->p);
    mp32bfree(&dig->q);
    mp32nfree(&dig->g);
    mp32nfree(&dig->y);

    mp32nfree(&dig->hm);
    mp32nfree(&dig->r);
    mp32nfree(&dig->s);

fprintf(stderr, "=============================== GPG Secret Key\n");
    if ((rc = doit(jbjSecretDSA, dig, printing)) != 0)
	fprintf(stderr, "==> FAILED: rc %d\n", rc);

fprintf(stderr, "=============================== GPG Public Key\n");
    if ((rc = doit(jbjPublicDSA, dig, printing)) != 0)
	fprintf(stderr, "==> FAILED: rc %d\n", rc);

fprintf(stderr, "=============================== GPG Signature of \"abc\"\n");
    if ((rc = doit(abcSignatureDSA, dig, printing)) != 0)
	fprintf(stderr, "==> FAILED: rc %d\n", rc);

    {	DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	struct pgpDigParams_s * dsig = &dig->signature;
	const char * digest = NULL;
	size_t digestlen = 0;
	const char * txt = "abc";
	
	rpmDigestUpdate(ctx, txt, strlen(txt));
	rpmDigestUpdate(ctx, dsig->hash, dsig->hashlen);
	rpmDigestFinal(ctx, (void **)&digest, &digestlen, 1);

	mp32nzero(&dig->hm);mp32nsethex(&dig->hm, digest);

fprintf(stderr, "\n    hm = [ 160]: %s\n\n", digest);

	if (digest) {
	    free((void *)digest);
	    digest = NULL;
	}
    }

    rc = dsavrfy(&dig->p, &dig->q, &dig->g, &dig->hm,
		&dig->y, &dig->r, &dig->s);

fprintf(stderr, "=============================== DSA verify: rc %d\n", rc);

    mp32bfree(&dig->p);
    mp32bfree(&dig->q);
    mp32nfree(&dig->g);
    mp32nfree(&dig->y);

    mp32nfree(&dig->hm);
    mp32nfree(&dig->r);
    mp32nfree(&dig->s);

    dig = pgpFreeDig(dig);

    return rc;
}
