/** \ingroup HMAC_sha1_m HMAC_m
 * \file hmacsha1.c
 *
 * HMAC-SHA-1 message authentication code, code.
 */

/*
 * Copyright (c) 1999, 2000, 2001 Virtual Unlimited B.V.
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

#include "system.h"
#include "hmacsha1.h"
#include "debug.h"

/*@-sizeoftype@*/
const keyedHashFunction hmacsha1 = { "HMAC-SHA-1", sizeof(hmacsha1Param), 64, 5 * sizeof(uint32), 64, 512, 32, (keyedHashFunctionSetup) hmacsha1Setup, (keyedHashFunctionReset) hmacsha1Reset, (keyedHashFunctionUpdate) hmacsha1Update, (keyedHashFunctionDigest) hmacsha1Digest };
/*@=sizeoftype@*/

/*@-type@*/
int hmacsha1Setup (hmacsha1Param* sp, const uint32* key, int keybits)
{
	return hmacSetup((hmacParam*) sp, &sha1, &sp->param, key, keybits);
}

int hmacsha1Reset (hmacsha1Param* sp)
{
	return hmacReset((hmacParam*) sp, &sha1, &sp->param);
}

int hmacsha1Update(hmacsha1Param* sp, const byte* data, int size)
{
	return hmacUpdate((hmacParam*) sp, &sha1, &sp->param, data, size);
}

int hmacsha1Digest(hmacsha1Param* sp, uint32* data)
{
	return hmacDigest((hmacParam*) sp, &sha1, &sp->param, data);
}
/*@=type@*/
