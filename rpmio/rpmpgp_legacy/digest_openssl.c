#include "system.h"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <rpm/rpmcrypto.h>

#include "rpmpgp_internal.h"

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

    case RPM_HASH_MD5:
        return EVP_md5();

    case RPM_HASH_SHA1:
        return EVP_sha1();

    case RPM_HASH_SHA256:
        return EVP_sha256();

    case RPM_HASH_SHA384:
        return EVP_sha384();

    case RPM_HASH_SHA512:
        return EVP_sha512();

    case RPM_HASH_SHA224:
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
            *datap = rpmhex(s, digestlen);
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
    unsigned char immutable; /* if set, this key cannot be mutated */
};

static int constructRSASigningKey(struct pgpDigKeyRSA_s *key)
{
    EVP_PKEY_CTX *ctx = NULL;
    OSSL_PARAM_BLD *param_bld = NULL;
    OSSL_PARAM *params =NULL;

    if (key->evp_pkey) {
        /* We've already constructed it, so just reuse it */
        return 1;
    } else if (key->immutable)
	return 0;
    key->immutable = 1;

    ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
    if (!ctx)
	goto exit;
    param_bld = OSSL_PARAM_BLD_new();
    if (!param_bld)
	goto exit;
    if (!OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_RSA_N, key->n))
	goto exit;
    if (!OSSL_PARAM_BLD_push_BN(param_bld, OSSL_PKEY_PARAM_RSA_E, key->e))
	goto exit;
    params = OSSL_PARAM_BLD_to_param(param_bld);
    if (!params)
	goto exit;

    if (!EVP_PKEY_fromdata_init(ctx))
	goto exit;
    if (!EVP_PKEY_fromdata(ctx, &key->evp_pkey, EVP_PKEY_PUBLIC_KEY, params))
	goto exit;
    EVP_PKEY_CTX_free(ctx);
    key->n = key->e = NULL;

    return 1;

 exit:
    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_BLD_free(param_bld);
    OSSL_PARAM_free(params);

    return 0;
}

static int pgpSetKeyMpiRSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    size_t mlen = pgpMpiLen(p) - 2;
    struct pgpDigKeyRSA_s *key = pgpkey->data;

    if (!key)
        key = pgpkey->data = xcalloc(1, sizeof(*key));
    else if (key->immutable)
	return 1;

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
    int rc = 1; /* assume failure */
    EVP_PKEY_CTX *pkey_ctx = NULL;
    struct pgpDigSigRSA_s *sig = pgpsig->data;

    void *padded_sig = NULL;

    struct pgpDigKeyRSA_s *key = pgpkey->data;

    if (!constructRSASigningKey(key))
        goto done;

    pkey_ctx = EVP_PKEY_CTX_new(key->evp_pkey, NULL);
    if (!pkey_ctx)
        goto done;

    if (EVP_PKEY_verify_init(pkey_ctx) != 1)
        goto done;

    if (EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PADDING) <= 0)
        goto done;

    if (EVP_PKEY_CTX_set_signature_md(pkey_ctx, getEVPMD(hash_algo)) <= 0)
        goto done;

    int pkey_len = EVP_PKEY_size(key->evp_pkey);
    padded_sig = xcalloc(1, pkey_len);
    if (BN_bn2binpad(sig->bn, padded_sig, pkey_len) <= 0)
        goto done;

    if (EVP_PKEY_verify(pkey_ctx, padded_sig, pkey_len, hash, hashlen) == 1)
    {
        /* Success */
        rc = 0;
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
    EVP_PKEY *evp_pkey; /* Fully constructed key */
};

static int constructDSASigningKey(struct pgpDigKeyDSA_s *key)
{
    int rc;

    if (key->evp_pkey) {
        /* We've already constructed it, so just reuse it */
        return 1;
    }

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "DSA", NULL);
    OSSL_PARAM params[5] = {
	OSSL_PARAM_BN("pbits", key->p, BN_num_bytes(key->p)*8),
	OSSL_PARAM_BN("qbits", key->q, BN_num_bytes(key->q)*8),
	OSSL_PARAM_BN("gindex", key->g, BN_num_bytes(key->g)*8),
	OSSL_PARAM_utf8_string("digest", "SHA384", 0),
	OSSL_PARAM_END,
    };
    EVP_PKEY_CTX_set_params(ctx, params);
    EVP_PKEY_fromdata(ctx, &key->evp_pkey, EVP_PKEY_PUBLIC_KEY, params);
    rc = 1;
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
        if (key->evp_pkey) {
            EVP_PKEY_free(key->evp_pkey);
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
    int rc = 1; /* assume failure */
    EVP_PKEY_CTX *pkey_ctx = NULL;

    struct pgpDigSigDSA_s *sig = pgpsig->data;
    struct pgpDigKeyDSA_s *key = pgpkey->data;

    void *padded_sig = NULL;

    if (!constructDSASigningKey(key))
        goto done;

    if (!constructDSASignature(sig))
        goto done;

    pkey_ctx = EVP_PKEY_CTX_new(key->evp_pkey, NULL);
    if (!pkey_ctx)
        goto done;

    if (EVP_PKEY_verify_init(pkey_ctx) != 1)
        goto done;

    if (EVP_PKEY_CTX_set_signature_md(pkey_ctx, getEVPMD(hash_algo)) <= 0)
        goto done;

    int pkey_len = EVP_PKEY_size(key->evp_pkey);
    padded_sig = xcalloc(1, pkey_len);
    if (BN_bn2binpad(sig->r, padded_sig, pkey_len) <= 0)
        goto done;

    if (EVP_PKEY_verify(pkey_ctx, padded_sig, pkey_len, hash, hashlen) == 1)
    {
        /* Success */
        rc = 0;
    }

done:
    return rc;
}


/****************************** EDDSA ***************************************/

#ifdef EVP_PKEY_ED25519

struct pgpDigKeyEDDSA_s {
    EVP_PKEY *evp_pkey; /* Fully constructed key */
    unsigned char *q;	/* compressed point */
    int qlen;
};

static int constructEDDSASigningKey(struct pgpDigKeyEDDSA_s *key, int curve)
{
    if (key->evp_pkey)
	return 1;	/* We've already constructed it, so just reuse it */
    if (curve == PGPCURVE_ED25519)
	key->evp_pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, key->q, key->qlen);
    return key->evp_pkey ? 1 : 0;
}

static int pgpSetKeyMpiEDDSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    size_t mlen = pgpMpiLen(p) - 2;
    struct pgpDigKeyEDDSA_s *key = pgpkey->data;
    int rc = 1;

    if (!key)
	key = pgpkey->data = xcalloc(1, sizeof(*key));
    if (num == 0 && !key->q && mlen > 1 && p[2] == 0x40) {
	key->qlen = mlen - 1;
	key->q = xmalloc(key->qlen);
	memcpy(key->q, p + 3, key->qlen),
	rc = 0;
    }
    return rc;
}

static void pgpFreeKeyEDDSA(pgpDigAlg pgpkey)
{
    struct pgpDigKeyEDDSA_s *key = pgpkey->data;
    if (key) {
	if (key->q)
	    free(key->q);
	if (key->evp_pkey)
	    EVP_PKEY_free(key->evp_pkey);
	free(key);
    }
}

struct pgpDigSigEDDSA_s {
    unsigned char sig[32 + 32];
};

static int pgpSetSigMpiEDDSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    struct pgpDigSigEDDSA_s *sig = pgpsig->data;
    int mlen = pgpMpiLen(p) - 2;

    if (!sig)
	sig = pgpsig->data = xcalloc(1, sizeof(*sig));
    if (!mlen || mlen > 32 || (num != 0 && num != 1))
	return 1;
    memcpy(sig->sig + 32 * num + 32 - mlen, p + 2, mlen);
    return 0;
}

static void pgpFreeSigEDDSA(pgpDigAlg pgpsig)
{
    struct pgpDigSigEDDSA_s *sig = pgpsig->data;
    if (sig) {
	free(pgpsig->data);
    }
}

static int pgpVerifySigEDDSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
                           uint8_t *hash, size_t hashlen, int hash_algo)
{
    int rc = 1;		/* assume failure */
    struct pgpDigSigEDDSA_s *sig = pgpsig->data;
    struct pgpDigKeyEDDSA_s *key = pgpkey->data;
    EVP_MD_CTX *md_ctx = NULL;

    if (!constructEDDSASigningKey(key, pgpkey->curve))
	goto done;
    md_ctx = EVP_MD_CTX_new();
    if (EVP_DigestVerifyInit(md_ctx, NULL, EVP_md_null(), NULL, key->evp_pkey) != 1)
	goto done;
    if (EVP_DigestVerify(md_ctx, sig->sig, 64, hash, hashlen) == 1)
	rc = 0;		/* Success */
done:
    if (md_ctx)
	EVP_MD_CTX_free(md_ctx);
    return rc;
}

#endif


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
pgpDigAlg pgpPubkeyNew(int algo, int curve)
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
#ifdef EVP_PKEY_ED25519
    case PGPPUBKEYALGO_EDDSA:
	if (curve != PGPCURVE_ED25519) {
	    ka->setmpi = pgpSetMpiNULL;	/* unsupported curve */
	    ka->mpis = -1;
	    break;
	}
        ka->setmpi = pgpSetKeyMpiEDDSA;
        ka->free = pgpFreeKeyEDDSA;
        ka->mpis = 1;
        ka->curve = curve;
        break;
#endif
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
#ifdef EVP_PKEY_ED25519
    case PGPPUBKEYALGO_EDDSA:
        sa->setmpi = pgpSetSigMpiEDDSA;
        sa->free = pgpFreeSigEDDSA;
        sa->verify = pgpVerifySigEDDSA;
        sa->mpis = 2;
        break;
#endif
    default:
        sa->setmpi = pgpSetMpiNULL;
        sa->verify = pgpVerifyNULL;
        sa->mpis = -1;
        break;
    }
    return sa;
}
