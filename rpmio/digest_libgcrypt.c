#include "system.h"

#include <gcrypt.h>

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
    gcry_md_hd_t h;
};


/****************************  init   ************************************/

int rpmInitCrypto(void) {
    return 0;
}

int rpmFreeCrypto(void) {
    return 0;
}

/****************************  digest ************************************/

size_t rpmDigestLength(int hashalgo)
{
    switch (hashalgo) {
    case PGPHASHALGO_MD5:
	return 16;
    case PGPHASHALGO_SHA1:
	return 20;
    case PGPHASHALGO_SHA224:
	return 28;
    case PGPHASHALGO_SHA256:
	return 32;
    case PGPHASHALGO_SHA384:
	return 48;
    case PGPHASHALGO_SHA512:
	return 64;
    default:
	return 0;
    }
}

static int hashalgo2gcryalgo(int hashalgo)
{
    switch (hashalgo) {
    case PGPHASHALGO_MD5:
	return GCRY_MD_MD5;
    case PGPHASHALGO_SHA1:
	return GCRY_MD_SHA1;
    case PGPHASHALGO_SHA224:
	return GCRY_MD_SHA224;
    case PGPHASHALGO_SHA256:
	return GCRY_MD_SHA256;
    case PGPHASHALGO_SHA384:
	return GCRY_MD_SHA384;
    case PGPHASHALGO_SHA512:
	return GCRY_MD_SHA512;
    default:
	return 0;
    }
}

DIGEST_CTX rpmDigestInit(int hashalgo, rpmDigestFlags flags)
{
    gcry_md_hd_t h;
    DIGEST_CTX ctx;
    int gcryalgo = hashalgo2gcryalgo(hashalgo);

    if (!gcryalgo || gcry_md_open(&h, gcryalgo, 0) != 0)
	return NULL;

    ctx = xcalloc(1, sizeof(*ctx));
    ctx->flags = flags;
    ctx->algo = hashalgo;
    ctx->h = h;
    return ctx;
}

int rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
    if (ctx == NULL)
	return -1;
    gcry_md_write(ctx->h, data, len);
    return 0;
}

int rpmDigestFinal(DIGEST_CTX ctx, void ** datap, size_t *lenp, int asAscii)
{
    unsigned char *digest;
    int digestlen;
    if (ctx == NULL)
	return -1;
    digest = gcry_md_read(ctx->h, 0);
    digestlen = rpmDigestLength(ctx->algo);
    if (!asAscii) {
	if (lenp)
	    *lenp = digestlen;
	if (datap) {
	    *datap = xmalloc(digestlen);
	    memcpy(*datap, digest, digestlen);
	}
    } else {
	if (lenp)
	    *lenp = 2 * digestlen + 1;
	if (datap) {
	    *datap = pgpHexStr((const uint8_t *)digest, digestlen);
	}
    }
    gcry_md_close(ctx->h);
    free(ctx);
    return 0;
}

DIGEST_CTX rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx = NULL;
    if (octx) {
        gcry_md_hd_t h;
	if (gcry_md_copy(&h, octx->h))
	    return NULL;
	nctx = memcpy(xcalloc(1, sizeof(*nctx)), octx, sizeof(*nctx));
	nctx->h = h;
    }
    return nctx;
}


/****************************** RSA **************************************/

struct pgpDigSigRSA_s {
    gcry_mpi_t s;
};

struct pgpDigKeyRSA_s {
    gcry_mpi_t n;
    gcry_mpi_t e;
};

static int pgpSetSigMpiRSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    struct pgpDigSigRSA_s *sig = pgpsig->data;
    int mlen = pgpMpiLen(p);
    int rc = 1;

    if (!sig)
	sig = pgpsig->data = xcalloc(1, sizeof(*sig));

    switch (num) {
    case 0:
	if (!gcry_mpi_scan(&sig->s, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    }
    return rc;
}

static int pgpSetKeyMpiRSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    struct pgpDigKeyRSA_s *key = pgpkey->data;
    int mlen = pgpMpiLen(p);
    int rc = 1;

    if (!key)
	key = pgpkey->data = xcalloc(1, sizeof(*key));

    switch (num) {
    case 0:
	if (!gcry_mpi_scan(&key->n, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    case 1:
	if (!gcry_mpi_scan(&key->e, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    }
    return rc;
}

static int pgpVerifySigRSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig, uint8_t *hash, size_t hashlen, int hash_algo)
{
    struct pgpDigKeyRSA_s *key = pgpkey->data;
    struct pgpDigSigRSA_s *sig = pgpsig->data;
    gcry_sexp_t sexp_sig = NULL, sexp_data = NULL, sexp_pkey = NULL;
    const char *hash_algo_name;
    int rc = 1;

    if (!sig || !key)
	return rc;

    hash_algo_name = gcry_md_algo_name(hashalgo2gcryalgo(hash_algo));
    gcry_sexp_build(&sexp_sig, NULL, "(sig-val (rsa (s %M)))", sig->s);
    gcry_sexp_build(&sexp_data, NULL, "(data (flags pkcs1) (hash %s %b))", hash_algo_name, (int)hashlen, (const char *)hash);
    gcry_sexp_build(&sexp_pkey, NULL, "(public-key (rsa (n %M) (e %M)))", key->n, key->e);
    if (sexp_sig && sexp_data && sexp_pkey)
	rc = gcry_pk_verify(sexp_sig, sexp_data, sexp_pkey) == 0 ? 0 : 1;
    gcry_sexp_release(sexp_sig);
    gcry_sexp_release(sexp_data);
    gcry_sexp_release(sexp_pkey);
    return rc;
}

static void pgpFreeSigRSA(pgpDigAlg pgpsig)
{
    struct pgpDigSigRSA_s *sig = pgpsig->data;
    if (sig) {
        gcry_mpi_release(sig->s);
	pgpsig->data = _free(sig);
    }
}

static void pgpFreeKeyRSA(pgpDigAlg pgpkey)
{
    struct pgpDigKeyRSA_s *key = pgpkey->data;
    if (key) {
        gcry_mpi_release(key->n);
        gcry_mpi_release(key->e);
	pgpkey->data = _free(key);
    }
}


/****************************** DSA **************************************/

struct pgpDigSigDSA_s {
    gcry_mpi_t r;
    gcry_mpi_t s;
};

struct pgpDigKeyDSA_s {
    gcry_mpi_t p;
    gcry_mpi_t q;
    gcry_mpi_t g;
    gcry_mpi_t y;
};

static int pgpSetSigMpiDSA(pgpDigAlg pgpsig, int num, const uint8_t *p)
{
    struct pgpDigSigDSA_s *sig = pgpsig->data;
    int mlen = pgpMpiLen(p);
    int rc = 1;

    if (!sig)
	sig = pgpsig->data = xcalloc(1, sizeof(*sig));

    switch (num) {
    case 0:
	if (!gcry_mpi_scan(&sig->r, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    case 1:
	if (!gcry_mpi_scan(&sig->s, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    }
    return rc;
}

static int pgpSetKeyMpiDSA(pgpDigAlg pgpkey, int num, const uint8_t *p)
{
    struct pgpDigKeyDSA_s *key = pgpkey->data;
    int mlen = pgpMpiLen(p);
    int rc = 1;

    if (!key)
	key = pgpkey->data = xcalloc(1, sizeof(*key));

    switch (num) {
    case 0:
	if (!gcry_mpi_scan(&key->p, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    case 1:
	if (!gcry_mpi_scan(&key->q, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    case 2:
	if (!gcry_mpi_scan(&key->g, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    case 3:
	if (!gcry_mpi_scan(&key->y, GCRYMPI_FMT_PGP, p, mlen, NULL))
	    rc = 0;
	break;
    }
    return rc;
}

static int pgpVerifySigDSA(pgpDigAlg pgpkey, pgpDigAlg pgpsig, uint8_t *hash, size_t hashlen, int hash_algo)
{
    struct pgpDigKeyDSA_s *key = pgpkey->data;
    struct pgpDigSigDSA_s *sig = pgpsig->data;
    gcry_sexp_t sexp_sig = NULL, sexp_data = NULL, sexp_pkey = NULL;
    int rc = 1;

    if (!sig || !key)
	return rc;

    gcry_sexp_build(&sexp_sig, NULL, "(sig-val (dsa (r %M) (s %M)))", sig->r, sig->s);
    gcry_sexp_build(&sexp_data, NULL, "(data (flags raw) (value %b))", (int)hashlen, (const char *)hash);
    gcry_sexp_build(&sexp_pkey, NULL, "(public-key (dsa (p %M) (q %M) (g %M) (y %M)))", key->p, key->q, key->g, key->y);
    if (sexp_sig && sexp_data && sexp_pkey)
	rc = gcry_pk_verify(sexp_sig, sexp_data, sexp_pkey) == 0 ? 0 : 1;
    gcry_sexp_release(sexp_sig);
    gcry_sexp_release(sexp_data);
    gcry_sexp_release(sexp_pkey);
    return rc;
}

static void pgpFreeSigDSA(pgpDigAlg pgpsig)
{
    struct pgpDigSigDSA_s *sig = pgpsig->data;
    if (sig) {
        gcry_mpi_release(sig->r);
        gcry_mpi_release(sig->s);
	pgpsig->data = _free(sig);
    }
}

static void pgpFreeKeyDSA(pgpDigAlg pgpkey)
{
    struct pgpDigKeyDSA_s *key = pgpkey->data;
    if (key) {
        gcry_mpi_release(key->p);
        gcry_mpi_release(key->q);
        gcry_mpi_release(key->g);
        gcry_mpi_release(key->y);
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
