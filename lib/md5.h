#ifndef MD5_H
#define MD5_H

#include <sys/types.h>

typedef unsigned int uint32;

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

void rpmMD5Init( /*@out@*/ struct MD5Context *context, int brokenEndian);
void rpmMD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void rpmMD5Final(unsigned char digest[16], struct MD5Context *context);
void rpmMD5Transform(uint32 buf[4], uint32 const in[16]);

int mdfile(const char *fn, unsigned char *digest);
int mdbinfile(const char *fn, unsigned char *bindigest);

/* These assume a little endian machine and return incorrect results!
   They are here for compatibility with old (broken) versions of RPM */
int mdfileBroken(const char *fn, unsigned char *digest);
int mdbinfileBroken(const char *fn, unsigned char *bindigest);

#ifdef __cplusplus
}
#endif

#endif	/* MD5_H */
