#include "system.h"

#include <beecrypt/beecrypt.h>
#include <beecrypt/dsa.h>
#include <beecrypt/endianness.h>
#include <beecrypt/md5.h>
#include <beecrypt/mp.h>
#include <beecrypt/rsa.h>
#include <beecrypt/rsapk.h>
#include <beecrypt/sha1.h>
#if HAVE_BEECRYPT_API_H
#include <beecrypt/sha256.h>
#include <beecrypt/sha384.h>
#include <beecrypt/sha512.h>
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


/****************************** RSA **************************************/

struct pgpDigSigRSA_s {
    mpnumber c;
};

struct pgpDigKeyRSA_s {
    rsapk rsa_pk;
    int nbytes;
};

static int pgpSetSigMpiRSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    struct pgpDigSigRSA_s *sig = pgpsig->data;
    int mlen = pgpMpiLen(p) - 2;
    int rc = 1;

    if (!sig)
	sig = pgpsig->data = xcalloc(1, sizeof(*sig));

    switch (num) {
    case 0:
	if (!mpnsetbin(&sig->c, p + 2, mlen))
	    rc = 0;
	break;
    }
    return rc;
}

static int pgpSetKeyMpiRSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    struct pgpDigKeyRSA_s *key = pgpkey->data;
    int mlen = pgpMpiLen(p) - 2;
    int rc = 1;

    if (!key)
	key = pgpkey->data = xcalloc(1, sizeof(*key));

    switch (num) {
    case 0:
	key->nbytes = mlen;
	if (!mpbsetbin(&key->rsa_pk.n, p + 2, mlen))
	    rc = 0;
	break;
    case 1:
	if (!mpnsetbin(&key->rsa_pk.e, p + 2, mlen))
	    rc = 0;
	break;
    }
    return rc;
}

static int pkcs1pad(mpnumber *rsahm, int nbytes, const char *prefix, uint8_t *hash, size_t hashlen)
{
    int datalen = strlen(prefix) / 2 + hashlen;
    byte *buf, *bp;
    int rc = 1;

    if (nbytes < 4 + datalen)
	return 1;
    buf = xmalloc(nbytes);
    memset(buf, 0xff, nbytes);
    buf[0] = 0x00;
    buf[1] = 0x01;
    bp = buf + nbytes - datalen;
    bp[-1] = 0;
    for (; *prefix; prefix += 2)
	*bp++ = (rnibble(prefix[0]) << 4) | rnibble(prefix[1]);
    memcpy(bp, hash, hashlen);
    if (!mpnsetbin(rsahm, buf, nbytes))
	rc = 0;
    buf = _free(buf);
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

    memset(&rsahm, 0, sizeof(rsahm));
    if (pkcs1pad(&rsahm, key->nbytes, prefix, hash, hashlen) != 0)
	return 1;

#if HAVE_BEECRYPT_API_H
    rc = rsavrfy(&key->rsa_pk.n, &key->rsa_pk.e, &sig->c, &rsahm) == 1 ? 0 : 1;
#else
    rc = rsavrfy(&key->rsa_pk, &rsahm, &sig->c) == 1 ? 0 : 1;
#endif
    mpnfree(&rsahm);
    return rc;
}

static void pgpFreeSigRSA(pgpDigAlg pgpsig)
{
    struct pgpDigSigRSA_s *sig = pgpsig->data;
    if (sig) {
	mpnfree(&sig->c);
	pgpsig->data = _free(sig);
    }
}

static void pgpFreeKeyRSA(pgpDigAlg pgpkey)
{
    struct pgpDigKeyRSA_s *key = pgpkey->data;
    if (key) {
	mpbfree(&key->rsa_pk.n);
	mpnfree(&key->rsa_pk.e);
	pgpkey->data = _free(key);
    }
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
    int qbytes;
};

static int pgpSetSigMpiDSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    struct pgpDigSigDSA_s *sig = pgpsig->data;
    int mlen = pgpMpiLen(p) - 2;
    int rc = 1;

    if (!sig)
	sig = pgpsig->data = xcalloc(1, sizeof(*sig));

    switch (num) {
    case 0:
	if (!mpnsetbin(&sig->r, p + 2, mlen))
	    rc = 0;
	break;
    case 1:
	if (!mpnsetbin(&sig->s, p + 2, mlen))
	    rc = 0;
	break;
    }
    return rc;
}

static int pgpSetKeyMpiDSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    struct pgpDigKeyDSA_s *key = pgpkey->data;
    int mlen = pgpMpiLen(p) - 2;
    int rc = 1;

    if (!key)
	key = pgpkey->data = xcalloc(1, sizeof(*key));

    switch (num) {
    case 0:
	if (!mpbsetbin(&key->p, p + 2, mlen))
	    rc = 0;
	break;
    case 1:
	key->qbytes = mlen;
	if (!mpbsetbin(&key->q, p + 2, mlen))
	    rc = 0;
	break;
    case 2:
	if (!mpnsetbin(&key->g, p + 2, mlen))
	    rc = 0;
	break;
    case 3:
	if (!mpnsetbin(&key->y, p + 2, mlen))
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

    if (sig && key && hashlen >= key->qbytes) {
	mpnzero(&hm);
	mpnsetbin(&hm, hash, key->qbytes);
	rc = dsavrfy(&key->p, &key->q, &key->g, &hm, &key->y, &sig->r, &sig->s) == 1 ? 0 : 1;
	mpnfree(&hm);
    }
    return rc;
}

static void pgpFreeSigDSA(pgpDigAlg pgpsig)
{
    struct pgpDigSigDSA_s *sig = pgpsig->data;
    if (sig) {
	mpnfree(&sig->r);
	mpnfree(&sig->s);
	pgpsig->data = _free(sig);
    }
}

static void pgpFreeKeyDSA(pgpDigAlg pgpkey)
{
    struct pgpDigKeyDSA_s *key = pgpkey->data;
    if (key) {
	mpbfree(&key->p);
	mpbfree(&key->q);
	mpnfree(&key->g);
	mpnfree(&key->y);
	pgpkey->data = _free(key);
    }
}


/****************************** NULL **************************************/

static int pgpSetMpiNULL(pgpDigAlg pgpkey, int num, const uint8_t *p)
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
        ka->free = pgpFreeKeyRSA;
        ka->mpis = 2;
        break;
    case PGPPUBKEYALGO_DSA:
        ka->setmpi = pgpSetKeyMpiDSA;
        ka->free = pgpFreeKeyDSA;
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
        sa->free = pgpFreeSigRSA;
        sa->verify = pgpVerifySigRSA;
        sa->mpis = 1;
        break;
    case PGPPUBKEYALGO_DSA:
        sa->setmpi = pgpSetSigMpiDSA;
        sa->free = pgpFreeSigDSA;
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
