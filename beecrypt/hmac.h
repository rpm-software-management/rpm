/*
 * hmac.h
 *
 * HMAC message authentication code, header
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

#ifndef _HMAC_H
#define _HMAC_H

#include "beecrypt.h"

typedef struct
{
	uint32 kxi[16];
	uint32 kxo[16];
} hmacParam;

#ifdef __cplusplus
extern "C" {
#endif

/* not used directly as keyed hash function, but instead used as generic methods */

BEEDLLAPI
int hmacSetup (hmacParam*, const hashFunction*, hashFunctionParam*, const uint32*, int);
BEEDLLAPI
int hmacReset (hmacParam*, const hashFunction*, hashFunctionParam*);
BEEDLLAPI
int hmacUpdate(hmacParam*, const hashFunction*, hashFunctionParam*, const byte*, int);
BEEDLLAPI
int hmacDigest(hmacParam*, const hashFunction*, hashFunctionParam*, uint32*);

#ifdef __cplusplus
}
#endif

#endif
