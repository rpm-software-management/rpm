/** \ingroup signature
 * \file rpmio/digest.c
 */

#include "system.h"

#include "rpmio/digest.h"

#include "debug.h"

#ifdef	SHA_DEBUG
#define	DPRINTF(_a)	fprintf _a
#else
#define	DPRINTF(_a)
#endif


/**
 * MD5/SHA1 digest private data.
 */
struct DIGEST_CTX_s {
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    HASHContext *hashctx;	/*!< Internal NSS hash context. */
    pgpHashAlgo algo;		/*!< Used hash algorithm */
};

#define DIGESTS_MAX 11
struct rpmDigestBundle_s {
    int index_min;			/*!< Smallest index of active digest */
    int index_max;			/*!< Largest index of active digest */
    off_t nbytes;			/*!< Length of total input data */
    DIGEST_CTX digests[DIGESTS_MAX];	/*!< Digest contexts indexed by algo */
};

rpmDigestBundle rpmDigestBundleNew(void)
{
    rpmDigestBundle bundle = xcalloc(1, sizeof(*bundle));
    return bundle;
}

rpmDigestBundle rpmDigestBundleFree(rpmDigestBundle bundle)
{
    if (bundle) {
	for (int i = bundle->index_min; i <= bundle->index_max ; i++) {
	    if (bundle->digests[i] == NULL)
		continue;
	    rpmDigestFinal(bundle->digests[i], NULL, NULL, 0);
	    bundle->digests[i] = NULL;
	}
	memset(bundle, 0, sizeof(*bundle));
	free(bundle);
    }
    return NULL;
}

int rpmDigestBundleAdd(rpmDigestBundle bundle, pgpHashAlgo algo,
			rpmDigestFlags flags)
{
    DIGEST_CTX ctx = NULL;
    if (bundle && algo > 0 && algo < DIGESTS_MAX) {
	if (bundle->digests[algo] == NULL) {
	    ctx = rpmDigestInit(algo, flags);
	    if (ctx) {
		bundle->digests[algo] = ctx;
		if (algo < bundle->index_min) {
		    bundle->index_min = algo;
		}
		if (algo > bundle->index_max) {
		    bundle->index_max = algo;
		}
	    }
	}
    }
    return (ctx != NULL);
}

int rpmDigestBundleUpdate(rpmDigestBundle bundle, const void *data, size_t len)
{
    int rc = 0;
    if (bundle && data && len > 0) {
	for (int i = bundle->index_min; i <= bundle->index_max; i++) {
	    DIGEST_CTX ctx = bundle->digests[i];
	    if (ctx == NULL)
		continue;
	    rc += rpmDigestUpdate(ctx, data, len);
	}
	bundle->nbytes += len;
    }
    return rc;
}

int rpmDigestBundleFinal(rpmDigestBundle bundle, 
		pgpHashAlgo algo, void ** datap, size_t * lenp, int asAscii)
{
    int rc = 0;
    if (bundle && algo >= bundle->index_min && algo <= bundle->index_max) {
	rc = rpmDigestFinal(bundle->digests[algo], datap, lenp, asAscii);
	bundle->digests[algo] = NULL;
    }
    return rc;
}

DIGEST_CTX rpmDigestBundleDupCtx(rpmDigestBundle bundle, pgpHashAlgo algo)
{
    DIGEST_CTX dup = NULL;
    if (bundle && algo >= bundle->index_min && algo <= bundle->index_max) {
	dup = rpmDigestDup(bundle->digests[algo]);
    }
    return dup;
}

DIGEST_CTX
rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx = NULL;
    if (octx) {
	HASHContext *hctx = HASH_Clone(octx->hashctx);
	if (hctx) {
	    nctx = memcpy(xcalloc(1, sizeof(*nctx)), octx, sizeof(*nctx));
	    nctx->hashctx = hctx;
	}
    }
    return nctx;
}

RPM_GNUC_PURE
static HASH_HashType getHashType(pgpHashAlgo hashalgo)
{
    switch (hashalgo) {
    case PGPHASHALGO_MD5:
	return HASH_AlgMD5;
	break;
    case PGPHASHALGO_MD2:
	return HASH_AlgMD2;
	break;
    case PGPHASHALGO_SHA1:
	return HASH_AlgSHA1;
	break;
    case PGPHASHALGO_SHA256:
	return HASH_AlgSHA256;
	break;
    case PGPHASHALGO_SHA384:
	return HASH_AlgSHA384;
	break;
    case PGPHASHALGO_SHA512:
	return HASH_AlgSHA512;
	break;
    case PGPHASHALGO_RIPEMD160:
    case PGPHASHALGO_TIGER192:
    case PGPHASHALGO_HAVAL_5_160:
    default:
	return HASH_AlgNULL;
	break;
    }
}

size_t
rpmDigestLength(pgpHashAlgo hashalgo)
{
    return HASH_ResultLen(getHashType(hashalgo));
}

DIGEST_CTX
rpmDigestInit(pgpHashAlgo hashalgo, rpmDigestFlags flags)
{
    HASH_HashType type = getHashType(hashalgo);
    HASHContext *hashctx = NULL;
    DIGEST_CTX ctx = NULL;

    if (type == HASH_AlgNULL || rpmInitCrypto() < 0)
	goto exit;

    if ((hashctx = HASH_Create(type)) != NULL) {
	ctx = xcalloc(1, sizeof(*ctx));
	ctx->flags = flags;
	ctx->algo = hashalgo;
	ctx->hashctx = hashctx;
    	HASH_Begin(ctx->hashctx);
    }
    
DPRINTF((stderr, "*** Init(%x) ctx %p hashctx %p\n", flags, ctx, ctx->hashctx));
exit:
    return ctx;
}

int
rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
    size_t partlen;
    const unsigned char *ptr = data;

    if (ctx == NULL)
	return -1;

DPRINTF((stderr, "*** Update(%p,%p,%zd) hashctx %p \"%s\"\n", ctx, data, len, ctx->hashctx, ((char *)data)));
   partlen = ~(unsigned int)0xFF;
   while (len > 0) {
   	if (len < partlen) {
   		partlen = len;
   	}
	HASH_Update(ctx->hashctx, ptr, partlen);
	ptr += partlen;
	len -= partlen;
   }
   return 0;
}

int
rpmDigestFinal(DIGEST_CTX ctx, void ** datap, size_t *lenp, int asAscii)
{
    unsigned char * digest;
    unsigned int digestlen;

    if (ctx == NULL)
	return -1;
    digestlen = HASH_ResultLenContext(ctx->hashctx);
    digest = xmalloc(digestlen);

DPRINTF((stderr, "*** Final(%p,%p,%p,%zd) hashctx %p digest %p\n", ctx, datap, lenp, asAscii, ctx->hashctx, digest));
/* FIX: check rc */
    HASH_End(ctx->hashctx, digest, (unsigned int *) &digestlen, digestlen);

    /* Return final digest. */
    if (!asAscii) {
	if (lenp) *lenp = digestlen;
	if (datap) {
	    *datap = digest;
	    digest = NULL;
	}
    } else {
	if (lenp) *lenp = (2*digestlen) + 1;
	if (datap) {
	    const uint8_t * s = (const uint8_t *) digest;
	    *datap = pgpHexStr(s, digestlen);
	}
    }
    if (digest) {
	memset(digest, 0, digestlen);	/* In case it's sensitive */
	free(digest);
    }
    HASH_Destroy(ctx->hashctx);
    memset(ctx, 0, sizeof(*ctx));	/* In case it's sensitive */
    free(ctx);
    return 0;
}

