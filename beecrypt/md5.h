/*
 * md5.h
 *
 * MD5 hash function, header
 *
 * Copyright (c) 2000 Virtual Unlimited B.V.
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

#ifndef _MD5_H
#define _MD5_H

#include "beecrypt.h"

typedef struct
{
	uint32 h[4];
	uint32 data[16];
	uint64 length;
	uint8  offset;
} md5Param;

#ifdef __cplusplus
extern "C" {
#endif

extern BEEDLLAPI const hashFunction md5;

BEEDLLAPI
void md5Process(md5Param* p)
	/*@modifies p @*/;
BEEDLLAPI
int md5Reset   (md5Param* p)
	/*@modifies p @*/;
BEEDLLAPI
int md5Update  (md5Param* p, const byte* data, int size)
	/*@modifies p @*/;
BEEDLLAPI
int md5Digest  (md5Param* p, uint32* data)
	/*@modifies p, data @*/;

#ifdef __cplusplus
}
#endif

#endif
