/*
 * sha1hmac.h
 *
 * SHA-1/HMAC message authentication code, header
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

#ifndef _SHA1HMAC_H
#define _SHA1HMAC_H

#include "hmac.h"
#include "fips180.h"

typedef struct
{
	byte kxi[64];
	byte kxo[64];
	sha1Param param;
} sha1hmacParam;

#ifdef __cplusplus
extern "C" {
#endif

extern BEEDLLAPI const keyedHashFunction sha1hmac;

BEEDLLAPI
int sha1hmacSetup (sha1hmacParam*, const uint32*, int);
BEEDLLAPI
int sha1hmacReset (sha1hmacParam*);
BEEDLLAPI
int sha1hmacUpdate(sha1hmacParam*, const byte*, int);
BEEDLLAPI
int sha1hmacDigest(sha1hmacParam*, uint32*);

#ifdef __cplusplus
}
#endif

#endif
