#ifndef MD5_H
#define MD5_H

/**
 * \file lib/md5.h
 * @todo Eliminate, use rpmio version instead.
 */

#include <sys/types.h>

typedef unsigned int uint32;

/**
 * MD5 private data.
 */
struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];
	unsigned char in[64];
	int doByteReverse;
};

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
/*@-mutrep@*/	/* FIX: redefine as pointer */
typedef /*@abstract@*/ struct MD5Context MD5_CTX;
/*@=mutrep@*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize MD5 hash.
 * Set bit count to 0 and buffer to mysterious initialization constants.
 * @param ctx	MD5 private data
 * @param brokenEndian	calculate broken MD5 sum?
 */
void rpmMD5Init( /*@out@*/ struct MD5Context * ctx, int brokenEndian)
	/*@modifies *ctx @*/;

/**
 * Update context to reflect the concatenation of another buffer full.
 * of bytes.
 * @param ctx		MD5 private data
 * @param data		next data buffer
 * @param len		no. bytes of data
 */
void rpmMD5Update(struct MD5Context * ctx, unsigned char const *buf,
	       unsigned len)
	/*@modifies ctx @*/;
/**
 * Return MD5 digest, and reset context.
 * @retval		MD5 digest
 * @param ctx		MD5 private data
 */
/*@-fixedformalarray@*/
void rpmMD5Final(unsigned char digest[16], struct MD5Context * ctx)
	/*@modifies digest, ctx @*/;
/*@=fixedformalarray@*/

/**
 * The core of the MD5 algorithm.
 * This alters an existing MD5 hash to reflect the addition of 16 longwords
 * of new data.
 * @param buf		current MD5 variables
 * @param in		next block of data to add
 */
/*@-fixedformalarray -exportlocal@*/
void rpmMD5Transform(uint32 buf[4], uint32 const in[16])
	/*@modifies *buf @*/;
/*@=fixedformalarray =exportlocal@*/

/**
 * Return MD5 sum of file as ASCII string.
 * @param fn		file name
 * @retval digest	MD5 digest
 * @return		0 on success, 1 on error
 */
int mdfile(const char * fn, /*@out@*/ unsigned char * digest)
	/*@modifies digest @*/;

/**
 * Return MD5 sum of file as binary data.
 * @param fn		file name
 * @retval bindigest	MD5 digest
 * @return		0 on success, 1 on error
 */
int mdbinfile(const char * fn, /*@out@*/ unsigned char * bindigest)
	/*@modifies *bindigest @*/;

/* These assume a little endian machine and return incorrect results!
   They are here for compatibility with old (broken) versions of RPM */

/**
 * Return (broken!) MD5 sum of file as ASCII string.
 * @deprecated Here for compatibility with old (broken) versions of RPM.
 * @param fn		file name
 * @retval digest	MD5 digest
 * @return		0 on success, 1 on error
 */
int mdfileBroken(const char * fn, /*@out@*/ unsigned char * digest)
	/*@modifies *digest @*/;

/**
 * Return (broken!) MD5 sum of file as binary data.
 * @deprecated Here for compatibility with old (broken) versions of RPM.
 * @param fn		file name
 * @retval bindigest	MD5 digest
 * @return		0 on success, 1 on error
 */
int mdbinfileBroken(const char * fn, /*@out@*/ unsigned char * bindigest)
	/*@modifies *bindigest @*/;

#ifdef __cplusplus
}
#endif

#endif	/* MD5_H */
