/*
 * fips180.c
 *
 * SHA-1 hash function, code
 *
 * For more information on this algorithm, see:
 * NIST FIPS PUB 180-1
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
 
#define BEECRYPT_DLL_EXPORT

#include "fips180.h"
#include "mp32.h"
#include "endianness.h"

static uint32 sha1hinit[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };

const hashFunction sha1 = { "SHA-1", sizeof(sha1Param), 5 * sizeof(uint32), (hashFunctionReset) sha1Reset, (hashFunctionUpdate) sha1Update, (hashFunctionDigest) sha1Digest };

int sha1Reset(register sha1Param *p)
{
	mp32copy(5, p->h, sha1hinit);
	mp32zero(80, p->data);
	p->length = 0;
	p->offset = 0;
	return 0;
}

#define K00 0x5a827999
#define K20 0x6ed9eba1
#define K40 0x8f1bbcdc
#define K60 0xca62c1d6

#define ROL1(x) (((x) << 1) | ((x) >> 31))
#define ROL5(x) (((x) << 5) | ((x) >> 27))
#define ROR2(x) (((x) >> 2) | ((x) << 30))

#define subround1(a, b, c, d, e, w) \
	e = ROL5(a) + ((b&(c^d))^d) + e + w + K00; b = ROR2(b)
#define subround2(a, b, c, d, e, w) \
	e = ROL5(a) + (b^c^d) + e + w + K20; b = ROR2(b)
#define subround3(a, b, c, d, e, w) \
	e = ROL5(a) + (((b|c)&d)|(b&c)) + e + w + K40; b = ROR2(b)
#define subround4(a, b, c, d, e, w) \
	e = ROL5(a) + (b^c^d) + e + w + K60; b = ROR2(b)

#ifndef ASM_SHA1PROCESS
void sha1Process(register sha1Param *p)
{
	register uint32 a, b, c, d, e;
	register uint32 *w;
	register byte t;
	
	#if WORDS_BIGENDIAN
	w = p->data + 16;
	#else
	w = p->data;
	t = 16;
	while (t--)
	{
		register uint32 temp = swapu32(*w);
		*(w++) = temp;
	}
	#endif

	t = 64;
	while (t--)
	{
		register uint32 temp = w[-3] ^ w[-8] ^ w[-14] ^ w[-16];
		*(w++) = ROL1(temp);
	}

	w = p->data;

	a = p->h[0]; b = p->h[1]; c = p->h[2]; d = p->h[3]; e = p->h[4];

	t = 4;
	while (t--)
	{
		subround1(a,b,c,d,e,w[0]);
		subround1(e,a,b,c,d,w[1]);
		subround1(d,e,a,b,c,w[2]);
		subround1(c,d,e,a,b,w[3]);
		subround1(b,c,d,e,a,w[4]);
		w += 5;
	}

	t = 4;
	while (t--)
	{
		subround2(a,b,c,d,e,w[0]);
		subround2(e,a,b,c,d,w[1]);
		subround2(d,e,a,b,c,w[2]);
		subround2(c,d,e,a,b,w[3]);
		subround2(b,c,d,e,a,w[4]);
		w += 5;
	}

	t = 4;
	while (t--)
	{
		subround3(a,b,c,d,e,w[0]);
		subround3(e,a,b,c,d,w[1]);
		subround3(d,e,a,b,c,w[2]);
		subround3(c,d,e,a,b,w[3]);
		subround3(b,c,d,e,a,w[4]);
		w += 5;
	}
	t = 4;
	while (t--)
	{
		subround4(a,b,c,d,e,w[0]);
		subround4(e,a,b,c,d,w[1]);
		subround4(d,e,a,b,c,w[2]);
		subround4(c,d,e,a,b,w[3]);
		subround4(b,c,d,e,a,w[4]);
		w += 5;
	}

	p->h[0] += a;
	p->h[1] += b;
	p->h[2] += c;
	p->h[3] += d;
	p->h[4] += e;
}
#endif

int sha1Update(register sha1Param *p, const byte *data, int size)
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
			sha1Process(p);
			p->offset = 0;
		}
	}
	return 0;
}

static void sha1Finish(register sha1Param *p)
{
	register byte *ptr = ((byte *) p->data) + p->offset++;

	*(ptr++) = 0x80;

	if (p->offset > 56)
	{
		while (p->offset++ < 64)
			*(ptr++) = 0;

		sha1Process(p);
		p->offset = 0;
	}

	ptr = ((byte *) p->data) + p->offset;
	while (p->offset++ < 56)
		*(ptr++) = 0;

	#if WORDS_BIGENDIAN
	p->data[14] = (p->length >> 29);
	p->data[15] = (p->length << 3) & 0xffffffff;
	#else
	p->data[14] = swapu32(p->length >> 29);
	p->data[15] = swapu32(p->length << 3) & 0xffffffff;
	#endif

	sha1Process(p);
	p->offset = 0;
}

int sha1Digest(register sha1Param *p, uint32 *data)
{
	sha1Finish(p);
	mp32copy(5, data, p->h);
	sha1Reset(p);
	return 0;
}
