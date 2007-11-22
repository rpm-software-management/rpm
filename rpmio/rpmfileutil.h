#ifndef _RPMFILEUTIL_H
#define _RPMFILEUTIL_H

#include <rpmpgp.h>

/**
 * Calculate a file digest and size.
 * @param algo		digest algorithm (ignored for now, md5 used)
 * @param fn		file name
 * @param asAscii	return checksum as ascii string?
 * @retval digest	address of calculated checksum
 * @retval *fsizep	file size pointer (or NULL)
 * @return		0 on success, 1 on error
 */
int rpmDoDigest(pgpHashAlgo algo, const char * fn,int asAscii,
		  unsigned char * digest, size_t * fsizep);

#endif /* _RPMFILEUTIL_H */
