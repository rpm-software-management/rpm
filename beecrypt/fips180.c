/*
 * fips180.c
 *
 * SHA-1 hash function, code
 *
 * For more information on this algorithm, see:
 * NIST FIPS PUB 180-1
 *
 * Copyright (c) 1997, 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

/*@observer@*/ static const uint32 k[4] = { 0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6 };

/*@observer@*/ static const uint32 hinit[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };

const hashFunction sha1 = { "SHA-1", sizeof(sha1Param), 64, 5 * sizeof(uint32), (hashFunctionReset) sha1Reset, (hashFunctionUpdate) sha1Update, (hashFunctionDigest) sha1Digest };

int sha1Reset(register sha1Param *p)
{
	mp32copy(5, p->h, hinit);
	mp32zero(80, p->data);
	p->length = 0;
	p->offset = 0;
	return 0;
}

#define SUBROUND1(a, b, c, d, e, w, k) \
	e = ROTL32(a, 5) + ((b&(c^d))^d) + e + w + k;	\
	b = ROTR32(b, 2)
#define SUBROUND2(a, b, c, d, e, w, k) \
	e = ROTL32(a, 5) + (b^c^d) + e + w + k;	\
	b = ROTR32(b, 2)
#define SUBROUND3(a, b, c, d, e, w, k) \
	e = ROTL32(a, 5) + (((b|c)&d)|(b&c)) + e + w + k;	\
	b = ROTR32(b, 2)
#define SUBROUND4(a, b, c, d, e, w, k) \
	e = ROTL32(a, 5) + (b^c^d) + e + w + k;	\
	b = ROTR32(b, 2)

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
		*(w++) = ROTL32(temp, 1);
	}

	w = p->data;

	a = p->h[0]; b = p->h[1]; c = p->h[2]; d = p->h[3]; e = p->h[4];

	SUBROUND1(a,b,c,d,e,w[ 0],k[0]);
	SUBROUND1(e,a,b,c,d,w[ 1],k[0]);
	SUBROUND1(d,e,a,b,c,w[ 2],k[0]);
	SUBROUND1(c,d,e,a,b,w[ 3],k[0]);
	SUBROUND1(b,c,d,e,a,w[ 4],k[0]);
	SUBROUND1(a,b,c,d,e,w[ 5],k[0]);
	SUBROUND1(e,a,b,c,d,w[ 6],k[0]);
	SUBROUND1(d,e,a,b,c,w[ 7],k[0]);
	SUBROUND1(c,d,e,a,b,w[ 8],k[0]);
	SUBROUND1(b,c,d,e,a,w[ 9],k[0]);
	SUBROUND1(a,b,c,d,e,w[10],k[0]);
	SUBROUND1(e,a,b,c,d,w[11],k[0]);
	SUBROUND1(d,e,a,b,c,w[12],k[0]);
	SUBROUND1(c,d,e,a,b,w[13],k[0]);
	SUBROUND1(b,c,d,e,a,w[14],k[0]);
	SUBROUND1(a,b,c,d,e,w[15],k[0]);
	SUBROUND1(e,a,b,c,d,w[16],k[0]);
	SUBROUND1(d,e,a,b,c,w[17],k[0]);
	SUBROUND1(c,d,e,a,b,w[18],k[0]);
	SUBROUND1(b,c,d,e,a,w[19],k[0]);

	SUBROUND2(a,b,c,d,e,w[20],k[1]);
	SUBROUND2(e,a,b,c,d,w[21],k[1]);
	SUBROUND2(d,e,a,b,c,w[22],k[1]);
	SUBROUND2(c,d,e,a,b,w[23],k[1]);
	SUBROUND2(b,c,d,e,a,w[24],k[1]);
	SUBROUND2(a,b,c,d,e,w[25],k[1]);
	SUBROUND2(e,a,b,c,d,w[26],k[1]);
	SUBROUND2(d,e,a,b,c,w[27],k[1]);
	SUBROUND2(c,d,e,a,b,w[28],k[1]);
	SUBROUND2(b,c,d,e,a,w[29],k[1]);
	SUBROUND2(a,b,c,d,e,w[30],k[1]);
	SUBROUND2(e,a,b,c,d,w[31],k[1]);
	SUBROUND2(d,e,a,b,c,w[32],k[1]);
	SUBROUND2(c,d,e,a,b,w[33],k[1]);
	SUBROUND2(b,c,d,e,a,w[34],k[1]);
	SUBROUND2(a,b,c,d,e,w[35],k[1]);
	SUBROUND2(e,a,b,c,d,w[36],k[1]);
	SUBROUND2(d,e,a,b,c,w[37],k[1]);
	SUBROUND2(c,d,e,a,b,w[38],k[1]);
	SUBROUND2(b,c,d,e,a,w[39],k[1]);

	SUBROUND3(a,b,c,d,e,w[40],k[2]);
	SUBROUND3(e,a,b,c,d,w[41],k[2]);
	SUBROUND3(d,e,a,b,c,w[42],k[2]);
	SUBROUND3(c,d,e,a,b,w[43],k[2]);
	SUBROUND3(b,c,d,e,a,w[44],k[2]);
	SUBROUND3(a,b,c,d,e,w[45],k[2]);
	SUBROUND3(e,a,b,c,d,w[46],k[2]);
	SUBROUND3(d,e,a,b,c,w[47],k[2]);
	SUBROUND3(c,d,e,a,b,w[48],k[2]);
	SUBROUND3(b,c,d,e,a,w[49],k[2]);
	SUBROUND3(a,b,c,d,e,w[50],k[2]);
	SUBROUND3(e,a,b,c,d,w[51],k[2]);
	SUBROUND3(d,e,a,b,c,w[52],k[2]);
	SUBROUND3(c,d,e,a,b,w[53],k[2]);
	SUBROUND3(b,c,d,e,a,w[54],k[2]);
	SUBROUND3(a,b,c,d,e,w[55],k[2]);
	SUBROUND3(e,a,b,c,d,w[56],k[2]);
	SUBROUND3(d,e,a,b,c,w[57],k[2]);
	SUBROUND3(c,d,e,a,b,w[58],k[2]);
	SUBROUND3(b,c,d,e,a,w[59],k[2]);

	SUBROUND4(a,b,c,d,e,w[60],k[3]);
	SUBROUND4(e,a,b,c,d,w[61],k[3]);
	SUBROUND4(d,e,a,b,c,w[62],k[3]);
	SUBROUND4(c,d,e,a,b,w[63],k[3]);
	SUBROUND4(b,c,d,e,a,w[64],k[3]);
	SUBROUND4(a,b,c,d,e,w[65],k[3]);
	SUBROUND4(e,a,b,c,d,w[66],k[3]);
	SUBROUND4(d,e,a,b,c,w[67],k[3]);
	SUBROUND4(c,d,e,a,b,w[68],k[3]);
	SUBROUND4(b,c,d,e,a,w[69],k[3]);
	SUBROUND4(a,b,c,d,e,w[70],k[3]);
	SUBROUND4(e,a,b,c,d,w[71],k[3]);
	SUBROUND4(d,e,a,b,c,w[72],k[3]);
	SUBROUND4(c,d,e,a,b,w[73],k[3]);
	SUBROUND4(b,c,d,e,a,w[74],k[3]);
	SUBROUND4(a,b,c,d,e,w[75],k[3]);
	SUBROUND4(e,a,b,c,d,w[76],k[3]);
	SUBROUND4(d,e,a,b,c,w[77],k[3]);
	SUBROUND4(c,d,e,a,b,w[78],k[3]);
	SUBROUND4(b,c,d,e,a,w[79],k[3]);

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
		memmove(((byte *) p->data) + p->offset, data, proclength);
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
	/*@modifies p @*/
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

	/*@-shiftsigned@*/ /* p->length is uint64 */
	#if WORDS_BIGENDIAN
	p->data[14] = ((uint32)(p->length >> 29));
	p->data[15] = ((uint32)((p->length << 3) & 0xffffffff));
	#else
	p->data[14] = swapu32((uint32)(p->length >> 29));
	p->data[15] = swapu32((uint32)((p->length << 3) & 0xffffffff));
	#endif
	/*@=shiftsigned@*/

	sha1Process(p);
	p->offset = 0;
}

int sha1Digest(register sha1Param *p, uint32 *data)
{
	sha1Finish(p);
	mp32copy(5, data, p->h);
	(void) sha1Reset(p);
	return 0;
}
