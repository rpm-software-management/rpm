/** \ingroup HMAC_m
 * \file hmac.c
 *
 * HMAC message authentication code, code.
 */

/*
 * Copyright (c) 1999, 2000 Virtual Unlimited B.V.
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

#define BEECRYPT_DLL_EXPORT

#include "hmac.h"
#include "endianness.h"

#define HMAC_IPAD	0x36363636
#define HMAC_OPAD	0x5c5c5c5c

int hmacSetup(hmacParam* hp, const hashFunction* hash, hashFunctionParam* param, const uint32* key, int keybits)
{
	int keywords = (((uint32)keybits) >> 5);

	if (keywords <= 16)
	{
		register int i;

		if (keywords > 0)
		{
			(void) encodeInts((const javaint*) key, (byte*) hp->kxi, keywords);
			(void) encodeInts((const javaint*) key, (byte*) hp->kxo, keywords);

			for (i = 0; i < keywords; i++)
			{
				hp->kxi[i] ^= HMAC_IPAD;
				hp->kxo[i] ^= HMAC_OPAD;
			}
		}

		for (i = keywords; i < 16; i++)
		{
			hp->kxi[i] = HMAC_IPAD;
			hp->kxo[i] = HMAC_OPAD;
		}

		return hmacReset(hp, hash, param);
	}

	/* key too long */

	return -1;
}

int hmacReset(hmacParam* hp, const hashFunction* hash, hashFunctionParam* param)
{
	if (hash->reset(param))
		return -1;

	if (hash->update(param, (const byte*) hp->kxi, 64))
		return -1;

	return 0;
}

int hmacUpdate(/*@unused@*/ hmacParam* hp, const hashFunction* hash, hashFunctionParam* param, const byte* data, int size)
{
	return hash->update(param, data, size);
}

int hmacDigest(hmacParam* hp, const hashFunction* hash, hashFunctionParam* param, uint32* data)
{
	if (hash->digest(param, data))
		return -1;

	if (hash->update(param, (const byte*) hp->kxo, 64))
		return -1;

	/* digestsize is in bytes; divide by 4 to get the number of words */
	(void) encodeInts((const javaint*) data, (byte*) data, hash->digestsize >> 2);

	if (hash->update(param, (const byte*) data, hash->digestsize))
		return -1;

	if (hash->digest(param, data))
		return -1;

	return 0;
}
