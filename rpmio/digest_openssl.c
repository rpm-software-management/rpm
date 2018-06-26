#include "system.h"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <rpm/rpmpgp.h>

#include "rpmio/digest.h"


/* Compatibility functions for OpenSSL 1.0.2 */

#ifndef HAVE_EVP_MD_CTX_NEW
# define EVP_MD_CTX_new EVP_MD_CTX_create
# define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif

#ifndef HAVE_RSA_SET0_KEY
int RSA_set0_key(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d);
int RSA_set0_key(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d)
{
    if (!r) return 0;

    if (n) {
        r->n = n;
    }

    if (e) {
        r->e = e;
    }

    if (d) {
        r->d = d;
    }

    return 1;
}
#endif /* HAVE_RSA_SET0_KEY */

#ifndef HAVE_DSA_SET0_KEY
int DSA_set0_key(DSA *d, BIGNUM *pub_key, BIGNUM *priv_key);

int DSA_set0_key(DSA *d, BIGNUM *pub_key, BIGNUM *priv_key)
{
    if (!d) return 0;

    if (pub_key) {
        d->pub_key = pub_key;
    }

    if (priv_key) {
        d->priv_key = priv_key;
    }

    return 1;
}
#endif /* HAVE_DSA_SET0_KEY */

#ifndef HAVE_DSA_SET0_PQG
int DSA_set0_pqg(DSA *d, BIGNUM *p, BIGNUM *q, BIGNUM *g);

int DSA_set0_pqg(DSA *d, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
    if (!d) return 0;

    if (p) {
        d->p = p;
    }

    if (q) {
        d->q = q;
    }

    if (g) {
        d->g = g;
    }

    return 1;
}
#endif /* HAVE_DSA_SET0_PQG */

#ifndef HAVE_DSA_SIG_SET0
int DSA_SIG_set0(DSA_SIG *sig, BIGNUM *r, BIGNUM *s);

int DSA_SIG_set0(DSA_SIG *sig, BIGNUM *r, BIGNUM *s)
{
    if (!sig) return 0;

    if (r) {
        sig->r = r;
    }

    if (s) {
        sig->s = s;
    }

    return 1;
}
#endif /* HAVE_DSA_SIG_SET0 */

#ifndef HAVE_BN2BINPAD
static int BN_bn2binpad(const BIGNUM *a, unsigned char *to, int tolen)
{
    int i;

    i = BN_num_bytes(a);
    if (tolen < i)
        return -1;

    /* Add leading zeroes if necessary */
    if (tolen > i) {
        memset(to, 0, tolen - i);
        to += tolen - i;
    }

    BN_bn2bin(a, to);

    return tolen;
}
#endif /* HAVE_BN2BINPAD */

struct DIGEST_CTX_s {
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    int algo;			/*!< Used hash algorithm */

    EVP_MD_CTX *md_ctx; /* Digest context (opaque) */

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
    if (!octx) return NULL;

    DIGEST_CTX nctx = NULL;
    nctx = xcalloc(1, sizeof(*nctx));

    nctx->flags = octx->flags;
    nctx->algo = octx->algo;
    nctx->md_ctx = EVP_MD_CTX_new();
    if (!nctx->md_ctx) {
        free(nctx);
        return NULL;
    }

    if (!EVP_MD_CTX_copy(nctx->md_ctx, octx->md_ctx)) {
        free(nctx);
        return NULL;
    }

    return nctx;
}

static const EVP_MD *getEVPMD(int hashalgo)
{
    switch (hashalgo) {

    case PGPHASHALGO_MD5:
        return EVP_md5();

    case PGPHASHALGO_SHA1:
        return EVP_sha1();

    case PGPHASHALGO_SHA256:
        return EVP_sha256();

    case PGPHASHALGO_SHA384:
        return EVP_sha384();

    case PGPHASHALGO_SHA512:
        return EVP_sha512();

    case PGPHASHALGO_SHA224:
        return EVP_sha224();

    default:
        return EVP_md_null();
    }
}

size_t rpmDigestLength(int hashalgo)
{
    return EVP_MD_size(getEVPMD(hashalgo));
}

DIGEST_CTX rpmDigestInit(int hashalgo, rpmDigestFlags flags)
{
    DIGEST_CTX ctx = xcalloc(1, sizeof(*ctx));

    ctx->md_ctx = EVP_MD_CTX_new();
    if (!ctx->md_ctx) {
        free(ctx);
        return NULL;
    }

    const EVP_MD *md = getEVPMD(hashalgo);
    if (md == EVP_md_null()) {
        free(ctx->md_ctx);
        free(ctx);
        return NULL;
    }

    ctx->algo = hashalgo;
    ctx->flags = flags;
    if (!EVP_DigestInit_ex(ctx->md_ctx, md, NULL)) {
        free(ctx->md_ctx);
        free(ctx);
        return NULL;
    }

    return ctx;
}

int rpmDigestUpdate(DIGEST_CTX ctx, const void *data, size_t len)
{
    if (ctx == NULL) return -1;

    EVP_DigestUpdate(ctx->md_ctx, data, len);

    return 0;
}

int rpmDigestFinal(DIGEST_CTX ctx, void ** datap, size_t *lenp, int asAscii)
{
    int ret;
    unsigned char *digest = NULL;
    unsigned int digestlen;

    if (ctx == NULL) return -1;

    digestlen = EVP_MD_CTX_size(ctx->md_ctx);
    digest = xcalloc(digestlen, sizeof(*digest));

    ret = EVP_DigestFinal_ex(ctx->md_ctx, digest, &digestlen);
    if (ret != 1) goto done;

    if (!asAscii) {
        /* Raw data requested */
        if (lenp) *lenp = digestlen;
        if (datap) {
            *datap = digest;
            digest = NULL;
        }
    }

    else {
        /* ASCII requested */
        if (lenp) *lenp = (2*digestlen) + 1;
        if (datap) {
            const uint8_t * s = (const uint8_t *) digest;
            *datap = pgpHexStr(s, digestlen);
        }
    }

    ret = 1;

done:
    if (digest) {
        /* Zero the digest, just in case it's sensitive */
        memset(digest, 0, digestlen);
        free(digest);
    }

    EVP_MD_CTX_free(ctx->md_ctx);
    free(ctx);

    if (ret != 1) {
        return -1;
    }

    return 0;
}


/****************************** RSA **************************************/

/* Key */

struct pgpDigKeyRSA_s {
    size_t nbytes; /* Size of modulus */

    BIGNUM *n; /* Common Modulus */
    BIGNUM *e; /* Public Exponent */

    EVP_PKEY *evp_pkey; /* Fully constructed key */
};

static int constructRSASigningKey(struct pgpDigKeyRSA_s *key)
{
    if (key->evp_pkey) {
        /* We've already constructed it, so just reuse it */
        return 1;
    }

    /* Create the RSA key */
    RSA *rsa = RSA_new();
    if (!rsa) return 0;

    if (!RSA_set0_key(rsa, key->n, key->e, NULL)) {
        RSA_free(rsa);
        return 0;
    }

    /* Create an EVP_PKEY container to abstract the key-type. */
    key->evp_pkey = EVP_PKEY_new();
    if (!key->evp_pkey) {
        RSA_free(rsa);
        return 0;
    }

    /* Assign the RSA key to the EVP_PKEY structure.
       This will take over memory management of the RSA key */
    if (!EVP_PKEY_assign_RSA(key->evp_pkey, rsa)) {
        EVP_PKEY_free(key->evp_pkey);
        key->evp_pkey = NULL;
        RSA_free(rsa);
    }

    return 1;
}

static int pgpSetKeyMpiRSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    size_t mlen = pgpMpiLen(p) - 2;
    struct pgpDigKeyRSA_s *key = pgpkey->data;

    if (!key) {
        key = pgpkey->data = xcalloc(1, sizeof(*key));
    }

    switch (num) {
    case 0:
        /* Modulus */
        if (key->n) {
            /* This should only ever happen once per key */
            return 1;
        }

        key->nbytes = mlen;
        /* Create a BIGNUM from the pointer.
           Note: this assumes big-endian data as required by PGP */
        key->n = BN_bin2bn(p+2, mlen, NULL);
        if (!key->n) return 1;
        break;

    case 1:
        /* Exponent */
        if (key->e) {
            /* This should only ever happen once per key */
            return 1;
        }

        /* Create a BIGNUM from the pointer.
           Note: this assumes big-endian data as required by PGP */
        key->e = BN_bin2bn(p+2, mlen, NULL);
        if (!key->e) return 1;
        break;
    }

    return 0;
}

static void pgpFreeKeyRSA(pgpDigAlg pgpkey)
{
    struct pgpDigKeyRSA_s *key = pgpkey->data;
    if (key) {
        if (key->evp_pkey) {
            EVP_PKEY_free(key->evp_pkey);
        } else {
            /* If key->evp_pkey was constructed,
             * the memory management of these BNs
             * are freed with it. */
            BN_clear_free(key->n);
            BN_clear_free(key->e);
        }

        free(key);
    }
}

/* Signature */

struct pgpDigSigRSA_s {
    BIGNUM *bn;
    size_t len;
};

static int pgpSetSigMpiRSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    BIGNUM *bn = NULL;

    int mlen = pgpMpiLen(p) - 2;
    int rc = 1;

    struct pgpDigSigRSA_s *sig = pgpsig->data;
    if (!sig) {
        sig = xcalloc(1, sizeof(*sig));
    }

    switch (num) {
    case 0:
        if (sig->bn) {
            /* This should only ever happen once per signature */
            return 1;
        }

        bn = sig->bn = BN_new();
        if (!bn) return 1;

        /* Create a BIGNUM from the signature pointer.
           Note: this assumes big-endian data as required
           by the PGP multiprecision integer format
           (RFC4880, Section 3.2)
           This will be useful later, as we can
           retrieve this value with appropriate
           padding. */
        bn = BN_bin2bn(p+2, mlen, bn);
        if (!bn) return 1;

        sig->bn = bn;
        sig->len = mlen;

        pgpsig->data = sig;
        rc = 0;
        break;
    }
    return rc;
}

static void pgpFreeSigRSA(pgpDigAlg pgpsig)
{
    struct pgpDigSigRSA_s *sig = pgpsig->data;
    if (sig) {
        BN_clear_free(sig->bn);
        free(pgpsig->data);
    }
}

static int pgpVerifySigRSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
                           uint8_t *hash, size_t hashlen, int hash_algo)
{
    int rc, ret;
    EVP_PKEY_CTX *pkey_ctx = NULL;
    struct pgpDigSigRSA_s *sig = pgpsig->data;

    void *padded_sig = NULL;

    struct pgpDigKeyRSA_s *key = pgpkey->data;

    if (!constructRSASigningKey(key)) {
        rc = 1;
        goto done;
    }

    pkey_ctx = EVP_PKEY_CTX_new(key->evp_pkey, NULL);
    if (!pkey_ctx) {
        rc = 1;
        goto done;
    }

    ret = EVP_PKEY_verify_init(pkey_ctx);
    if (ret < 0) {
        rc = 1;
        goto done;
    }

    ret = EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING);
    if (ret < 0) {
        rc = 1;
        goto done;
    }

    ret = EVP_PKEY_CTX_set_signature_md(pkey_ctx, getEVPMD(hash_algo));
    if (ret < 0) {
        rc = 1;
        goto done;
    }

    int pkey_len = EVP_PKEY_size(key->evp_pkey);
    padded_sig = xcalloc(1, pkey_len);
    if (!BN_bn2binpad(sig->bn, padded_sig, pkey_len)) {
        rc = 1;
        goto done;
    }

    ret = EVP_PKEY_verify(pkey_ctx, padded_sig, pkey_len, hash, hashlen);
    if (ret == 1)
    {
        /* Success */
        rc = 0;
    }
    else
    {
        /* Failure */
        rc = 1;
    }

done:
    EVP_PKEY_CTX_free(pkey_ctx);
    free(padded_sig);
    return rc;
}

/****************************** DSA ***************************************/
/* Key */

struct pgpDigKeyDSA_s {
    BIGNUM *p; /* Prime */
    BIGNUM *q; /* Subprime */
    BIGNUM *g; /* Base */
    BIGNUM *y; /* Public Key */

    DSA *dsa_key; /* Fully constructed key */
};

static int constructDSASigningKey(struct pgpDigKeyDSA_s *key)
{
    int rc;

    if (key->dsa_key) {
        /* We've already constructed it, so just reuse it */
        return 1;
    }

    /* Create the DSA key */
    DSA *dsa = DSA_new();
    if (!dsa) return 0;

    if (!DSA_set0_pqg(dsa, key->p, key->q, key->g)) {
        rc = 0;
        goto done;
    }

    if (!DSA_set0_key(dsa, key->y, NULL)) {
        rc = 0;
        goto done;
    }

    key->dsa_key = dsa;

    rc = 1;
done:
    if (rc == 0) {
        DSA_free(dsa);
    }
    return rc;
}


static int pgpSetKeyMpiDSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    BIGNUM *bn;
    size_t mlen = pgpMpiLen(p) - 2;
    struct pgpDigKeyDSA_s *key = pgpkey->data;

    if (!key) {
        key = pgpkey->data = xcalloc(1, sizeof(*key));
    }

    /* Create a BIGNUM from the key pointer.
       Note: this assumes big-endian data as required
       by the PGP multiprecision integer format
       (RFC4880, Section 3.2) */
    bn = BN_bin2bn(p+2, mlen, NULL);
    if (!bn) return 1;

    switch (num) {
    case 0:
        /* Prime */
        if (key->p) {
            /* This should only ever happen once per key */
            return 1;
        }
        key->p = bn;
        break;

    case 1:
        /* Subprime */
        if (key->q) {
            /* This should only ever happen once per key */
            return 1;
        }
        key->q = bn;
        break;
    case 2:
        /* Base */
        if (key->g) {
            /* This should only ever happen once per key */
            return 1;
        }
        key->g = bn;
        break;
    case 3:
        /* Public */
        if (key->y) {
            /* This should only ever happen once per key */
            return 1;
        }
        key->y = bn;
        break;
    }

    return 0;
}

static void pgpFreeKeyDSA(pgpDigAlg pgpkey)
{
    struct pgpDigKeyDSA_s *key = pgpkey->data;
    if (key) {
        if (key->dsa_key) {
            DSA_free(key->dsa_key);
        } else {
            /* If sig->dsa_key was constructed,
             * the memory management of these BNs
             * are freed with it. */
            BN_clear_free(key->p);
            BN_clear_free(key->q);
            BN_clear_free(key->g);
            BN_clear_free(key->y);
        }
        free(key);
    }
}

/* Signature */

struct pgpDigSigDSA_s {
    BIGNUM *r;
    BIGNUM *s;

    DSA_SIG *dsa_sig;
};

static int constructDSASignature(struct pgpDigSigDSA_s *sig)
{
    int rc;

    if (sig->dsa_sig) {
        /* We've already constructed it, so just reuse it */
        return 1;
    }

    /* Create the DSA signature */
    DSA_SIG *dsa_sig = DSA_SIG_new();
    if (!dsa_sig) return 0;

    if (!DSA_SIG_set0(dsa_sig, sig->r, sig->s)) {
        rc = 0;
        goto done;
    }

    sig->dsa_sig = dsa_sig;

    rc = 1;
done:
    if (rc == 0) {
        DSA_SIG_free(sig->dsa_sig);
    }
    return rc;
}

static int pgpSetSigMpiDSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    BIGNUM *bn = NULL;

    int mlen = pgpMpiLen(p) - 2;
    int rc = 1;

    struct pgpDigSigDSA_s *sig = pgpsig->data;
    if (!sig) {
        sig = xcalloc(1, sizeof(*sig));
    }

    /* Create a BIGNUM from the signature pointer.
       Note: this assumes big-endian data as required
       by the PGP multiprecision integer format
       (RFC4880, Section 3.2) */
    bn = BN_bin2bn(p+2, mlen, NULL);
    if (!bn) return 1;

    switch (num) {
    case 0:
        if (sig->r) {
            /* This should only ever happen once per signature */
            BN_free(bn);
            return 1;
        }
        sig->r = bn;
        rc = 0;
        break;
    case 1:
        if (sig->s) {
            /* This should only ever happen once per signature */
            BN_free(bn);
            return 1;
        }
        sig->s = bn;
        rc = 0;
        break;
    }

    pgpsig->data = sig;

    return rc;
}

static void pgpFreeSigDSA(pgpDigAlg pgpsig)
{
    struct pgpDigSigDSA_s *sig = pgpsig->data;
    if (sig) {
        if (sig->dsa_sig) {
            DSA_SIG_free(sig->dsa_sig);
        } else {
            /* If sig->dsa_sig was constructed,
             * the memory management of these BNs
             * are freed with it. */
            BN_clear_free(sig->r);
            BN_clear_free(sig->s);
        }
        free(pgpsig->data);
    }
}

static int pgpVerifySigDSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
                           uint8_t *hash, size_t hashlen, int hash_algo)
{
    int rc, ret;
    struct pgpDigSigDSA_s *sig = pgpsig->data;

    struct pgpDigKeyDSA_s *key = pgpkey->data;

    if (!constructDSASigningKey(key)) {
        rc = 1;
        goto done;
    }

    if (!constructDSASignature(sig)) {
        rc = 1;
        goto done;
    }

    ret = DSA_do_verify(hash, hashlen, sig->dsa_sig, key->dsa_key);
    if (ret == 1)
    {
        /* Success */
        rc = 0;
    }
    else
    {
        /* Failure */
        rc = 1;
    }

done:
    return rc;
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

/****************************** PGP **************************************/
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
