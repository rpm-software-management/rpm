/*
 * md5.c
 *
 * MD5 hash function, code
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

#define BEECRYPT_DLL_EXPORT

#include "md5.h"
#include "mp32.h"
#include "endianness.h"

static uint32 md5hinit[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

const hashFunction md5 = { "MD5", sizeof(md5Param), 4 * sizeof(uint32), (hashFunctionReset) md5Reset, (hashFunctionUpdate) md5Update, (hashFunctionDigest) md5Digest };

int md5Reset(register md5Param* p)
{
	mp32copy(4, p->h, md5hinit);
	mp32zero(16, p->data);
	p->length = 0;
	p->offset = 0;
	return 0;
}

#define ROL(x, s) (((x) << (s)) | ((x) >> (32 - (s))))

#define FF(a, b, c, d, w, s, t)	\
	a += ((b&(c^d))^d) + w + t;	\
	a = ROL(a, s);	\
	a += b;

#define GG(a, b, c, d, w, s, t)	\
	a += ((d&(b^c))^c) + w + t;	\
	a = ROL(a, s);	\
	a += b;

#define HH(a, b, c, d, w, s, t)	\
	a += (b^c^d) + w + t;	\
	a = ROL(a, s);	\
	a += b;

#define II(a, b, c, d, w, s, t)	\
	a += (c^(b|~d)) + w + t;	\
	a = ROL(a, s);	\
	a += b;

#ifndef ASM_MD5PROCESS
void md5Process(md5Param* p)
{
	register uint32 a,b,c,d;
	register uint32* w;
	#if WORDS_BIGENDIAN
	register byte t;
	#endif

	w = p->data;
	#if WORDS_BIGENDIAN
	t = 16;
	while (t--)
	{
		register uint32 temp = swapu32(*w);
		*(w++) = temp;
	}
	w = p->data;
	#endif

	a = p->h[0]; b = p->h[1]; c = p->h[2]; d = p->h[3];

	FF(a, b, c, d, w[ 0],  7, 0xd76aa478);
	FF(d, a, b, c, w[ 1], 12, 0xe8c7b756);
	FF(c, d, a, b, w[ 2], 17, 0x242070db);
	FF(b, c, d, a, w[ 3], 22, 0xc1bdceee);
	FF(a, b, c, d, w[ 4],  7, 0xf57c0faf);
	FF(d, a, b, c, w[ 5], 12, 0x4787c62a);
	FF(c, d, a, b, w[ 6], 17, 0xa8304613);
	FF(b, c, d, a, w[ 7], 22, 0xfd469501);
	FF(a, b, c, d, w[ 8],  7, 0x698098d8);
	FF(d, a, b, c, w[ 9], 12, 0x8b44f7af);
	FF(c, d, a, b, w[10], 17, 0xffff5bb1);
	FF(b, c, d, a, w[11], 22, 0x895cd7be);
	FF(a, b, c, d, w[12],  7, 0x6b901122);
	FF(d, a, b, c, w[13], 12, 0xfd987193);
	FF(c, d, a, b, w[14], 17, 0xa679438e);
	FF(b, c, d, a, w[15], 22, 0x49b40821);

	GG(a, b, c, d, w[ 1],  5, 0xf61e2562);
	GG(d, a, b, c, w[ 6],  9, 0xc040b340);
	GG(c, d, a, b, w[11], 14, 0x265e5a51);
	GG(b, c, d, a, w[ 0], 20, 0xe9b6c7aa);
	GG(a, b, c, d, w[ 5],  5, 0xd62f105d);
	GG(d, a, b, c, w[10],  9, 0x02441453);
	GG(c, d, a, b, w[15], 14, 0xd8a1e681);
	GG(b, c, d, a, w[ 4], 20, 0xe7d3fbc8);
	GG(a, b, c, d, w[ 9],  5, 0x21e1cde6);
	GG(d, a, b, c, w[14],  9, 0xc33707d6);
	GG(c, d, a, b, w[ 3], 14, 0xf4d50d87);
	GG(b, c, d, a, w[ 8], 20, 0x455a14ed);
	GG(a, b, c, d, w[13],  5, 0xa9e3e905);
	GG(d, a, b, c, w[ 2],  9, 0xfcefa3f8);
	GG(c, d, a, b, w[ 7], 14, 0x676f02d9);
	GG(b, c, d, a, w[12], 20, 0x8d2a4c8a);

	HH(a, b, c, d, w[ 5],  4, 0xfffa3942);
	HH(d, a, b, c, w[ 8], 11, 0x8771f681);
	HH(c, d, a, b, w[11], 16, 0x6d9d6122);
	HH(b, c, d, a, w[14], 23, 0xfde5380c);
	HH(a, b, c, d, w[ 1],  4, 0xa4beea44);
	HH(d, a, b, c, w[ 4], 11, 0x4bdecfa9);
	HH(c, d, a, b, w[ 7], 16, 0xf6bb4b60);
	HH(b, c, d, a, w[10], 23, 0xbebfbc70);
	HH(a, b, c, d, w[13],  4, 0x289b7ec6);
	HH(d, a, b, c, w[ 0], 11, 0xeaa127fa);
	HH(c, d, a, b, w[ 3], 16, 0xd4ef3085);
	HH(b, c, d, a, w[ 6], 23, 0x04881d05);
	HH(a, b, c, d, w[ 9],  4, 0xd9d4d039);
	HH(d, a, b, c, w[12], 11, 0xe6db99e5);
	HH(c, d, a, b, w[15], 16, 0x1fa27cf8);
	HH(b, c, d, a, w[ 2], 23, 0xc4ac5665);

	II(a, b, c, d, w[ 0],  6, 0xf4292244);
	II(d, a, b, c, w[ 7], 10, 0x432aff97);
	II(c, d, a, b, w[14], 15, 0xab9423a7);
	II(b, c, d, a, w[ 5], 21, 0xfc93a039);
	II(a, b, c, d, w[12],  6, 0x655b59c3);
	II(d, a, b, c, w[ 3], 10, 0x8f0ccc92);
	II(c, d, a, b, w[10], 15, 0xffeff47d);
	II(b, c, d, a, w[ 1], 21, 0x85845dd1);
	II(a, b, c, d, w[ 8],  6, 0x6fa87e4f);
	II(d, a, b, c, w[15], 10, 0xfe2ce6e0);
	II(c, d, a, b, w[ 6], 15, 0xa3014314);
	II(b, c, d, a, w[13], 21, 0x4e0811a1);
	II(a, b, c, d, w[ 4],  6, 0xf7537e82);
	II(d, a, b, c, w[11], 10, 0xbd3af235);
	II(c, d, a, b, w[ 2], 15, 0x2ad7d2bb);
	II(b, c, d, a, w[ 9], 21, 0xeb86d391);

    p->h[0] += a;
    p->h[1] += b;
    p->h[2] += c;
    p->h[3] += d;
}
#endif

int md5Update(md5Param* p, const byte* data, int size)
{
	register int proclength;

	p->length += size;
	while (size > 0)
	{
		proclength = ((p->offset + size) > 64) ? (64 - p->offset) : size;
		memcpy(((byte *) p->data) + p->offset, data, proclength);
		size -= proclength;
		data += proclength;
		p->offset += proclength;

		if (p->offset == 64)
		{
			md5Process(p);
			p->offset = 0;
		}
	}
	return 0;
}

static void md5Finish(md5Param* p)
{
	register byte *ptr = ((byte *) p->data) + p->offset++;

	*(ptr++) = 0x80;

	if (p->offset > 56)
	{
		while (p->offset++ < 64)
			*(ptr++) = 0;

		md5Process(p);
		p->offset = 0;
	}

	ptr = ((byte *) p->data) + p->offset;
	while (p->offset++ < 56)
		*(ptr++) = 0;

	#if !WORDS_BIGENDIAN
	p->data[14] = ((uint32)((p->length << 3) & 0xffffffff));
	p->data[15] = ((uint32)(p->length >> 29));
	#else
	p->data[14] = swapu32((uint32)((p->length << 3) & 0xffffffff));
	p->data[15] = swapu32((uint32)(p->length >> 29));
	#endif

	md5Process(p);

	#if 1 /* WORDS_BIGENDIAN */
	p->h[0] = swapu32(p->h[0]);
	p->h[1] = swapu32(p->h[1]);
	p->h[2] = swapu32(p->h[2]);
	p->h[3] = swapu32(p->h[3]);
	#endif
	p->offset = 0;
}

int md5Digest(md5Param* p, uint32* data)
{
	md5Finish(p);
	mp32copy(4, data, p->h);
	md5Reset(p);
	return 0;
}
