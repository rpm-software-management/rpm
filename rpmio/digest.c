/** \ingroup signature
 * \file rpmio/digest.c
 */

#include "system.h"
#include "rpmio_internal.h"
#include "beecrypt.h"
#include "md5.h"
#include "endianness.h"
#include "fips180.h"
#include "debug.h"

#ifdef	SHA_DEBUG
#define	DPRINTF(_a)	fprintf _a
#else
#define	DPRINTF(_a)
#endif

/*@access DIGEST_CTX@*/

/**
 * MD5/SHA1 digest private data.
 */
struct DIGEST_CTX_s {
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    uint32 digestlen;		/*!< No. bytes of digest. */
    uint32 datalen;		/*!< No. bytes in block of plaintext data. */
    void * param;		/*!< Digest parameters. */
    int (*Reset) (void * param)
	/*@modifies param @*/;	/*!< Digest initialize. */
    int (*Update) (void * param, const byte * data, int len)
	/*@modifies param @*/;	/*!< Digest transform. */
    int (*Digest) (void * param, /*@out@*/ uint32 * digest)
	/*@modifies param, digest @*/;	/*!< Digest finish. */
};

DIGEST_CTX
rpmDigestDup(DIGEST_CTX octx)
{
    DIGEST_CTX nctx = xcalloc(1, sizeof(*nctx));
    memcpy(nctx, octx, sizeof(*nctx));
    return nctx;
}

DIGEST_CTX
rpmDigestInit(rpmDigestFlags flags)
{
    DIGEST_CTX ctx = xcalloc(1, sizeof(*ctx));

    ctx->flags = flags;

    if (flags & RPMDIGEST_MD5) {
	ctx->digestlen = 16;
	ctx->datalen = 64;
	/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->param = xcalloc(1, sizeof(md5Param));
	/*@=sizeoftype@*/
	/*@-type@*/ /* FIX: cast? */
	ctx->Reset = (void *) md5Reset;
	ctx->Update = (void *) md5Update;
	ctx->Digest = (void *) md5Digest;
	/*@=type@*/
    }

    if (flags & RPMDIGEST_SHA1) {
	ctx->digestlen = 20;
	ctx->datalen = 64;
	/*@-sizeoftype@*/ /* FIX: union, not void pointer */
	ctx->param = xcalloc(1, sizeof(sha1Param));
	/*@=sizeoftype@*/
	/*@-type@*/ /* FIX: cast? */
	ctx->Reset = (void *) sha1Reset;
	ctx->Update = (void *) sha1Update;
	ctx->Digest = (void *) sha1Digest;
	/*@=type@*/
    }

    /*@-noeffectuncon@*/ /* FIX: check rc */
    (void) (*ctx->Reset) (ctx->param);
    /*@=noeffectuncon@*/ /* FIX: check rc */

DPRINTF((stderr, "*** Init(%x) ctx %p param %p\n", flags, ctx, ctx->param));
    return ctx;
}

void
rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
DPRINTF((stderr, "*** Update(%p,%p,%d) param %p \"%s\"\n", ctx, data, len, ctx->param, ((char *)data)));
    /*@-noeffectuncon@*/ /* FIX: check rc */
    (void) (*ctx->Update) (ctx->param, data, len);
    /*@=noeffectuncon@*/
}

/*@unchecked@*/
static int _ie = 0x44332211;
/*@-redef@*/
/*@unchecked@*/
static union _dendian {
/*@unused@*/ int i;
    char b[4];
} *_endian = (union _dendian *)&_ie;
/*@=redef@*/
#define        IS_BIG_ENDIAN()         (_endian->b[0] == '\x44')
#define        IS_LITTLE_ENDIAN()      (_endian->b[0] == '\x11')

void
rpmDigestFinal(/*@only@*/ DIGEST_CTX ctx, /*@out@*/ void ** datap,
	/*@out@*/ size_t *lenp, int asAscii)
{
    uint32 * digest = xmalloc(ctx->digestlen);
    char * t;
    int i;

DPRINTF((stderr, "*** Final(%p,%p,%p,%d) param %p digest %p\n", ctx, datap, lenp, asAscii, ctx->param, digest));
    /*@-noeffectuncon@*/ /* FIX: check rc */
    (void) (*ctx->Digest) (ctx->param, digest);
    /*@=noeffectuncon@*/

    /*@-sizeoftype@*/
    if (IS_LITTLE_ENDIAN())
    for (i = 0; i < (ctx->digestlen/sizeof(uint32)); i++)
	digest[i] = swapu32(digest[i]);
    /*@=sizeoftype@*/

    /* Return final digest. */
    /*@-branchstate@*/
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
    /*@=branchstate@*/
    if (digest) {
	memset(digest, 0, ctx->digestlen);	/* In case it's sensitive */
	free(digest);
    }
    free(ctx->param);
    memset(ctx, 0, sizeof(*ctx));	/* In case it's sensitive */
    free(ctx);
}
