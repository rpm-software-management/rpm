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
typedef /*@abstract@*/ struct MD5Context MD5_CTX;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize MD5 hash.
 * Set bit count to 0 and buffer to mysterious initialization constants.
 * @param context	MD5 private data
 * @param brokenEndian	calculate broken MD5 sum?
 */
void rpmMD5Init( /*@out@*/ struct MD5Context *context, int brokenEndian);

/**
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 * @param context	MD5 private data
 * @param data		next data buffer
 * @param len		no. bytes of data
 */
void rpmMD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
/**
 * Return MD5 digest, and reset context.
 * @retval		MD5 digest
 * @param context	MD5 private data
 */
/*@-fixedformalarray@*/
void rpmMD5Final(unsigned char digest[16], struct MD5Context *context);
/*@=fixedformalarray@*/

/**
 * The core of the MD5 algorithm.
 * This alters an existing MD5 hash to reflect the addition of 16 longwords
 * of new data.
 * @param buf		current MD5 variables
 * @param in		next block of data to add
 */
/*@-fixedformalarray@*/
void rpmMD5Transform(uint32 buf[4], uint32 const in[16]);
/*@=fixedformalarray@*/

/**
 * Return MD5 sum of file as ASCII string.
 * @param fn		file name
 * @retval digest	MD5 digest
 * @return		0 on success, 1 on error
 */
int mdfile(const char *fn, unsigned char *digest);

/**
 * Return MD5 sum of file as binary data.
 * @param fn		file name
 * @retval bindigest	MD5 digest
 * @return		0 on success, 1 on error
 */
int mdbinfile(const char *fn, unsigned char *bindigest);

/* These assume a little endian machine and return incorrect results!
   They are here for compatibility with old (broken) versions of RPM */

/**
 * Return (broken!) MD5 sum of file as ASCII string.
 * @deprecated Here for compatibility with old (broken) versions of RPM.
 * @param fn		file name
 * @retval digest	MD5 digest
 * @return		0 on success, 1 on error
 */
int mdfileBroken(const char *fn, unsigned char *digest);

/**
 * Return (broken!) MD5 sum of file as binary data.
 * @deprecated Here for compatibility with old (broken) versions of RPM.
 * @param fn		file name
 * @retval bindigest	MD5 digest
 * @return		0 on success, 1 on error
 */
int mdbinfileBroken(const char *fn, unsigned char *bindigest);

#ifdef __cplusplus
}
#endif

#endif	/* MD5_H */
