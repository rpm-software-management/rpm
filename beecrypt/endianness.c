/*
 * endianness.c
 *
 * Endianness-dependant encoding/decoding - implementation
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
 
#define BEECRYPT_DLL_EXPORT

#include "endianness.h"

#if HAVE_STRING_H
# include <string.h>
#endif

#include <stdio.h>

/*@-shiftimplementation@*/
int16 swap16(int16 n)
{
	return (    ((n & 0xff) << 8) |
				((n & 0xff00) >> 8) );
}

uint16 swapu16(uint16 n)
{
	return (    ((n & 0xffU) << 8) |
				((n & 0xff00U) >> 8) );
}

int32 swap32(int32 n)
{
	return (    ((n & 0xff) << 24) |
				((n & 0xff00) << 8) |
				((n & 0xff0000) >> 8) |
				((n & 0xff000000) >> 24) );
}

uint32 swapu32(uint32 n)
{
	return (    ((n & 0xffU) << 24) |
				((n & 0xff00U) << 8) |
				((n & 0xff0000U) >> 8) |
				((n & 0xff000000U) >> 24) );
}

int64 swap64(int64 n)
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
/*@=shiftimplementation@*/

int encodeByte(javabyte b, byte *data)
{
	*data = b;
	return 1;
}

int encodeShort(javashort s, byte *data)
{
	#if (!WORDS_BIGENDIAN)
	s = swap16(s);
	#endif
	memcpy(data, &s, 2);
	return 2;
}

int encodeInt(javaint i, byte* data)
{
	#if (!WORDS_BIGENDIAN)
	i = swap32(i);
	#endif
	memcpy(data, &i, 4);
	return 4;
}

int encodeLong(javalong l, byte* data)
{
	#if (!WORDS_BIGENDIAN)
	l = swap64(l);
	#endif
	memcpy(data, &l, 8);
	return 8;
}

int encodeFloat(javafloat f, byte* data)
{
	#if (!WORDS_BIGENDIAN)
	register const byte* src = ((const byte*) &f) + 3;
	register int i;
	for (i = 0; i < 4; i++)
		data[i] = *(src--);
	#else
	memcpy(data, &f, 4);
	#endif
	return 4;
}

int encodeDouble(javadouble d, byte* data)
{
	#if (!WORDS_BIGENDIAN)
	register const byte* src = ((byte*) &d) + 7;
	register int i;
	for (i = 0; i < 8; i++)
		data[i] = *(src--);
	#else
	memcpy(data, &d, 8);
	#endif
	return 8;
}

int encodeChar(javachar c, byte* data)
{
	#if (!WORDS_BIGENDIAN)
	c = swapu16(c);
	#endif
	memcpy(data, &c, 2);
	return 2;
}

int encodeInts(const javaint* i, byte* data, int count)
{
	register int rc = ((uint32)count) << 2;
	#if (WORDS_BIGENDIAN)
	memcpy(data, i, rc);
	#else
	javaint tmp;
	while (count--)
	{
		tmp = swap32(*(i++));
		memcpy(data, &tmp, 4);
		data += 4;
	}
	#endif
	return rc;
}

int encodeIntsPartial(const javaint* i, byte* data, int bytecount)
{
	register int rc = bytecount;
	#if (WORDS_BIGENDIAN)
	memcpy(data, i, rc);
	#else
	javaint tmp;

	while (bytecount > 0)
	{
		tmp = swap32(*(i++));
		memcpy(data, &tmp, (bytecount > 4) ? 4 : bytecount);
		data += 4;
		bytecount -= 4;
	}
	#endif
	return rc;
}

int encodeChars(const javachar* c, byte* data, int count)
{
	register int rc = ((uint32)count) << 1;
	#if (WORDS_BIGENDIAN)
	memcpy(data, c, rc);
	#else
	javaint tmp;
	while (count--)
	{
		tmp = swapu16(*(c++));
		memcpy(data, &tmp, 2);
		data += 2;
	}
	#endif
	return rc;
}

int decodeByte(javabyte* b, const byte* data)
{
	*b = *data;
	return 1;
}

int decodeShort(javashort* s, const byte* data)
{
	#if (WORDS_BIGENDIAN)
	memcpy(s, data, 2);
	#else
	javashort tmp;
	memcpy(&tmp, data, 2);
	*s = swap16(tmp);
	#endif
	return 2;
}

int decodeInt(javaint* i, const byte* data)
{
	#if (WORDS_BIGENDIAN)
	memcpy(i, data, 4);
	#else
	javaint tmp;
	memcpy(&tmp, data, 4);
	*i = swap32(tmp);
	#endif
	return 4;
}

int decodeLong(javalong* l, const byte* data)
{
	#if (WORDS_BIGENDIAN)
	memcpy(l, data, 8);
	#else
	javalong tmp;
	memcpy(&tmp, data, 8);
	*l = swap64(tmp);
	#endif
	return 8;
}

int decodeFloat(javafloat* f, const byte* data)
{
	#if (WORDS_BIGENDIAN)
	memcpy(f, data, 4);
	#else
	register byte *dst = ((byte*) f) + 3;
	register int i;
	for (i = 0; i < 4; i++)
		*(dst--) = data[i];
	#endif
	return 4;
}

int decodeDouble(javadouble* d, const byte* data)
{
	#if (WORDS_BIGENDIAN)
	memcpy(d, data, 8);
	#else
	register byte *dst = ((byte*) d) + 7;
	register int i;
	for (i = 0; i < 8; i++)
		*(dst--) = data[i];
	#endif
	return 8;
}

int decodeChar(javachar* c, const byte* data)
{
	#if (WORDS_BIGENDIAN)
	memcpy(c, data, 2);
	#else
	javachar tmp;
	memcpy(&tmp, data, 2);
	*c = swapu16(tmp);
	#endif
	return 2;
}

int decodeInts(javaint* i, const byte* data, int count)
{
	register int rc = ((uint32)count) << 2;
	#if (WORDS_BIGENDIAN)
	memcpy(i, data, rc);
	#else
	javaint tmp;
	while (count--)
	{
		memcpy(&tmp, data, 4);
		*(i++) = swap32(tmp);
		data += 4;
	}
	#endif
	return rc;
}

int decodeIntsPartial(javaint* i, const byte* data, int bytecount)
{
	register int rc = bytecount;
	#if (WORDS_BIGENDIAN)
	memcpy(i, data, rc);
	if (rc & 0x3)
		memset(i + (rc >> 2), 0, 4 - (rc & 0x3));
	#else
	javaint tmp;
	while (bytecount >= 4)
	{
		memcpy(&tmp, data, 4);
		*(i++) = swap32(tmp);
		data += 4;
		bytecount -= 4;
	}
	if (bytecount)
	{
		tmp = 0;
		memcpy(&tmp, data, bytecount);
		*(i++) = swap32(tmp);
	}
	#endif
	return rc;
}

int decodeChars(javachar* c, const byte* data, int count)
{
	register int rc = ((uint32)count) << 1;
	#if (WORDS_BIGENDIAN)
	memcpy(c, data, rc);
	#else
	javachar tmp;
	while (count--)
	{
		memcpy(&tmp, data, 2);
		*(c++) = swapu16(tmp);
		data += 2;
	}
	#endif
	return rc;
}

int readByte(javabyte* b, FILE* ifp)
{
	return fread(b, 1, 1, ifp);
}

int readShort(javashort* s, FILE* ifp)
{
	register int rc = fread(s, 2, 1, ifp);
	#if !(WORDS_BIGENDIAN)
	if (rc == 1)
	{
		register javashort tmp = *s;
		*s = swap16(tmp);
	}
	#endif
	return rc;
}

int readInt(javaint* i, FILE* ifp)
{
	register int rc = fread(i, 4, 1, ifp);
	#if !(WORDS_BIGENDIAN)
	if (rc == 1)
	{
		register javaint tmp = *i;
		*i = swap32(tmp);
	}
	#endif
	return rc;
}

int readLong(javalong* l, FILE* ifp)
{
	register int rc = fread(l, 8, 1, ifp);
	#if !(WORDS_BIGENDIAN)
	if (rc == 1)
	{
		register javalong tmp = *l;
		*l = swap64(tmp);
	}
	#endif
	return rc;
}

int readChar(javachar* c, FILE* ifp)
{
	register int rc = fread(c, 2, 1, ifp);
	#if !(WORDS_BIGENDIAN)
	if (rc == 1)
	{
		register javachar tmp = *c;
		*c = swapu16(tmp);
	}
	#endif
	return rc;
}

int readInts(javaint* i, FILE* ifp, int count)
{
	register int rc = fread(i, 4, count, ifp);
	#if !(WORDS_BIGENDIAN)
	if (rc == count)
	{
		while (count > 0)
		{
			register javaint tmp = *i;
			*(i++) = swap32(tmp);
			count--;
		}
	}
	#endif
	return rc;
}

int readChars(javachar* c, FILE* ifp, int count)
{
	register int rc = fread(c, 2, count, ifp);
	#if !(WORDS_BIGENDIAN)
	if (rc == count)
	{
		while (count > 0)
		{
			register javachar tmp = *c;
			*(c++) = swap16(tmp);
			count--;
		}
	}
	#endif
	return rc;
}

int writeByte(javabyte b, FILE* ofp)
{
	return fwrite(&b, 1, 1, ofp);
}

int writeShort(javashort s, FILE* ofp)
{
	#if !(WORDS_BIGENDIAN)
	s = swap16(s);
	#endif
	return fwrite(&s, 2, 1, ofp);
}

int writeInt(javaint i, FILE* ofp)
{
	#if !(WORDS_BIGENDIAN)
	i = swap32(i);
	#endif
	return fwrite(&i, 4, 1, ofp);
}

int writeLong(javalong l, FILE* ofp)
{
	#if !(WORDS_BIGENDIAN)
	l = swap64(l);
	#endif
	return fwrite(&l, 8, 1, ofp);
}

int writeChar(javachar c, FILE* ofp)
{
	#if !(WORDS_BIGENDIAN)
	c = swap16(c);
	#endif
	return fwrite(&c, 2, 1, ofp);
}

int writeInts(const javaint* i, FILE* ofp, int count)
{
	#if WORDS_BIGENDIAN
	return fwrite(i, 4, count, ofp);
	#else
	register int total = 0;
	while (count-- > 0)
	{
		register int rc = writeInt(*(i++), ofp);
		if (rc < 0)
			break;
		total += rc;
	}
	return total;
	#endif
}

int writeChars(const javachar* c, FILE* ofp, int count)
{
	#if WORDS_BIGENDIAN
	return fwrite(c, 2, count, ofp);
	#else
	register int total = 0;
	while (count-- > 0)
	{
		register int rc = writeChar(*(c++), ofp);
		if (rc < 0)
			break;
		total += rc;
	}
	return total;
	#endif
}
