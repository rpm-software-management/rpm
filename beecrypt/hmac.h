/** \ingroup HMAC_m
 * \file hmac.h
 *
 * HMAC message authentication code, header.
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

#ifndef _HMAC_H
#define _HMAC_H

#include "beecrypt.h"

/**
 */
typedef struct
{
	uint32 kxi[16];
	uint32 kxo[16];
} hmacParam;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI
int hmacSetup (hmacParam* hp, const hashFunction* hash, hashFunctionParam* param, const uint32* key, int keybits)
	/*@modifies hp */;

/**
 */
BEECRYPTAPI
int hmacReset (hmacParam* hp, const hashFunction* hash, hashFunctionParam* param)
	/*@modifies hp */;

/**
 */
BEECRYPTAPI
int hmacUpdate(hmacParam* hp, const hashFunction* hash, hashFunctionParam* param, const byte* data, int size)
	/*@modifies hp */;

/**
 */
BEECRYPTAPI
int hmacDigest(hmacParam* hp, const hashFunction* hash, hashFunctionParam* param, /*@out@*/ uint32* data)
	/*@modifies hp, data */;

#ifdef __cplusplus
}
#endif

#endif
