/** \ingroup HMAC_md5_m HMAC_m
 * \file hmacmd5.h
 *
 * HMAC-MD5 message authentication code, header.
 */

/*
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
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

#ifndef _HMACMD5_H
#define _HMACMD5_H

#include "hmac.h"
#include "md5.h"

/** \ingroup HMAC_md5_m
 */
typedef struct
{
/*@unused@*/	byte kxi[64];
/*@unused@*/	byte kxo[64];
	md5Param param;
} hmacmd5Param;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup HMAC_md5_m
 */
/*@unused@*/ extern BEEDLLAPI const keyedHashFunction hmacmd5;

/** \ingroup HMAC_md5_m
 */
BEEDLLAPI
int hmacmd5Setup (hmacmd5Param* sp, const uint32* key, int keybits)
	/*@modifies sp @*/;

/** \ingroup HMAC_md5_m
 */
BEEDLLAPI
int hmacmd5Reset (hmacmd5Param* sp)
	/*@modifies sp @*/;

/** \ingroup HMAC_md5_m
 */
BEEDLLAPI
int hmacmd5Update(hmacmd5Param* sp, const byte* data, int size)
	/*@modifies sp @*/;

/** \ingroup HMAC_md5_m
 */
BEEDLLAPI
int hmacmd5Digest(hmacmd5Param* sp, uint32* data)
	/*@modifies sp, data @*/;

#ifdef __cplusplus
}
#endif

#endif
