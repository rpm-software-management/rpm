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

/*!\file hmacmd5.c
 * \brief HMAC-MD5 message authentication code.
 * 
 * \see RFC2202 - Test Cases for HMAC-MD5 and HMAC-SHA-1.
 *                P. Cheng, R. Glenn.
 *
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup HMAC_m HMAC_md5_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/hmacmd5.h"

/*!\addtogroup HMAC_md5_m
 * \{
 */

const keyedHashFunction hmacmd5 = {
	"HMAC-MD5",
	sizeof(hmacmd5Param),
	64,
	16,
	64,
	512,
	32,
	(keyedHashFunctionSetup) hmacmd5Setup,
	(keyedHashFunctionReset) hmacmd5Reset,
	(keyedHashFunctionUpdate) hmacmd5Update,
	(keyedHashFunctionDigest) hmacmd5Digest
};

int hmacmd5Setup (hmacmd5Param* sp, const byte* key, size_t keybits)
{
	return hmacSetup(sp->kxi, sp->kxo, &md5, &sp->mparam, key, keybits);
}

int hmacmd5Reset (hmacmd5Param* sp)
{
	return hmacReset(sp->kxi, &md5, &sp->mparam);
}

int hmacmd5Update(hmacmd5Param* sp, const byte* data, size_t size)
{
	return hmacUpdate(&md5, &sp->mparam, data, size);
}

int hmacmd5Digest(hmacmd5Param* sp, byte* data)
{
	return hmacDigest(sp->kxo, &md5, &sp->mparam, data);
}

/*!\}
 */
