/*
 * Copyright (c) 2000, 2001, 2002 Virtual Unlimited B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!\file sha256.h
 * \brief SHA-256 hash function, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup HASH_m HASH_sha256_m
 */

#ifndef _SHA256_H
#define _SHA256_H

#include "beecrypt.h"

/** \ingroup HASH_sha256_m
 */
typedef struct
{
	uint32_t h[8];
	uint32_t data[64];
	#if (MP_WBITS == 64)
	mpw length[1];
	#elif (MP_WBITS == 32)
	mpw length[2];
	#else
	# error
	#endif
	short offset;
} sha256Param;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HASH_sha256_m
 * Holds the full API description of the SHA-256 algorithm.
 */
/*@observer@*/ /*@checked@*/
extern BEECRYPTAPI const hashFunction sha256;

/*@-exportlocal@*/
/** \ingroup HASH_sha256_m
 * This function performs the SHA-256 hash algorithm on 64 byte blocks of data.
 * @param sp		hash parameter block
 */
BEECRYPTAPI
void sha256Process(sha256Param* sp)
	/*@globals internalState @*/
	/*@modifies sp, internalState @*/;

/** \ingroup HASH_sha256_m
 * This function resets the parameter block so that it's ready for a new hash.
 * @param sp		hash parameter block
 * @return		0 on success
 */
BEECRYPTAPI
int  sha256Reset  (sha256Param* sp)
	/*@modifies sp @*/;

/** \ingroup HASH_sha256_m
 * This function should be used to pass successive blocks of data to be hashed.
 * @param sp		hash parameter block
 * @param *data		bytes to hash
 * @param size		no. of bytes to hash
 * @return		0 on success
 */
BEECRYPTAPI
int  sha256Update (sha256Param* sp, const byte* data, size_t size)
	/*@globals internalState @*/
	/*@modifies sp, internalState @*/;

/** \ingroup HASH_sha256_m
 * This function finishes the current hash computation, returning the digest
 * value in \a digest.
 * @param sp		hash parameter block
 * @retval *digest	32 byte SHA-256 digest
 * @return		0 on success
 */
BEECRYPTAPI
int  sha256Digest (sha256Param* sp, /*@out@*/ byte* digest)
	/*@globals internalState @*/
	/*@modifies sp, digest, internalState @*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
