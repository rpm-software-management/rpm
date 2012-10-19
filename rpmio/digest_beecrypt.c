#include "system.h"

#include <beecrypt.h>
#include <dsa.h>
#include <endianness.h>
#include <md5.h>
#include <mp.h>
#include <rsa.h>
#include <rsapk.h>
#include <sha1.h>
#if HAVE_BEECRYPT_API_H
#include <sha256.h>
#include <sha384.h>
#include <sha512.h>
#endif

#include <rpm/rpmpgp.h>
#include "rpmio/digest.h"
#include "rpmio/rpmio_internal.h"
#include "debug.h"

/**
 * MD5/SHA1 digest private data.
 */
struct DIGEST_CTX_s {
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    int algo;			/*!< Used hash algorithm */
    uint32_t datalen;		/*!< No. bytes in block of plaintext data. */
    uint32_t paramlen;		/*!< No. bytes of digest parameters. */
    uint32_t digestlen;		/*!< No. bytes of digest. */
    void * param;		/*!< Digest parameters. */
    int (*Reset) (void * param);	/*!< Digest initialize. */
    int (*Update) (void * param, const byte * data, size_t size);	/*!< Digest transform. */
    int (*Digest) (void * param, byte * digest);	/*!< Digest finish. */
};


/****************************  init   ************************************/

int rpmInitCrypto(void) {
    return 0;
}

int rpmFreeCrypto(void) {
    return 0;
}

/****************************  digest ************************************/

DIGEST_CTX rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx = NULL;
    if (octx) {
	nctx = memcpy(xcalloc(1, sizeof(*nctx)), octx, sizeof(*nctx));
	nctx->param = memcpy(xcalloc(1, nctx->paramlen), octx->param, nctx->paramlen);
    }
    return nctx;
}

size_t rpmDigestLength(int hashalgo)
{
    switch (hashalgo) {
    case PGPHASHALGO_MD5:
	return 16;
    case PGPHASHALGO_SHA1:
	return 20;
#if HAVE_BEECRYPT_API_H
    case PGPHASHALGO_SHA256:
	return 32;
    case PGPHASHALGO_SHA384:
	return 48;
    case PGPHASHALGO_SHA512:
	return 64;
#endif
    default:
	return 0;
    }
}

DIGEST_CTX rpmDigestInit(int hashalgo, rpmDigestFlags flags)
{
    DIGEST_CTX ctx = xcalloc(1, sizeof(*ctx));

    ctx->flags = flags;
    ctx->algo = hashalgo;

    switch (hashalgo) {
    case PGPHASHALGO_MD5:
	ctx->digestlen = 16;
	ctx->datalen = 64;
	ctx->paramlen = sizeof(md5Param);
	ctx->param = xcalloc(1, ctx->paramlen);
	ctx->Reset = (void *) md5Reset;
	ctx->Update = (void *) md5Update;
	ctx->Digest = (void *) md5Digest;
	break;
    case PGPHASHALGO_SHA1:
	ctx->digestlen = 20;
	ctx->datalen = 64;
	ctx->paramlen = sizeof(sha1Param);
	ctx->param = xcalloc(1, ctx->paramlen);
	ctx->Reset = (void *) sha1Reset;
	ctx->Update = (void *) sha1Update;
	ctx->Digest = (void *) sha1Digest;
	break;
#if HAVE_BEECRYPT_API_H
    case PGPHASHALGO_SHA256:
	ctx->digestlen = 32;
	ctx->datalen = 64;
	ctx->paramlen = sizeof(sha256Param);
	ctx->param = xcalloc(1, ctx->paramlen);
	ctx->Reset = (void *) sha256Reset;
	ctx->Update = (void *) sha256Update;
	ctx->Digest = (void *) sha256Digest;
	break;
    case PGPHASHALGO_SHA384:
	ctx->digestlen = 48;
	ctx->datalen = 128;
	ctx->paramlen = sizeof(sha384Param);
	ctx->param = xcalloc(1, ctx->paramlen);
	ctx->Reset = (void *) sha384Reset;
	ctx->Update = (void *) sha384Update;
	ctx->Digest = (void *) sha384Digest;
	break;
    case PGPHASHALGO_SHA512:
	ctx->digestlen = 64;
	ctx->datalen = 128;
	ctx->paramlen = sizeof(sha512Param);
	ctx->param = xcalloc(1, ctx->paramlen);
	ctx->Reset = (void *) sha512Reset;
	ctx->Update = (void *) sha512Update;
	ctx->Digest = (void *) sha512Digest;
	break;
#endif
    case PGPHASHALGO_RIPEMD160:
    case PGPHASHALGO_MD2:
    case PGPHASHALGO_TIGER192:
    case PGPHASHALGO_HAVAL_5_160:
    default:
	free(ctx);
	return NULL;
    }

    (*ctx->Reset)(ctx->param);
    return ctx;
}

int rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
    if (ctx == NULL)
	return -1;

    return (*ctx->Update) (ctx->param, data, len);
}

int rpmDigestFinal(DIGEST_CTX ctx, void ** datap, size_t *lenp, int asAscii)
{
    byte * digest;
    char * t;
    int i;

    if (ctx == NULL)
	return -1;
    digest = xmalloc(ctx->digestlen);

    /* FIX: check rc */
    (void) (*ctx->Digest) (ctx->param, digest);

    /* Return final digest. */
    if (!asAscii) {
	if (lenp) *lenp = ctx->digestlen;
	if (datap) {
	    *datap = digest;
	    digest = NULL;
	}
    } else {
	if (lenp) *lenp = (2*ctx->digestlen) + 1;
	if (datap) {
	    const byte * s = (const byte *) digest;
	    static const char hex[] = "0123456789abcdef";

	    *datap = t = xmalloc((2*ctx->digestlen) + 1);
	    for (i = 0 ; i < ctx->digestlen; i++) {
		*t++ = hex[ (unsigned)((*s >> 4) & 0x0f) ];
		*t++ = hex[ (unsigned)((*s++   ) & 0x0f) ];
	    }
	    *t = '\0';
	}
    }
    if (digest) {
	memset(digest, 0, ctx->digestlen);	/* In case it's sensitive */
	free(digest);
    }
    memset(ctx->param, 0, ctx->paramlen);	/* In case it's sensitive */
    free(ctx->param);
    memset(ctx, 0, sizeof(*ctx));	/* In case it's sensitive */
    free(ctx);
    return 0;
}

/**************************** helpers ************************************/

static inline char * pgpHexCvt(char *t, const byte *s, int nbytes)
{
    static char hex[] = "0123456789abcdef";
    while (nbytes-- > 0) { 
        unsigned int i;
        i = *s++;
        *t++ = hex[ (i >> 4) & 0xf ];
        *t++ = hex[ (i     ) & 0xf ];
    }    
    *t = '\0';
    return t;
}

static const char * pgpMpiHex(const byte *p, const byte *pend)
{
    static char prbuf[2048];
    char *t = prbuf;
    int nbytes =  pgpMpiLen(p) - 2;
    if (nbytes > 1024 || nbytes > pend - (p + 2))
	return NULL;
    t = pgpHexCvt(t, p+2, nbytes);
    return prbuf;
}

static int pgpHexSet(int lbits, mpnumber * mpn, const byte * p, const byte * pend)
{
    unsigned int mbits = pgpMpiBits(p);
    unsigned int nbits;
    unsigned int nbytes;
    char *t;
    unsigned int ix;

    nbits = (lbits > mbits ? lbits : mbits);
    nbytes = ((nbits + 7) >> 3);
    t = xmalloc(2*nbytes+1);
    ix = 2 * ((nbits - mbits) >> 3);

    if (ix > 0) memset(t, (int)'0', ix);
    strcpy(t+ix, pgpMpiHex(p, pend));
    (void) mpnsethex(mpn, t);
    t = _free(t);
    return 0;
}

static void pgpFreeSigRSADSA(pgpDigAlg sa)
{
    if (sa->data)
	free(sa->data);
    sa->data = 0;
}

static void pgpFreeKeyRSADSA(pgpDigAlg sa)
{
    if (sa->data)
	free(sa->data);
    sa->data = 0;
}


/****************************** RSA **************************************/

struct pgpDigSigRSA_s {
    mpnumber c;
};

struct pgpDigKeyRSA_s {
    rsapk rsa_pk;
};

static int pgpSetSigMpiRSA(pgpDigAlg pgpsig, int num,
                           const uint8_t *p, const uint8_t *pend)
{
    struct pgpDigSigRSA_s *sig = pgpsig->data;
    int rc = 1;

    switch (num) {
    case 0:
	sig = pgpsig->data = xcalloc(1, sizeof(*sig));
	(void) mpnsethex(&sig->c, pgpMpiHex(p, pend));
	rc = 0;
	break;
    }
    return rc;
}

static int pgpSetKeyMpiRSA(pgpDigAlg pgpkey, int num,
                           const uint8_t *p, const uint8_t *pend)
{
    struct pgpDigKeyRSA_s *key = pgpkey->data;
    int rc = 1;

    if (!key)
	key = pgpkey->data = xcalloc(1, sizeof(*key));
    switch (num) {
    case 0:
	(void) mpbsethex(&key->rsa_pk.n, pgpMpiHex(p, pend));
	rc = 0;
	break;
    case 1:
	(void) mpnsethex(&key->rsa_pk.e, pgpMpiHex(p, pend));
	rc = 0;
	break;
    }
    return rc;
}

static int pgpVerifySigRSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig, uint8_t *hash, size_t hashlen, int hash_algo)
{
    struct pgpDigKeyRSA_s *key = pgpkey->data;
    struct pgpDigSigRSA_s *sig = pgpsig->data;
    const char * prefix = NULL;
    mpnumber rsahm;
    int rc = 1;

    if (!sig || !key)
	return rc;

    switch (hash_algo) {
    case PGPHASHALGO_MD5:
        prefix = "3020300c06082a864886f70d020505000410";
        break;
    case PGPHASHALGO_SHA1:
        prefix = "3021300906052b0e03021a05000414";
        break;
    case PGPHASHALGO_MD2:
        prefix = "3020300c06082a864886f70d020205000410";
        break;
    case PGPHASHALGO_SHA256:
        prefix = "3031300d060960864801650304020105000420";
        break;
    case PGPHASHALGO_SHA384:
        prefix = "3041300d060960864801650304020205000430";
        break;
    case PGPHASHALGO_SHA512:
        prefix = "3051300d060960864801650304020305000440";
        break;
    default:
	return 1;
    }

    /* Generate RSA modulus parameter. */
    {   unsigned int nbits = MP_WORDS_TO_BITS(sig->c.size);
        unsigned int nb = (nbits + 7) >> 3;
        byte *buf, *bp;

	if (nb < 3)
	    return 1;
	buf = xmalloc(nb);
	memset(buf, 0xff, nb);
	buf[0] = 0x00;
	buf[1] = 0x01;
	bp = buf + nb - strlen(prefix)/2 - hashlen - 1;
	if (bp < buf)
	    return 1;
	*bp++ = 0;
	for (; *prefix; prefix += 2)
	    *bp++ = (rnibble(prefix[0]) << 4) | rnibble(prefix[1]);
        memcpy(bp, hash, hashlen);
        mpnzero(&rsahm);
        (void) mpnsetbin(&rsahm, buf, nb);
        buf = _free(buf);
    }
#if HAVE_BEECRYPT_API_H
    rc = rsavrfy(&key->rsa_pk.n, &key->rsa_pk.e, &sig->c, &rsahm) == 1 ? 0 : 1;
#else
    rc = rsavrfy(&key->rsa_pk, &rsahm, &sig->c) == 1 ? 0 : 1;
#endif
    mpnfree(&rsahm);
    return rc;
}


/****************************** DSA **************************************/

struct pgpDigSigDSA_s {
    mpnumber r;
    mpnumber s;
};

struct pgpDigKeyDSA_s {
    mpbarrett p;
    mpbarrett q;
    mpnumber g;
    mpnumber y;
};

static int pgpSetSigMpiDSA(pgpDigAlg pgpsig, int num,
                           const uint8_t *p, const uint8_t *pend)
{
    struct pgpDigSigDSA_s *sig = pgpsig->data;
    int rc = 1;

    switch (num) {
    case 0:
	sig = pgpsig->data = xcalloc(1, sizeof(*sig));
	rc = pgpHexSet(160, &sig->r, p, pend);
	break;
    case 1:
	rc = pgpHexSet(160, &sig->s, p, pend);
	break;
    }
    return rc;
}

static int pgpSetKeyMpiDSA(pgpDigAlg pgpkey, int num,
                           const uint8_t *p, const uint8_t *pend)
{
    struct pgpDigKeyDSA_s *key = pgpkey->data;
    int rc = 1;

    if (!key)
	key = pgpkey->data = xcalloc(1, sizeof(*key));

    switch (num) {
    case 0:
	mpbsethex(&key->p, pgpMpiHex(p, pend));
	rc = 0;
	break;
    case 1:
	mpbsethex(&key->q, pgpMpiHex(p, pend));
	rc = 0;
	break;
    case 2:
	mpnsethex(&key->g, pgpMpiHex(p, pend));
	rc = 0;
	break;
    case 3:
	mpnsethex(&key->y, pgpMpiHex(p, pend));
	rc = 0;
	break;
    }
    return rc;
}

static int pgpVerifySigDSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig, uint8_t *hash, size_t hashlen, int hash_algo)
{
    struct pgpDigKeyDSA_s *key = pgpkey->data;
    struct pgpDigSigDSA_s *sig = pgpsig->data;
    mpnumber hm;
    int rc = 1;

    if (sig && key) {
	mpnzero(&hm);
	mpnsetbin(&hm, hash, hashlen);
	rc = dsavrfy(&key->p, &key->q, &key->g, &hm, &key->y, &sig->r, &sig->s) == 1 ? 0 : 1;
	mpnfree(&hm);
    }
    return rc;
}

static int pgpSetMpiNULL(pgpDigAlg pgpkey, int num,
                         const uint8_t *p, const uint8_t *pend)
{
    return 1;
}

static int pgpVerifyNULL(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
                         uint8_t *hash, size_t hashlen, int hash_algo)
{
    return 1;
}

pgpDigAlg pgpPubkeyNew(int algo)
{
    pgpDigAlg ka = xcalloc(1, sizeof(*ka));;

    switch (algo) {
    case PGPPUBKEYALGO_RSA:
        ka->setmpi = pgpSetKeyMpiRSA;
        ka->free = pgpFreeKeyRSADSA;
        ka->mpis = 2;
        break;
    case PGPPUBKEYALGO_DSA:
        ka->setmpi = pgpSetKeyMpiDSA;
        ka->free = pgpFreeKeyRSADSA;
        ka->mpis = 4;
        break;
    default:
        ka->setmpi = pgpSetMpiNULL;
        ka->mpis = -1;
        break;
    }

    ka->verify = pgpVerifyNULL; /* keys can't be verified */

    return ka;
}

pgpDigAlg pgpSignatureNew(int algo)
{
    pgpDigAlg sa = xcalloc(1, sizeof(*sa));

    switch (algo) {
    case PGPPUBKEYALGO_RSA:
        sa->setmpi = pgpSetSigMpiRSA;
        sa->free = pgpFreeSigRSADSA;
        sa->verify = pgpVerifySigRSA;
        sa->mpis = 1;
        break;
    case PGPPUBKEYALGO_DSA:
        sa->setmpi = pgpSetSigMpiDSA;
        sa->free = pgpFreeSigRSADSA;
        sa->verify = pgpVerifySigDSA;
        sa->mpis = 2;
        break;
    default:
        sa->setmpi = pgpSetMpiNULL;
        sa->verify = pgpVerifyNULL;
        sa->mpis = -1;
        break;
    }
    return sa;
}
