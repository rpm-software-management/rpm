/** \ingroup signature
 * \file rpmio/digest.c
 */

#include "system.h"
#include "rpmio_internal.h"
#include "debug.h"

/*@-redef@*/
typedef unsigned int uint32;
typedef unsigned char byte;
/*@=redef@*/

/*@access DIGEST_CTX@*/

/**
 * MD5/SHA1 digest private data.
 */
struct DIGEST_CTX_s {
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    uint32 digestlen;		/*!< No. bytes of digest. */
    uint32 datalen;		/*!< No. bytes in block of plaintext data. */
    void (*transform) (DIGEST_CTX); /*!< Digest transform. */
    int doByteReverse;		/*!< Swap bytes in uint32? */
    uint32 bits[2];		/*!< No. bits of plain text. */
    uint32 digest[8];		/*!< Message digest. */
    byte in[64];		/*!< Next block of plain text. */
};

/*
 * This code implements SHA-1 as defined in FIPS publication 180-1.
 * Based on SHA code originally posted to sci.crypt by Peter Gutmann
 * in message <30ajo5$oe8@ccu2.auckland.ac.nz>.
 * Modified to test for endianness on creation of SHA objects by AMK.
 * Also, the original specification of SHA was found to have a weakness
 * by NSA/NIST.  This code implements the fixed version of SHA.
 *
 * Here's the first paragraph of Peter Gutmann's original posting:
 * - The following is my SHA (FIPS 180) code updated to allow use of the "fixed"
 * SHA, thanks to Jim Gillogly and an anonymous contributor for the information
 * on what's changed in the new version.  The fix is a simple change which
 * involves adding a single rotate in the initial expansion function. It is
 * unknown whether this is an optimal solution to the problem which was
 * discovered in the SHA or whether it's simply a bandaid which fixes the
 * problem with a minimum of effort (for example the reengineering of a great
 * many Capstone chips).
 *
 * Copyright (C) 1995, A.M. Kuchling
 *
 * Distribute and use freely; there are no restrictions on further 
 * dissemination and usage except those imposed by the laws of your 
 * country of residence.
 *
 * Adapted to pike and some cleanup by Niels Möller.
 * Adapted for rpm use from mhash-0.8.3.
 */

/**
 * The SHA f()-functions.  The f1 and f3 functions can be optimized to
 * save one boolean operation each - thanks to Rich Schroeppel,
 * rcs@cs.arizona.edu for discovering this.
 */
/*#define f1(x,y,z) ( ( x & y ) | ( ~x & z ) )		// Rounds  0-19 */
#define f1(x,y,z)   ( z ^ ( x & ( y ^ z ) ) )		/* Rounds  0-19 */
#define f2(x,y,z)   ( x ^ y ^ z )			/* Rounds 20-39 */
/*#define f3(x,y,z) ( ( x & y ) | ( x & z ) | ( y & z ) ) // Rounds 40-59 */
#define f3(x,y,z)   ( ( x & y ) | ( z & ( x | y ) ) )	/* Rounds 40-59 */
#define f4(x,y,z)   ( x ^ y ^ z )			/* Rounds 60-79 */

/**
 * The SHA Mysterious Constants.
 */
#define K1  0x5A827999L					/* Rounds  0-19 */
#define K2  0x6ED9EBA1L					/* Rounds 20-39 */
#define K3  0x8F1BBCDCL					/* Rounds 40-59 */
#define K4  0xCA62C1D6L					/* Rounds 60-79 */

/**
 * 32-bit rotate left - kludged with shifts.
 */
#define ROTL(n,X)  ( ( (X) << (n) ) | ( (X) >> ( 32 - (n) ) ) )

/**
 * The initial expanding function.  The hash function is defined over an
 * 80-word expanded input array W, where the first 16 are copies of the input
 * data, and the remaining 64 are defined by
 *
 * \verbatim
 *      W[ i ] = W[ i - 16 ] ^ W[ i - 14 ] ^ W[ i - 8 ] ^ W[ i - 3 ]
 * \endverbatim
 *
 * This implementation generates these values on the fly in a circular
 * buffer - thanks to Colin Plumb, colin@nyx10.cs.du.edu for this
 * optimization.
 *
 * The updated SHA changes the expanding function by adding a rotate of 1
 * bit.  Thanks to Jim Gillogly, jim@rand.org, and an anonymous contributor
 * for this information
 */
#define expand(W,i) ( W[ i & 15 ] = \
		      ROTL( 1, ( W[ i & 15 ] ^ W[ (i - 14) & 15 ] ^ \
				 W[ (i - 8) & 15 ] ^ W[ (i - 3) & 15 ] ) ) )

/**
 * The prototype SHA sub-round.
 * The fundamental sub-round is:
 *
 * \verbatim
 *      a' = e + ROTL( 5, a ) + f( b, c, d ) + k + data;
 *      b' = a;
 *      c' = ROTL( 30, b );
 *      d' = c;
 *      e' = d;
 * \endverbatim
 *
 * but this is implemented by unrolling the loop 5 times and renaming the
 * variables ( e, a, b, c, d ) = ( a', b', c', d', e' ) each iteration.
 * This code is then replicated 20 times for each of the 4 functions, using
 * the next 20 values from the W[] array each time.
 */

#define subRound(a, b, c, d, e, f, k, data) \
    ( e += ROTL( 5, a ) + f( b, c, d ) + k + data, b = ROTL( 30, b ) )

/**
 * Perform the SHA transformation.  Note that this code, like MD5, seems to
 * break some optimizing compilers due to the complexity of the expressions
 * and the size of the basic block.  It may be necessary to split it into
 * sections, e.g. based on the four subrounds
 *
 * Note that this function destroys the data area.
 */

/**
 * The core of the SHA algorithm.
 * This alters an existing SHA hash to reflect the addition of 16 longwords
 * of new data.
 * @param private	SHA private data
 */
static void
SHA1Transform(DIGEST_CTX ctx)
{
    uint32 * in = (uint32 *) ctx->in;
    uint32 A, B, C, D, E;     /* Local vars */

    /* Set up first buffer and local data buffer */
    A = ctx->digest[0];
    B = ctx->digest[1];
    C = ctx->digest[2];
    D = ctx->digest[3];
    E = ctx->digest[4];

    /* Heavy mangling, in 4 sub-rounds of 20 interations each. */
    subRound( A, B, C, D, E, f1, K1, in[ 0] );
    subRound( E, A, B, C, D, f1, K1, in[ 1] );
    subRound( D, E, A, B, C, f1, K1, in[ 2] );
    subRound( C, D, E, A, B, f1, K1, in[ 3] );
    subRound( B, C, D, E, A, f1, K1, in[ 4] );
    subRound( A, B, C, D, E, f1, K1, in[ 5] );
    subRound( E, A, B, C, D, f1, K1, in[ 6] );
    subRound( D, E, A, B, C, f1, K1, in[ 7] );
    subRound( C, D, E, A, B, f1, K1, in[ 8] );
    subRound( B, C, D, E, A, f1, K1, in[ 9] );
    subRound( A, B, C, D, E, f1, K1, in[10] );
    subRound( E, A, B, C, D, f1, K1, in[11] );
    subRound( D, E, A, B, C, f1, K1, in[12] );
    subRound( C, D, E, A, B, f1, K1, in[13] );
    subRound( B, C, D, E, A, f1, K1, in[14] );
    subRound( A, B, C, D, E, f1, K1, in[15] );
    subRound( E, A, B, C, D, f1, K1, expand( in, 16 ) );
    subRound( D, E, A, B, C, f1, K1, expand( in, 17 ) );
    subRound( C, D, E, A, B, f1, K1, expand( in, 18 ) );
    subRound( B, C, D, E, A, f1, K1, expand( in, 19 ) );

    subRound( A, B, C, D, E, f2, K2, expand( in, 20 ) );
    subRound( E, A, B, C, D, f2, K2, expand( in, 21 ) );
    subRound( D, E, A, B, C, f2, K2, expand( in, 22 ) );
    subRound( C, D, E, A, B, f2, K2, expand( in, 23 ) );
    subRound( B, C, D, E, A, f2, K2, expand( in, 24 ) );
    subRound( A, B, C, D, E, f2, K2, expand( in, 25 ) );
    subRound( E, A, B, C, D, f2, K2, expand( in, 26 ) );
    subRound( D, E, A, B, C, f2, K2, expand( in, 27 ) );
    subRound( C, D, E, A, B, f2, K2, expand( in, 28 ) );
    subRound( B, C, D, E, A, f2, K2, expand( in, 29 ) );
    subRound( A, B, C, D, E, f2, K2, expand( in, 30 ) );
    subRound( E, A, B, C, D, f2, K2, expand( in, 31 ) );
    subRound( D, E, A, B, C, f2, K2, expand( in, 32 ) );
    subRound( C, D, E, A, B, f2, K2, expand( in, 33 ) );
    subRound( B, C, D, E, A, f2, K2, expand( in, 34 ) );
    subRound( A, B, C, D, E, f2, K2, expand( in, 35 ) );
    subRound( E, A, B, C, D, f2, K2, expand( in, 36 ) );
    subRound( D, E, A, B, C, f2, K2, expand( in, 37 ) );
    subRound( C, D, E, A, B, f2, K2, expand( in, 38 ) );
    subRound( B, C, D, E, A, f2, K2, expand( in, 39 ) );

    subRound( A, B, C, D, E, f3, K3, expand( in, 40 ) );
    subRound( E, A, B, C, D, f3, K3, expand( in, 41 ) );
    subRound( D, E, A, B, C, f3, K3, expand( in, 42 ) );
    subRound( C, D, E, A, B, f3, K3, expand( in, 43 ) );
    subRound( B, C, D, E, A, f3, K3, expand( in, 44 ) );
    subRound( A, B, C, D, E, f3, K3, expand( in, 45 ) );
    subRound( E, A, B, C, D, f3, K3, expand( in, 46 ) );
    subRound( D, E, A, B, C, f3, K3, expand( in, 47 ) );
    subRound( C, D, E, A, B, f3, K3, expand( in, 48 ) );
    subRound( B, C, D, E, A, f3, K3, expand( in, 49 ) );
    subRound( A, B, C, D, E, f3, K3, expand( in, 50 ) );
    subRound( E, A, B, C, D, f3, K3, expand( in, 51 ) );
    subRound( D, E, A, B, C, f3, K3, expand( in, 52 ) );
    subRound( C, D, E, A, B, f3, K3, expand( in, 53 ) );
    subRound( B, C, D, E, A, f3, K3, expand( in, 54 ) );
    subRound( A, B, C, D, E, f3, K3, expand( in, 55 ) );
    subRound( E, A, B, C, D, f3, K3, expand( in, 56 ) );
    subRound( D, E, A, B, C, f3, K3, expand( in, 57 ) );
    subRound( C, D, E, A, B, f3, K3, expand( in, 58 ) );
    subRound( B, C, D, E, A, f3, K3, expand( in, 59 ) );

    subRound( A, B, C, D, E, f4, K4, expand( in, 60 ) );
    subRound( E, A, B, C, D, f4, K4, expand( in, 61 ) );
    subRound( D, E, A, B, C, f4, K4, expand( in, 62 ) );
    subRound( C, D, E, A, B, f4, K4, expand( in, 63 ) );
    subRound( B, C, D, E, A, f4, K4, expand( in, 64 ) );
    subRound( A, B, C, D, E, f4, K4, expand( in, 65 ) );
    subRound( E, A, B, C, D, f4, K4, expand( in, 66 ) );
    subRound( D, E, A, B, C, f4, K4, expand( in, 67 ) );
    subRound( C, D, E, A, B, f4, K4, expand( in, 68 ) );
    subRound( B, C, D, E, A, f4, K4, expand( in, 69 ) );
    subRound( A, B, C, D, E, f4, K4, expand( in, 70 ) );
    subRound( E, A, B, C, D, f4, K4, expand( in, 71 ) );
    subRound( D, E, A, B, C, f4, K4, expand( in, 72 ) );
    subRound( C, D, E, A, B, f4, K4, expand( in, 73 ) );
    subRound( B, C, D, E, A, f4, K4, expand( in, 74 ) );
    subRound( A, B, C, D, E, f4, K4, expand( in, 75 ) );
    subRound( E, A, B, C, D, f4, K4, expand( in, 76 ) );
    subRound( D, E, A, B, C, f4, K4, expand( in, 77 ) );
    subRound( C, D, E, A, B, f4, K4, expand( in, 78 ) );
    subRound( B, C, D, E, A, f4, K4, expand( in, 79 ) );

    /* Build message digest */
    ctx->digest[0] += A;
    ctx->digest[1] += B;
    ctx->digest[2] += C;
    ctx->digest[3] += D;
    ctx->digest[4] += E;
}

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

/** The four core functions used in MD5 - F1 is optimized somewhat. */
/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/** The central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/**
 * The core of the MD5 algorithm.
 * Update MD5 context with next 64 bytes of plain text.
 * @param private	MD5 private data
 */
static void
MD5Transform(DIGEST_CTX ctx)
{
    register uint32 * in = (uint32 *)ctx->in;
    register uint32 a = ctx->digest[0];
    register uint32 b = ctx->digest[1];
    register uint32 c = ctx->digest[2];
    register uint32 d = ctx->digest[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    ctx->digest[0] += a;
    ctx->digest[1] += b;
    ctx->digest[2] += c;
    ctx->digest[3] += d;
}

static int _ie = 0x44332211;
/*@-redef@*/
static union _mendian { int i; char b[4]; } *_endian = (union _mendian *)&_ie;
/*@=redef@*/
#define	IS_BIG_ENDIAN()		(_endian->b[0] == '\x44')
#define	IS_LITTLE_ENDIAN()	(_endian->b[0] == '\x11')

/*
 * Reverse bytes for each integer in buffer.
 * @param buf		data buffer (uint32 aligned address)
 * @param nbytes	no. bytes of data (multiple of sizeof(uint32))
 */
/*@-shadow@*/
static void
byteReverse(byte *buf, unsigned nbytes)
{
    unsigned nlongs = nbytes / sizeof(uint32);
    uint32 t;
    do {
	t = (uint32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
	    ((unsigned) buf[1] << 8 | buf[0]);
	*(uint32 *) buf = t;
	buf += 4;
    } while (--nlongs);
}
/*@=shadow@*/

DIGEST_CTX
rpmDigestInit(rpmDigestFlags flags)
{
    DIGEST_CTX ctx = xcalloc(1, sizeof(*ctx));

    ctx->flags = flags;

    if (flags & RPMDIGEST_MD5) {
	ctx->digestlen = 16;
	ctx->datalen = 64;
	ctx->transform = MD5Transform;
	ctx->digest[0] = 0x67452301;
	ctx->digest[1] = 0xefcdab89;
	ctx->digest[2] = 0x98badcfe;
	ctx->digest[3] = 0x10325476;
    }

    if (flags & RPMDIGEST_SHA1) {
	ctx->digestlen = 20;
	ctx->datalen = 64;
	ctx->transform = SHA1Transform;
	ctx->digest[ 0 ] = 0x67452301;
	ctx->digest[ 1 ] = 0xefcdab89;
	ctx->digest[ 2 ] = 0x98badcfe;
	ctx->digest[ 3 ] = 0x10325476;
	ctx->digest[ 4 ] = 0xc3d2e1f0;
    }

    /* md5 sums are little endian (no swap) so big endian needs the swap. */
    ctx->doByteReverse = (IS_BIG_ENDIAN()) ? 1 : 0;
    if (flags & RPMDIGEST_NATIVE)
	ctx->doByteReverse = 0;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;

    return ctx;
}

void
rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
{
    const byte * buf = data;
    uint32 t;

    /* Update bitcount */

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((uint32) len << 3)) < t)
	ctx->bits[1]++;		/* Carry from low to high */
    ctx->bits[1] += len >> 29;

    t = (t >> 3) % ctx->datalen;	/* Bytes already in ctx->in */

    /* Handle any leading odd-sized chunks */
    if (t) {
	byte *p = (byte *) ctx->in + t;

	t = ctx->datalen - t;	/* Bytes left in ctx->in */
	if (len < t) {
	    memcpy(p, buf, len);
	    return;
	}
	memcpy(p, buf, t);
	if (ctx->doByteReverse)
	    byteReverse(ctx->in, ctx->datalen);
	ctx->transform(ctx);
	buf += t;
	len -= t;
    }

    /* Process data in ctx->datalen chunks */
    for (; len >= ctx->datalen; buf += ctx->datalen, len -= ctx->datalen) {
	/*@-mayaliasunique@*/
	memcpy(ctx->in, buf, ctx->datalen);
	/*@=mayaliasunique@*/
	if (ctx->doByteReverse)
	    byteReverse(ctx->in, ctx->datalen);
	ctx->transform(ctx);
    }

    /* Handle any remaining bytes of data. */
    /*@-mayaliasunique@*/
    memcpy(ctx->in, buf, len);
    /*@=mayaliasunique@*/
}

void
rpmDigestFinal(/*@only@*/ DIGEST_CTX ctx, /*@out@*/ void ** datap,
	/*@out@*/ size_t *lenp, int asAscii)
{
    unsigned count = (ctx->bits[0] >> 3) % ctx->datalen;
    byte * p = ctx->in + count;

    /* Set the first char of padding to 0x80.  This is safe since there is
       always at least one byte free */
    *p++ = 0x80;

    /* No. bytes of padding needed to fill buffer. */
    count = ctx->datalen - 1 - count;

    /* Insure that next block has room for no. of plaintext bits. */
    if (count < sizeof(ctx->bits)) {
	memset(p, 0, count);
	if (ctx->doByteReverse)
	    byteReverse(ctx->in, ctx->datalen);
	ctx->transform(ctx);
	p = ctx->in;
	count = ctx->datalen;
    }

    /* Pad next block with zeroes, add no. of plaintext bits. */
    memset(p, 0, count - sizeof(ctx->bits));
    if (ctx->doByteReverse)
	byteReverse(ctx->in, ctx->datalen - sizeof(ctx->bits));
    ((uint32 *) ctx->in)[14] = ctx->bits[0];
    ((uint32 *) ctx->in)[15] = ctx->bits[1];
    ctx->transform(ctx);

    /* Return final digest. */
    if (ctx->doByteReverse)
	byteReverse((byte *) ctx->digest, ctx->digestlen);

    if (!asAscii) {
	if (lenp) *lenp = ctx->digestlen;
	if (datap) {
	    *datap = xmalloc(ctx->digestlen);
	    memcpy(*datap, ctx->digest, ctx->digestlen);
	}
    } else {
	if (lenp) *lenp = (2*ctx->digestlen) + 1;
	if (datap) {
	    const byte * s = (const byte *) ctx->digest;
	    static const char hex[] = "0123456789abcdef";
	    char * t;
	    int i;

	    *datap = t = xmalloc((2*ctx->digestlen) + 1);

	    for (i = 0 ; i < ctx->digestlen; i++) {
		*t++ = hex[ (unsigned)((*s >> 4) & 0x0f) ];
		*t++ = hex[ (unsigned)((*s++   ) & 0x0f) ];
	    }
	    *t = '\0';
	}
    }
    memset(ctx, 0, sizeof(*ctx));	/* In case it's sensitive */
    free(ctx);
}
