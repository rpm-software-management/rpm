/*
 * sha1hmac.c
 *
 * SHA-1/HMAC message authentication code, code
 *
 * Copyright (c) 1999-2000 Virtual Unlimited B.V.
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

#include "sha1hmac.h"

const keyedHashFunction sha1hmac = { "SHA-1/HMAC", sizeof(sha1hmacParam), 5 * sizeof(uint32), 64, 512, 32, (const keyedHashFunctionSetup) sha1hmacSetup, (const keyedHashFunctionReset) sha1hmacReset, (const keyedHashFunctionUpdate) sha1hmacUpdate, (const keyedHashFunctionDigest) sha1hmacDigest };

int sha1hmacSetup (sha1hmacParam* sp, const uint32* key, int keybits)
{
	return hmacSetup((hmacParam*) sp, &sha1, &sp->param, key, keybits);
}

int sha1hmacReset (sha1hmacParam* sp)
{
	return hmacReset((hmacParam*) sp, &sha1, &sp->param);
}

int sha1hmacUpdate(sha1hmacParam* sp, const byte* data, int size)
{
	return hmacUpdate((hmacParam*) sp, &sha1, &sp->param, data, size);
}

int sha1hmacDigest(sha1hmacParam* sp, uint32* data)
{
	return hmacDigest((hmacParam*) sp, &sha1, &sp->param, data);
}
