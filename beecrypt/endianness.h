/*
 * endianness.h
 *
 * Endian-dependant encoding/decoding, header
 *
 * Copyright (c) 1998-2000 Virtual Unlimited B.V.
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
	#if (SIZEOF_LONG == 4)
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
 int16 swap16 (int16);
uint16 swapu16(uint16);
 int32 swap32 (int32);
uint32 swapu32(uint32);
 int64 swap64 (int64);
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int encodeByte(javabyte, byte*);
BEEDLLAPI
int encodeShort(javashort, byte*);
BEEDLLAPI
int encodeInt(javaint, byte*);

BEEDLLAPI
int encodeLong(javalong, byte*);
BEEDLLAPI
int encodeChar(javachar, byte*);
BEEDLLAPI
int encodeFloat(javafloat, byte*);
BEEDLLAPI
int encodeDouble(javadouble, byte*);

BEEDLLAPI
int encodeInts(const javaint*, byte*, int);
BEEDLLAPI
int encodeChars(const javachar*, byte*, int);

BEEDLLAPI
int decodeByte(javabyte*, const byte*);
BEEDLLAPI
int decodeShort(javashort*, const byte*);
BEEDLLAPI
int decodeInt(javaint*, const byte*);
BEEDLLAPI
int decodeLong(javalong*, const byte*);
BEEDLLAPI
int decodeChar(javachar*, const byte*);
BEEDLLAPI
int decodeFloat(javafloat*, const byte*);
BEEDLLAPI
int decodeDouble(javadouble*, const byte*);

BEEDLLAPI
int decodeInts(javaint*, const byte*, int);
BEEDLLAPI
int decodeChars(javachar*, const byte*, int);

BEEDLLAPI
int writeByte(javabyte, FILE*);
BEEDLLAPI
int writeShort(javashort, FILE*);
BEEDLLAPI
int writeInt(javaint, FILE*);
BEEDLLAPI
int writeLong(javalong, FILE*);
BEEDLLAPI
int writeChar(javachar, FILE*);

BEEDLLAPI
int writeInts(const javaint*, FILE*, int);
BEEDLLAPI
int writeChars(const javachar*, FILE*, int);

BEEDLLAPI
int readByte(javabyte*, FILE*);
BEEDLLAPI
int readShort(javashort*, FILE*);
BEEDLLAPI
int readInt(javaint*, FILE*);
BEEDLLAPI
int readLong(javalong*, FILE*);
BEEDLLAPI
int readChar(javachar*, FILE*);

BEEDLLAPI
int readInts(javaint*, FILE*, int);
BEEDLLAPI
int readChars(javachar*, FILE*, int);

#ifdef __cplusplus
}
#endif

#endif
