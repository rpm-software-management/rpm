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

/*!\file hmacmd5.h
 * \brief HMAC-MD5 message authentication code, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup HMAC_m HMAC_md5_m
 */

#ifndef _HMACMD5_H
#define _HMACMD5_H

#include "beecrypt/hmac.h"
#include "beecrypt/md5.h"

/*!\ingroup HMAC_md5_m
 */
typedef struct
{
	md5Param mparam;
	byte kxi[64];
	byte kxo[64];
} hmacmd5Param;

#ifdef __cplusplus
extern "C" {
#endif

extern BEECRYPTAPI const keyedHashFunction hmacmd5;

BEECRYPTAPI
int hmacmd5Setup (hmacmd5Param*, const byte*, size_t);
BEECRYPTAPI
int hmacmd5Reset (hmacmd5Param*);
BEECRYPTAPI
int hmacmd5Update(hmacmd5Param*, const byte*, size_t);
BEECRYPTAPI
int hmacmd5Digest(hmacmd5Param*, byte*);

#ifdef __cplusplus
}
#endif

#endif
