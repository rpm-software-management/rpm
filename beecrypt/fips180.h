/*
 * fips180.h
 *
 * SHA-1 hash function, header
 *
 * Copyright (c) 1997-2000 Virtual Unlimited B.V.
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

#ifndef _FIPS180_H
#define _FIPS180_H

#include "beecrypt.h"
#include "fips180opt.h"

typedef struct
{
	uint32 h[5];
	uint32 data[80];
	uint64 length;
	uint8  offset;
} sha1Param;

#ifdef __cplusplus
extern "C" {
#endif

extern BEEDLLAPI const hashFunction sha1;

BEEDLLAPI
void sha1Process(sha1Param*);
BEEDLLAPI
int  sha1Reset  (sha1Param*);
BEEDLLAPI
int  sha1Update (sha1Param*, const byte*, int);
BEEDLLAPI
int  sha1Digest (sha1Param*, uint32*);

#ifdef __cplusplus
}
#endif

#endif
