/*
 * sha256.c
 *
 * SHA-256 hash function, code
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
 
#define BEECRYPT_DLL_EXPORT

#include "sha256.h"
#include "mp32.h"
#include "endianness.h"

static const uint32 k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static const uint32 hinit[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

const hashFunction sha256 = { "SHA-256", sizeof(sha256Param), 64, 8 * sizeof(uint32), (hashFunctionReset) sha256Reset, (hashFunctionUpdate) sha256Update, (hashFunctionDigest) sha256Digest };

int sha256Reset(register sha256Param *p)
{
	mp32copy(8, p->h, hinit);
	mp32zero(64, p->data);
	p->length = 0;
	p->offset = 0;
	return 0;
}

#define R(x,s)  ((x) >> (s))
#define S(x,s) ROTR32(x, s)

#define CH(x,y,z) ((x&(y^z))^z)
#define MAJ(x,y,z) (((x|y)&z)|(x&y))
#define SIG0(x)	(S(x,2) ^ S(x,13) ^ S(x,22))
#define SIG1(x)	(S(x,6) ^ S(x,11) ^ S(x,25))
#define sig0(x) (S(x,7) ^ S(x,18) ^ R(x,3))
#define sig1(x) (S(x,17) ^ S(x,19) ^ R(x,10))

#define ROUND(a,b,c,d,e,f,g,h,w,k)	\
	temp = h + SIG1(e) + CH(e,f,g) + k + w;	\
	h = temp + SIG0(a) + MAJ(a,b,c);	\
	d += temp

#ifndef ASM_SHA256PROCESS
void sha256Process(register sha256Param *p)
{
	register uint32 a, b, c, d, e, f, g, h, temp;
	register uint32 *w;
	register byte t;
	
	#if WORDS_BIGENDIAN
	w = p->data + 16;
	#else
	w = p->data;
	t = 16;
	while (t--)
	{
		register uint32 ttemp = swapu32(*w);
		*(w++) = ttemp;
	}
	#endif

	t = 48;
	while (t--)
	{
		register uint32 ttemp = sig1(w[-2]) + w[-7] + sig0(w[-15]) + w[-16];
		*(w++) = ttemp;
	}

	w = p->data;

	a = p->h[0]; b = p->h[1]; c = p->h[2]; d = p->h[3];
	e = p->h[4]; f = p->h[5]; g = p->h[6]; h = p->h[7];

	ROUND(a,b,c,d,e,f,g,h,w[ 0],k[ 0]);
	ROUND(h,a,b,c,d,e,f,g,w[ 1],k[ 1]);
	ROUND(g,h,a,b,c,d,e,f,w[ 2],k[ 2]);
	ROUND(f,g,h,a,b,c,d,e,w[ 3],k[ 3]);
	ROUND(e,f,g,h,a,b,c,d,w[ 4],k[ 4]);
	ROUND(d,e,f,g,h,a,b,c,w[ 5],k[ 5]);
	ROUND(c,d,e,f,g,h,a,b,w[ 6],k[ 6]);
	ROUND(b,c,d,e,f,g,h,a,w[ 7],k[ 7]);
	ROUND(a,b,c,d,e,f,g,h,w[ 8],k[ 8]);
	ROUND(h,a,b,c,d,e,f,g,w[ 9],k[ 9]);
	ROUND(g,h,a,b,c,d,e,f,w[10],k[10]);
	ROUND(f,g,h,a,b,c,d,e,w[11],k[11]);
	ROUND(e,f,g,h,a,b,c,d,w[12],k[12]);
	ROUND(d,e,f,g,h,a,b,c,w[13],k[13]);
	ROUND(c,d,e,f,g,h,a,b,w[14],k[14]);
	ROUND(b,c,d,e,f,g,h,a,w[15],k[15]);
	ROUND(a,b,c,d,e,f,g,h,w[16],k[16]);
	ROUND(h,a,b,c,d,e,f,g,w[17],k[17]);
	ROUND(g,h,a,b,c,d,e,f,w[18],k[18]);
	ROUND(f,g,h,a,b,c,d,e,w[19],k[19]);
	ROUND(e,f,g,h,a,b,c,d,w[20],k[20]);
	ROUND(d,e,f,g,h,a,b,c,w[21],k[21]);
	ROUND(c,d,e,f,g,h,a,b,w[22],k[22]);
	ROUND(b,c,d,e,f,g,h,a,w[23],k[23]);
	ROUND(a,b,c,d,e,f,g,h,w[24],k[24]);
	ROUND(h,a,b,c,d,e,f,g,w[25],k[25]);
	ROUND(g,h,a,b,c,d,e,f,w[26],k[26]);
	ROUND(f,g,h,a,b,c,d,e,w[27],k[27]);
	ROUND(e,f,g,h,a,b,c,d,w[28],k[28]);
	ROUND(d,e,f,g,h,a,b,c,w[29],k[29]);
	ROUND(c,d,e,f,g,h,a,b,w[30],k[30]);
	ROUND(b,c,d,e,f,g,h,a,w[31],k[31]);
	ROUND(a,b,c,d,e,f,g,h,w[32],k[32]);
	ROUND(h,a,b,c,d,e,f,g,w[33],k[33]);
	ROUND(g,h,a,b,c,d,e,f,w[34],k[34]);
	ROUND(f,g,h,a,b,c,d,e,w[35],k[35]);
	ROUND(e,f,g,h,a,b,c,d,w[36],k[36]);
	ROUND(d,e,f,g,h,a,b,c,w[37],k[37]);
	ROUND(c,d,e,f,g,h,a,b,w[38],k[38]);
	ROUND(b,c,d,e,f,g,h,a,w[39],k[39]);
	ROUND(a,b,c,d,e,f,g,h,w[40],k[40]);
	ROUND(h,a,b,c,d,e,f,g,w[41],k[41]);
	ROUND(g,h,a,b,c,d,e,f,w[42],k[42]);
	ROUND(f,g,h,a,b,c,d,e,w[43],k[43]);
	ROUND(e,f,g,h,a,b,c,d,w[44],k[44]);
	ROUND(d,e,f,g,h,a,b,c,w[45],k[45]);
	ROUND(c,d,e,f,g,h,a,b,w[46],k[46]);
	ROUND(b,c,d,e,f,g,h,a,w[47],k[47]);
	ROUND(a,b,c,d,e,f,g,h,w[48],k[48]);
	ROUND(h,a,b,c,d,e,f,g,w[49],k[49]);
	ROUND(g,h,a,b,c,d,e,f,w[50],k[50]);
	ROUND(f,g,h,a,b,c,d,e,w[51],k[51]);
	ROUND(e,f,g,h,a,b,c,d,w[52],k[52]);
	ROUND(d,e,f,g,h,a,b,c,w[53],k[53]);
	ROUND(c,d,e,f,g,h,a,b,w[54],k[54]);
	ROUND(b,c,d,e,f,g,h,a,w[55],k[55]);
	ROUND(a,b,c,d,e,f,g,h,w[56],k[56]);
	ROUND(h,a,b,c,d,e,f,g,w[57],k[57]);
	ROUND(g,h,a,b,c,d,e,f,w[58],k[58]);
	ROUND(f,g,h,a,b,c,d,e,w[59],k[59]);
	ROUND(e,f,g,h,a,b,c,d,w[60],k[60]);
	ROUND(d,e,f,g,h,a,b,c,w[61],k[61]);
	ROUND(c,d,e,f,g,h,a,b,w[62],k[62]);
	ROUND(b,c,d,e,f,g,h,a,w[63],k[63]);

	p->h[0] += a;
	p->h[1] += b;
	p->h[2] += c;
	p->h[3] += d;
	p->h[4] += e;
	p->h[5] += f;
	p->h[6] += g;
	p->h[7] += h;
}
#endif

int sha256Update(register sha256Param *p, const byte *data, int size)
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
			sha256Process(p);
			p->offset = 0;
		}
	}
	return 0;
}

static void sha256Finish(register sha256Param *p)
{
	register byte *ptr = ((byte *) p->data) + p->offset++;

	*(ptr++) = 0x80;

	if (p->offset > 56)
	{
		while (p->offset++ < 64)
			*(ptr++) = 0;

		sha256Process(p);
		p->offset = 0;
	}

	ptr = ((byte *) p->data) + p->offset;
	while (p->offset++ < 56)
		*(ptr++) = 0;

	#if WORDS_BIGENDIAN
	p->data[14] = ((uint32)(p->length >> 29));
	p->data[15] = ((uint32)((p->length << 3) & 0xffffffff));
	#else
	p->data[14] = swapu32((uint32)(p->length >> 29));
	p->data[15] = swapu32((uint32)((p->length << 3) & 0xffffffff));
	#endif

	sha256Process(p);
	p->offset = 0;
}

int sha256Digest(register sha256Param *p, uint32 *data)
{
	sha256Finish(p);
	mp32copy(8, data, p->h);
	(void) sha256Reset(p);
	return 0;
}
