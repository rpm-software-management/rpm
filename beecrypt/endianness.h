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
/*@-exportlocal@*/

/**
 */
 int16 swap16 (int16 n)
	/*@*/;

/**
 */
uint16 swapu16(uint16 n)
	/*@*/;

/**
 */
 int32 swap32 (int32 n)
	/*@*/;

/**
 */
uint32 swapu32(uint32 n)
	/*@*/;

/**
 */
 int64 swap64 (int64 n)
	/*@*/;
/*@=exportlocal@*/
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 */
BEECRYPTAPI /*@unused@*/
int encodeByte(javabyte b, /*@out@*/ byte* data)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int encodeShort(javashort s, /*@out@*/ byte* data)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int encodeInt(javaint i, /*@out@*/ byte* data)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int encodeLong(javalong l, /*@out@*/ byte* data)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int encodeChar(javachar c, /*@out@*/ byte* data)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int encodeFloat(javafloat f, /*@out@*/ byte* data)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int encodeDouble(javadouble d, /*@out@*/ byte* data)
	/*@modifies data */;

/**
 */
BEECRYPTAPI
int encodeInts(const javaint* i, /*@out@*/ byte* data, int count)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int encodeIntsPartial(const javaint* i, /*@out@*/ byte* data, int bytecount)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int encodeChars(const javachar* c, /*@out@*/ byte* data, int count)
	/*@modifies data */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeByte(/*@out@*/ javabyte* b, const byte* data)
	/*@modifies b */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeShort(/*@out@*/ javashort* s, const byte* data)
	/*@modifies s */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeInt(/*@out@*/ javaint* i, const byte* data)
	/*@modifies i */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeLong(/*@out@*/ javalong* l, const byte* data)
	/*@modifies l */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeChar(/*@out@*/ javachar* c, const byte* data)
	/*@modifies c */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeFloat(/*@out@*/ javafloat* f, const byte* data)
	/*@modifies f */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeDouble(/*@out@*/ javadouble* d, const byte* data)
	/*@modifies d */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeInts(/*@out@*/ javaint* i, const byte* data, int count)
	/*@modifies i */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeIntsPartial(/*@out@*/ javaint* i, const byte* data, int bytecount)
	/*@modifies i */;

/**
 */
BEECRYPTAPI /*@unused@*/
int decodeChars(/*@out@*/ javachar* c, const byte* data, int count)
	/*@modifies c */;

/**
 */
BEECRYPTAPI /*@unused@*/
int writeByte(javabyte b, FILE* ofp)
	/*@globals fileSystem @*/
	/*@modifies ofp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int writeShort(javashort s, FILE* ofp)
	/*@globals fileSystem @*/
	/*@modifies ofp, fileSystem */;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int writeInt(javaint i, FILE* ofp)
	/*@globals fileSystem @*/
	/*@modifies ofp, fileSystem */;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI /*@unused@*/
int writeLong(javalong l, FILE* ofp)
	/*@globals fileSystem @*/
	/*@modifies ofp, fileSystem */;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int writeChar(javachar c, FILE* ofp)
	/*@globals fileSystem @*/
	/*@modifies ofp, fileSystem */;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI /*@unused@*/
int writeInts(const javaint* i, FILE* ofp, int count)
	/*@globals fileSystem @*/
	/*@modifies ofp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int writeChars(const javachar* c, FILE* ofp, int count)
	/*@globals fileSystem @*/
	/*@modifies ofp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int readByte(/*@out@*/ javabyte* b, FILE* ifp)
	/*@globals fileSystem @*/
	/*@modifies b, ifp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int readShort(/*@out@*/ javashort* s, FILE* ifp)
	/*@globals fileSystem @*/
	/*@modifies s, ifp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int readInt(/*@out@*/ javaint* i, FILE* ifp)
	/*@globals fileSystem @*/
	/*@modifies i, ifp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int readLong(/*@out@*/ javalong* l, FILE* ifp)
	/*@globals fileSystem @*/
	/*@modifies l, ifp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int readChar(/*@out@*/ javachar* c, FILE* ifp)
	/*@globals fileSystem @*/
	/*@modifies c, ifp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int readInts(/*@out@*/ javaint* i, FILE* ifp, int count)
	/*@globals fileSystem @*/
	/*@modifies i, ifp, fileSystem */;

/**
 */
BEECRYPTAPI /*@unused@*/
int readChars(/*@out@*/ javachar* c, FILE* ifp, int count)
	/*@globals fileSystem @*/
	/*@modifies c, ifp, fileSystem */;

#ifdef __cplusplus
}
#endif

#endif
