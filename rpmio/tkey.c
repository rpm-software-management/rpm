/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 * Routines to handle RFC-2440 detached signatures.
 */

static int _debug = 0;

#include "system.h"
#include "rpmio.h"

#include "rpmpgp.h"
#include "rsapk.h"
#include "rsa.h"
#include "dsa.h"

#include "debug.h"

#include <stdio.h>

static int doit(const char *sig)
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
	exit(rc);
    }

    rc = pgpPrtPkts(dec, declen);
    if (rc < 0) {
	fprintf(stderr, "*** pgpPrtPkts returns %d\n", rc);
	exit(rc);
    }

    if ((enc = b64encode(dec, declen)) == NULL) {
	fprintf(stderr, "*** b64encode failed\n");
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

/* DSA verification from Red Hat 7.0 time-1.7-12.i386.rpm */
static const char * dsa_pkgfn = "7.0/i386/time-1.7-12.i386.rpm";
static const char * dsa_hm = "2de2a0ca8c289ea5add64f8cf64e0b5893d0607b";
static const char * dsa_r = "2247352810d76f6273b4e8d906f5ddfa7fcd4c45";
static const char * dsa_s = "02275c68fcf18edb272a019309c8c81e2e4998bf";

static const char * dsa_p = "c12abdc197a3cef014341fcb09a03ea9f54ed8ca5ee98ed66a8a8be23f05cff83096778bbc1d0cdcc40679ee6f2cb00d4b2b5171f777282610f3335df4fa9e411bd9984964a12da6d1dcdd4a5dd5d0618728c1338c1b17706111abd52a70f10c9d2e18d57d4d554a5e31b7a6eea4f6d625bca3920e59dcfec1954394266af1df";

static const char * dsa_q = "a1b35510319a59825c721e73e41d687ffe351bc9";

static const char * dsa_g = "a8a2eaaec6bbfbe15d79cb4ba9b5e683673d5c9bca7822c3fdb774a8d26551bbaca00d10ada6c60d5507ebdcfee51ce24e3d5312497c2660d60bb5b0f155dc649af98ffb8ec88b92d42f0a22cf967710b75755f843661d1e417480cf583cca0556e05a74839ea43a2ab67b4fa02567be153298c6c7191653abd5db1b165c22cf";

static const char * dsa_y = "4ffefb48980f0264dde3c4682d2b9186b43349fdc32186ebf9b677f0160e1bfd51a8d89afc9a12d4aa821b13acde1809996d4a5d7240dca3c0f97e2166a937f9fbefa9084a492c9924cde97fdf6376c38ab33eb7113028815111123e308b778639acfa14f04e2438773fe01b8737dd37b620ac08f9680ab5ac419ed47717d0e4";

/* RSA verification from Red Hat 5.2 time-1.7-6.i386.rpm */
static const char * rsa_pkgfn = "5.2/i386/time-1.7-6.i386.rpm";
static const char * rsa_m = "f0bd35cbeb3870b5c7c283a7e1e76c42";
static const char * rsa_n = "a1b8fd5f6851288373dc0881ee819a0e9eac31822f3985ca2a00868f240c8252adc9a217da9374fabe7e3ef6ea9816d6249bd18eecc999fd636960426e4790f3837cd8bc796c93152c5bafcca0a2c2d10a54cfc6a1d586d2271ab7d934ba12a6fd4b7563a67b4c8ea4e4099e085d38ccca5e981fe60f38bda520e8f1cba29bf9";
static const char * rsa_e = "11";

static const char * rsa_c = "5075c56cbbde79bd5c990e4f2ebce5d65745f7be84fd077698305be350d1d40fa175545aeec101d529b18912ad68e4c380e8e5ca477bc9652b5710c178ed359725f41a7e5825f32c194c7e56f9e32529abba36bd2569b515de59ac004e15b20bbc34293322ba76c0d55b796d99017b6b2acc15fab79f2eab94869ee5c0ca47f4";

static const char * gpg_private = "\
lQHPBDtnFRoRBADyRAlldI9km29INDTX82Sz5jXrsCcS93rzV66BcKQTcP+CPL1p\n\
kBDaQwf0asQiWr4uaPPUtWj0urSVz7obzyGdILyf/2XVFJWUU9xGtFFEFKMHwdSZ\n\
yGRpyTsd+jsUVPIR9X/fjJDvufuvcKtFHLU+F28nWv7C47rURAXEAoYCewCg5qDr\n\
4fEr1kT3ACg4T1e98QlTRfsD/RDZCp5wSGa5hp4YtoDPza5cTrYopGJSXAXuQVi7\n\
dGqTrAE3hdbWUmbG5hNerJRwOA0DK74Tn38/xoY091q3nH2UlEd3zLhoueOt6apa\n\
VY98UmOaG+Ef5QGd2edHn26kSQq1N8P73FQ3+34ICoRwq+hNNi9cENWiuudi6PKs\n\
eJlmA/sHR8dzcXSuwda8u2r2PvsqAu66pdA1gMdCdwSs3dtJTro3apXvASJDFqkN\n\
y9Vbo6GZ8wmpWYYHM3Xd00rLPTZFzc+F0rBiGscyIZOTDyx8ofK30DHrRfMKytSm\n\
K7TSCWmzKFDTSQxAUQd74qi6dJCVQ0dTuHWmXc1cKPvMEm6IWf8DAwIaLXrq0DYP\n\
j2AdZ4iCkENxNzK2k3OzIsJvaYGuJAhmujAtkS03LLs89/0mIzpEU0FfZmFjdG9y\n\
OgAAr0yL/JSDBRJlphGoWiOVfPhNpGFzHm/9JiM6RFNBX2ZhY3RvcjoAAK93Twk1\n\
SRG6vnHwq6211cGbSUnr28JF/SYjOkRTQV9mYWN0b3I6AACvVJWEWQPciRahMYqY\n\
Bq0a7YgA3QB3w7QpSmVmZiBKb2huc29uIChBUlMgTjNOUFEpIDxqYmpAcmVkaGF0\n\
LmNvbT6IVwQTEQIAFwUCO2cVGgULBwoDBAMVAwIDFgIBAheAAAoJEKNiNe+PnmDb\n\
sM4AmwTaY99wLwi9VE8tML5AS2gn89lIAJ4+BgiRQydv3L404qLZHVoq0KOkbp0B\n\
RgQ7ZxUeEAQAx1F12EdF0NMS1D78hPYKaKxhCkCs9KDgDRfvX4kUBm0Omy/LjvSD\n\
K6ol2k4CUgdHDI1DZwUB+g+zbpWP/osKah5pusNjf6rKUfeYSClOs1TqomMa4ur6\n\
FAnFqjm6rMiptTEb40FWQKDZG+iiLj/m3DrRzETvlAhEkBSuWdLCHcMAAwUEALt6\n\
Mo9BRDOgsZgC4gr+BFj1qj3UkpujduIrojdedVENzc9cvbxZ0uv762mt/TWkoVsd\n\
QPNe+Qh9UjBBchZQYBPjNSJumDyxZa9vk4yaBG+ekZW+qsz8IjuFhA4uCY3u2FoA\n\
Y9Sa7Gs4EKO1OnGuuCtpe5IPfqFolz55rlK/dbJj/wMDAhoteurQNg+PYKaUPggu\n\
Pw3pzYAm1keoYnJNGesJ6CxS6C77eJ2betFwgIeLtevzwmnFejCo/SYjOkVMR19m\n\
YWN0b3I6AACrBQvdBi1eWnh99itbVASMBUFew+yXAf0mIzpFTEdfZmFjdG9yOgAA\n\
qwavqmFHo+0ebuTE/7oFpE5qqh1hZEn9JiM6RUxHX2ZhY3RvcjoAAKsE0zE2Kw1N\n\
zwiX22mx1hjFTJ5AHhPd/SYjOkVMR19mYWN0b3I6AACrB+yUJFgU+djqGspx0xjx\n\
/gz7sIH/e/0mIzpFTEdfZmFjdG9yOgAAqwcV+xN8WTB/A0d2FbLBgH9taqf3TteI\n\
RgQYEQIABgUCO2cVHgAKCRCjYjXvj55g2+kGAKCh1/9z5GrteLdQcoduM7EBhmXD\n\
HwCeKBfXD33WreQdRYpOxNX9cRgOAQE=\n\
";


int
main (int argc, char *argv[])
{
    mp32barrett p;
    mp32barrett q;
    mp32number g;
    mp32number y;
    mp32number hm;
    mp32number r;
    mp32number s;

    rsapk rsa_pk;
    mp32number m;
    mp32number c;

    int rc;

fprintf(stderr, "=============================== Red Hat DSA Public Key\n");
    if ((rc = doit(redhatPubKeyDSA)) != 0)
	return rc;

    mp32bzero(&p);	mp32bsethex(&p, dsa_p);
    mp32bzero(&q);	mp32bsethex(&q, dsa_q);
    mp32nzero(&g);	mp32nsethex(&g, dsa_g);
    mp32nzero(&y);	mp32nsethex(&y, dsa_y);

    mp32nzero(&hm);	mp32nsethex(&hm, dsa_hm);
    mp32nzero(&r);	mp32nsethex(&r, dsa_r);
    mp32nzero(&s);	mp32nsethex(&s, dsa_s);

    rc = dsavrfy(&p, &q, &g, &hm, &y, &r, &s);

fprintf(stderr, "=============================== DSA verify %s: rc %d\n",
	dsa_pkgfn, rc);

    mp32bfree(&p);
    mp32bfree(&q);
    mp32nfree(&g);
    mp32nfree(&y);

    mp32nfree(&hm);
    mp32nfree(&r);
    mp32nfree(&s);

fprintf(stderr, "=============================== Red Hat RSA Public Key\n");
    if ((rc = doit(redhatPubKeyRSA)) != 0)
	return rc;

    rsapkInit(&rsa_pk);

    mp32bzero(&rsa_pk.n);	mp32bsethex(&rsa_pk.n, rsa_n);
    mp32nzero(&rsa_pk.e);	mp32nsethex(&rsa_pk.e, rsa_e);

    mp32nzero(&m);	mp32nsethex(&m, rsa_m);
    mp32nzero(&c);	mp32nsethex(&s, rsa_c);

#if 0
    rc = rsavrfy(&rsa_pk, &m, &c);
#else
    rc = 1;
#endif

fprintf(stderr, "=============================== RSA verify %s: rc %d\n",
	rsa_pkgfn, rc);

    rsapkFree(&rsa_pk);

fprintf(stderr, "=============================== A DSA Private Key\n");
    if ((rc = doit(gpg_private)) != 0)
	return rc;

    return rc;
}
