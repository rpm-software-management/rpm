/*
 * sha256.h
 *
 * SHA-256 hash function, header
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

#ifndef _SHA256_H
#define _SHA256_H

#include "beecrypt.h"

typedef struct
{
	uint32 h[8];
	uint32 data[64];
	uint64 length;
	uint8  offset;
} sha256Param;

#ifdef __cplusplus
extern "C" {
#endif

extern BEEDLLAPI const hashFunction sha256;

BEEDLLAPI
void sha256Process(sha256Param*);
BEEDLLAPI
int  sha256Reset  (sha256Param*);
BEEDLLAPI
int  sha256Update (sha256Param*, const byte*, int);
BEEDLLAPI
int  sha256Digest (sha256Param*, uint32*);

#ifdef __cplusplus
}
#endif

#endif
