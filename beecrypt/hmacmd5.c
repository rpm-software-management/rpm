/*
 * hmacmd5.c
 *
 * HMAC-MD5 message authentication code, code
 *
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

#define BEECRYPT_DLL_EXPORT

#include "hmacmd5.h"

const keyedHashFunction hmacmd5 = { "HMAC-MD5", sizeof(hmacmd5Param), 64, 4 * sizeof(uint32), 64, 512, 32, (const keyedHashFunctionSetup) hmacmd5Setup, (const keyedHashFunctionReset) hmacmd5Reset, (const keyedHashFunctionUpdate) hmacmd5Update, (const keyedHashFunctionDigest) hmacmd5Digest };

int hmacmd5Setup (hmacmd5Param* sp, const uint32* key, int keybits)
{
	return hmacSetup((hmacParam*) sp, &md5, &sp->param, key, keybits);
}

int hmacmd5Reset (hmacmd5Param* sp)
{
	return hmacReset((hmacParam*) sp, &md5, &sp->param);
}

int hmacmd5Update(hmacmd5Param* sp, const byte* data, int size)
{
	return hmacUpdate((hmacParam*) sp, &md5, &sp->param, data, size);
}

int hmacmd5Digest(hmacmd5Param* sp, uint32* data)
{
	return hmacDigest((hmacParam*) sp, &md5, &sp->param, data);
}
