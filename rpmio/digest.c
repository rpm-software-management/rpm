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
};

DIGEST_CTX
rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx;
    nctx = memcpy(xcalloc(1, sizeof(*nctx)), octx, sizeof(*nctx));
    nctx->hashctx = HASH_Clone(octx->hashctx);
    if (nctx->hashctx == NULL) {
    	fprintf(stderr, "HASH_Clone failed\n");
    	exit(EXIT_FAILURE);  /* FIX: callers do not bother checking error return */
    }
    return nctx;
}

static HASH_HashType
getHashType(pgpHashAlgo hashalgo)
{
    switch (hashalgo) {
    case PGPHASHALGO_MD5:
	return HASH_AlgMD5;
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
    case PGPHASHALGO_MD2:
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
    HASH_HashType type;
    DIGEST_CTX ctx = xcalloc(1, sizeof(*ctx));

    ctx->flags = flags;

    type = getHashType(hashalgo);
    if (type == HASH_AlgNULL) {
	free(ctx);
	return NULL;
    }

    ctx->hashctx = HASH_Create(type);
    if (ctx->hashctx == NULL) {
    	free(ctx);
    	return NULL;
    }

    HASH_Begin(ctx->hashctx);
    
DPRINTF((stderr, "*** Init(%x) ctx %p hashctx %p\n", flags, ctx, ctx->hashctx));
    return ctx;
}

int
rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
    unsigned int partlen;
    const unsigned char *ptr = data;

    if (ctx == NULL)
	return -1;

DPRINTF((stderr, "*** Update(%p,%p,%d) hashctx %p \"%s\"\n", ctx, data, len, ctx->hashctx, ((char *)data)));
   partlen = ~(unsigned int)0xFF;
   while (len > 0) {
   	if (len < partlen) {
   		partlen = (unsigned int)len;
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
    char * t;
    int i;
    unsigned int digestlen;

    if (ctx == NULL)
	return -1;
    digestlen = HASH_ResultLenContext(ctx->hashctx);
    digest = xmalloc(digestlen);

DPRINTF((stderr, "*** Final(%p,%p,%p,%d) hashctx %p digest %p\n", ctx, datap, lenp, asAscii, ctx->hashctx, digest));
/* FIX: check rc */
    HASH_End(ctx->hashctx, digest, &digestlen, digestlen);

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
	    const byte * s = (const byte *) digest;
	    static const char hex[] = "0123456789abcdef";

	    *datap = t = xmalloc((2*digestlen) + 1);
	    for (i = 0 ; i < digestlen; i++) {
		*t++ = hex[ (unsigned)((*s >> 4) & 0x0f) ];
		*t++ = hex[ (unsigned)((*s++   ) & 0x0f) ];
	    }
	    *t = '\0';
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
