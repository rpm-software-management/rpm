/*
 * Copyright (c) 2000, 2002 Virtual Unlimited B.V.
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

/*!\file md5.h
 * \brief MD5 hash function.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup HASH_m HASH_md5_m
 */

#ifndef _MD5_H
#define _MD5_H

#include "beecrypt.h"

/*!\ingroup HASH_md5_m
 */
typedef struct
{
	uint32_t h[4];
	uint32_t data[16];
	#if (MP_WBITS == 64)
	mpw length[1];
	#elif (MP_WBITS == 32)
	mpw length[2];
	#else
	# error
	#endif
	uint32_t offset;
} md5Param;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HASH_md5_m
 * Holds the full API description of the MD5 algorithm.
 */
/*@observer@*/ /*@checked@*/
extern BEECRYPTAPI const hashFunction md5;

/** \ingroup HASH_md5_m
 * This function performs the MD5 hash algorithm on 64 byte blocks of data.
 * @param mp		hash parameter block
 */
/*@-exportlocal@*/
BEECRYPTAPI
void md5Process(md5Param* mp)
	/*@modifies mp @*/;
/*@=exportlocal@*/

/** \ingroup HASH_md5_m
 * This function resets the parameter block so that it's ready for a new hash.
 * @param mp		hash parameter block
 * @return		0 on success
 */
/*@-exportlocal@*/
BEECRYPTAPI
int md5Reset   (md5Param* mp)
	/*@modifies mp @*/;
/*@=exportlocal@*/

/** \ingroup HASH_md5_m
 * This function should be used to pass successive blocks of data to be hashed.
 * @param mp		hash parameter block
 * @param *data		bytes to hash
 * @param size		no. of bytes to hash
 * @return		0 on success
 */
/*@-exportlocal@*/
BEECRYPTAPI
int md5Update  (md5Param* mp, const byte* data, size_t size)
	/*@modifies mp @*/;
/*@=exportlocal@*/

/** \ingroup HASH_md5_m
 * This function finishes the current hash computation, returning the digest
 * value in \a digest.
 * @param mp		hash parameter block
 * @retval *digest	16 byte MD5 digest
 * @return		0 on success
 */
/*@-exportlocal@*/
BEECRYPTAPI
int md5Digest  (md5Param* mp, /*@out@*/ byte* digest)
	/*@modifies mp, digest @*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
