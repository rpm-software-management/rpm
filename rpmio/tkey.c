/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

static int _debug = 0;

#include "system.h"
#include "rpmio/digest.h"
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

    if ((enc = b64encode(dec, declen, -1)) == NULL) {
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

/* Secret key */
static const char * jbjSecretDSA =
"lQFvBDu6XHwRAwCTIHRgKeIlOFUIEZeJVYSrXn0eUrM5S8OF471tTc+IV7AwiXBR"
"zCFCan4lO1ipmoAipyN2A6ZX0HWOcWdYlWz2adxA7l8JNiZTzkemA562xwex2wLy"
"AQWVTtRN6jv0LccAoN4UWZkIvkT6tV918sEvDEggGARxAv9190RhrDq/GMqd+AHm"
"qWrRkrBRHDUBBL2fYEuU3gFekYrW5CDIN6s3Mcq/yUsvwHl7bwmoqbf2qabbyfnv"
"Y66ETOPKLcw67ggcptHXHcwlvpfJmHKpjK+ByzgauPXXbRAC+gKDjzXL0kAQxjmT"
"2D+16O4vI8Emlx2JVcGLlq/aWhspvQWIzN6PytA3iKZ6uzesrM7yXmqzgodZUsJh"
"1wwl/0K5OIJn/oD41UayU8RXNER8SzDYvDYsJymFRwE1s58lL/8DAwJUAllw1pdZ"
"WmBIoAvRiv7kE6hWfeCvZzdBVgrHYrp8ceUa3OdulGfYw/0sIzpEU0FfZmFjdG9y"
"OgAA30gJ4JMFKVfthnDCHHL+O8lNxykKBmrgVPLClue0KUplZmYgSm9obnNvbiAo"
"QVJTIE4zTlBRKSA8amJqQHJlZGhhdC5jb20+iFcEExECABcFAju6XHwFCwcKAwQD"
"FQMCAxYCAQIXgAAKCRCB0qVW2I6DmQU6AJ490bVWZuM4yCOh8MWj6qApCr1/gwCf"
"f3+QgXFXAeTyPtMmReyWxThABtE="
;

/* Public key */
static const char * jbjPublicDSA =
"mQFCBDu6XHwRAwCTIHRgKeIlOFUIEZeJVYSrXn0eUrM5S8OF471tTc+IV7AwiXBR"
"zCFCan4lO1ipmoAipyN2A6ZX0HWOcWdYlWz2adxA7l8JNiZTzkemA562xwex2wLy"
"AQWVTtRN6jv0LccAoN4UWZkIvkT6tV918sEvDEggGARxAv9190RhrDq/GMqd+AHm"
"qWrRkrBRHDUBBL2fYEuU3gFekYrW5CDIN6s3Mcq/yUsvwHl7bwmoqbf2qabbyfnv"
"Y66ETOPKLcw67ggcptHXHcwlvpfJmHKpjK+ByzgauPXXbRAC+gKDjzXL0kAQxjmT"
"2D+16O4vI8Emlx2JVcGLlq/aWhspvQWIzN6PytA3iKZ6uzesrM7yXmqzgodZUsJh"
"1wwl/0K5OIJn/oD41UayU8RXNER8SzDYvDYsJymFRwE1s58lL7QpSmVmZiBKb2hu"
"c29uIChBUlMgTjNOUFEpIDxqYmpAcmVkaGF0LmNvbT6IVwQTEQIAFwUCO7pcfAUL"
"BwoDBAMVAwIDFgIBAheAAAoJEIHSpVbYjoOZBToAn3TXaAI+bhg51EeyaiFip/6W"
"OVwBAJ44rTtNsgZBQxXISjB64CWxl4VaWQ=="
;

/* Signature */
static const char * abcSignatureDSA =
"iD8DBQA7vII+gdKlVtiOg5kRAvg4AJ0fV3gDBADobAnK2HOkV88bfmFMEgCeNysO"
"nP3dWWJnp0Pnbor7pIob4Dk="
;

int
main (int argc, char *argv[])
{
    pgpDig dig;
    int printing = 1;
    int rc;

    rpmInitCrypto();
    dig = pgpNewDig();

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
	void *digest = NULL;
	size_t digestlen = 0;
	const char * txt = "abc";
	SECItem digitem;
	
	rpmDigestUpdate(ctx, txt, strlen(txt));
	rpmDigestUpdate(ctx, dsig->hash, dsig->hashlen);
	rpmDigestFinal(ctx, &digest, &digestlen, 0);

fprintf(stderr, "\n    hm = [ 160]: %s\n\n", pgpHexStr(digest, digestlen));
	digitem.type = siBuffer;
	digitem.data = digest;
	digitem.len = digestlen;

	rc = VFY_VerifyDigest(&digitem, dig->dsa, dig->dsasig, SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST, NULL);

	if (digest) {
	    free(digest);
	    digest = NULL;
	}
    }


fprintf(stderr, "=============================== DSA verify: rc %d\n", rc);

    dig = pgpFreeDig(dig);

    return rc;
}

