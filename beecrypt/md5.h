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
	short offset;
} md5Param;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HASH_md5_m
 */
/*@observer@*/ /*@checked@*/
extern BEECRYPTAPI const hashFunction md5;

/** \ingroup HASH_md5_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
void md5Process(md5Param* mp)
	/*@modifies mp @*/;
/*@=exportlocal@*/

/** \ingroup HASH_md5_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int md5Reset   (md5Param* mp)
	/*@modifies mp @*/;
/*@=exportlocal@*/

/** \ingroup HASH_md5_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int md5Update  (md5Param* mp, const byte* data, size_t size)
	/*@modifies mp @*/;
/*@=exportlocal@*/

/** \ingroup HASH_md5_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int md5Digest  (md5Param* mp, /*@out@*/ byte* data)
	/*@modifies mp, data @*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
