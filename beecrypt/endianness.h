/*
 * endianness.h
 *
 * Endian-dependant encoding/decoding, header
 *
 * Copyright (c) 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

#ifndef _ENDIANNESS_H
#define _ENDIANNESS_H

#include "beecrypt.h"

#include <stdio.h>

#ifdef __cplusplus
inline int16 swap16(int16 n)
{
	return (    ((n & 0xff) << 8) |
				((n & 0xff00) >> 8) );
}

inline uint16 swapu16(uint16 n)
{
	return (    ((n & 0xffU) << 8) |
				((n & 0xff00U) >> 8) );
}

inline int32 swap32(int32 n)
{
	#if (SIZEOF_LONG == 4)
	return (    ((n & 0xff) << 24) |
				((n & 0xff00) << 8) |
				((n & 0xff0000) >> 8) |
				((n & 0xff000000) >> 24) );
	#else
	return (    ((n & 0xffL) << 24) |
				((n & 0xff00L) << 8) |
				((n & 0xff0000L) >> 8) |
				((n & 0xff000000L) >> 24) );
	#endif
}

inline uint32 swapu32(uint32 n)
{
	#if (SIZEOF_UNSIGNED_LONG == 4)
	return (    ((n & 0xffU) << 24) |
				((n & 0xff00U) << 8) |
				((n & 0xff0000U) >> 8) |
				((n & 0xff000000U) >> 24) );
	#else
	return (    ((n & 0xffUL) << 24) |
				((n & 0xff00UL) << 8) |
				((n & 0xff0000UL) >> 8) |
				((n & 0xff000000UL) >> 24) );
	#endif
}

inline int64 swap64(int64 n)
{
	#if HAVE_LONG_LONG
	return (    ((n & 0xffLL) << 56) |
				((n & 0xff00LL) << 40) |
				((n & 0xff0000LL) << 24) |
				((n & 0xff000000LL) << 8) |
				((n & 0xff00000000LL) >> 8) |
				((n & 0xff0000000000LL) >> 24) |
				((n & 0xff000000000000LL) >> 40) |
				((n & 0xff00000000000000LL) >> 56) );
	#else
	return (    ((n & 0xffL) << 56) |
				((n & 0xff00L) << 40) |
				((n & 0xff0000L) << 24) |
				((n & 0xff000000L) << 8) |
				((n & 0xff00000000L) >> 8) |
				((n & 0xff0000000000L) >> 24) |
				((n & 0xff000000000000L) >> 40) |
				((n & 0xff00000000000000L) >> 56) );
	#endif
}
#else
 int16 swap16 (int16 n)
	/*@*/;
uint16 swapu16(uint16 n)
	/*@*/;
 int32 swap32 (int32 n)
	/*@*/;
uint32 swapu32(uint32 n)
	/*@*/;
 int64 swap64 (int64 n)
	/*@*/;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int encodeByte(javabyte b, byte* data)
	/*@modifies data */;
BEEDLLAPI
int encodeShort(javashort s, byte* data)
	/*@modifies data */;
BEEDLLAPI
int encodeInt(javaint i, byte* data)
	/*@modifies data */;

BEEDLLAPI
int encodeLong(javalong l, byte* data)
	/*@modifies data */;
BEEDLLAPI
int encodeChar(javachar c, byte* data)
	/*@modifies data */;
BEEDLLAPI
int encodeFloat(javafloat f, byte* data)
	/*@modifies data */;
BEEDLLAPI
int encodeDouble(javadouble d, byte* data)
	/*@modifies data */;

BEEDLLAPI
int encodeInts(const javaint* i, byte* data, int count)
	/*@modifies data */;
BEEDLLAPI
int encodeIntsPartial(const javaint* i, byte* data, int bytecount)
	/*@modifies data */;
BEEDLLAPI
int encodeChars(const javachar* c, byte* data, int count)
	/*@modifies data */;

BEEDLLAPI
int decodeByte(javabyte* b, const byte* data)
	/*@modifies b */;
BEEDLLAPI
int decodeShort(javashort* s, const byte* data)
	/*@modifies s */;
BEEDLLAPI
int decodeInt(javaint* i, const byte* data)
	/*@modifies i */;
BEEDLLAPI
int decodeLong(javalong* l, const byte* data)
	/*@modifies l */;
BEEDLLAPI
int decodeChar(javachar* c, const byte* data)
	/*@modifies c */;
BEEDLLAPI
int decodeFloat(javafloat* f, const byte* data)
	/*@modifies f */;
BEEDLLAPI
int decodeDouble(javadouble* d, const byte* data)
	/*@modifies d */;

BEEDLLAPI
int decodeInts(javaint* i, const byte* data, int count)
	/*@modifies i */;
BEEDLLAPI
int decodeIntsPartial(javaint* i, const byte* data, int bytecount)
	/*@modifies i */;
BEEDLLAPI
int decodeChars(javachar* c, const byte* data, int count)
	/*@modifies c */;

BEEDLLAPI
int writeByte(javabyte b, FILE* ofp)
	/*@modifies ofp, fileSystem */;
BEEDLLAPI
int writeShort(javashort s, FILE* ofp)
	/*@modifies ofp, fileSystem */;
BEEDLLAPI
int writeInt(javaint i, FILE* ofp)
	/*@modifies ofp, fileSystem */;
BEEDLLAPI
int writeLong(javalong l, FILE* ofp)
	/*@modifies ofp, fileSystem */;
BEEDLLAPI
int writeChar(javachar c, FILE* ofp)
	/*@modifies ofp, fileSystem */;

BEEDLLAPI
int writeInts(const javaint* i, FILE* ofp, int count)
	/*@modifies ofp, fileSystem */;
BEEDLLAPI
int writeChars(const javachar* c, FILE* ofp, int count)
	/*@modifies ofp, fileSystem */;

BEEDLLAPI
int readByte(javabyte* b, FILE* ifp)
	/*@modifies b, ifp, fileSystem */;
BEEDLLAPI
int readShort(javashort* s, FILE* ifp)
	/*@modifies s, ifp, fileSystem */;
BEEDLLAPI
int readInt(javaint* i, FILE* ifp)
	/*@modifies i, ifp, fileSystem */;
BEEDLLAPI
int readLong(javalong* l, FILE* ifp)
	/*@modifies l, ifp, fileSystem */;
BEEDLLAPI
int readChar(javachar* c, FILE* ifp)
	/*@modifies c, ifp, fileSystem */;

BEEDLLAPI
int readInts(javaint* i, FILE* ifp, int count)
	/*@modifies i, ifp, fileSystem */;
BEEDLLAPI
int readChars(javachar* c, FILE* ifp, int count)
	/*@modifies c, ifp, fileSystem */;

#ifdef __cplusplus
}
#endif

#endif
