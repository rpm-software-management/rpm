/*
 * mp32barrett.c
 *
 * Barrett modular reduction, code
 *
 * For more information on this algorithm, see:
 * "Handbook of Applied Cryptography", Chapter 14.3.3
 *  Menezes, van Oorschot, Vanstone
 *  CRC Press
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

#include "mp32.h"
#include "mp32prime.h"
#include "mp32barrett.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdio.h>

void mp32bmu(mp32barrett* b)
{
	/* workspace needs to acommodate the dividend (size*2+1), and the divmod result (size*2+1) */
	register uint32  size = b->size;
	register uint32* divmod = b->mu-1; /* uses the last word of b->modl, which we made large enough */
	register uint32* dividend = divmod+(size*2+2);
	register uint32* workspace = dividend+(size*2+1);
	register uint32  shift;

	/* normalize modulus before division */
	shift = mp32norm(size, b->modl);
	/* make the dividend, initialize first word to 1 (shifted); the rest is zero */
	*dividend = (1 << shift);
	mp32zero(size*2, dividend+1);
	mp32ndivmod(divmod, size*2+1, dividend, size, b->modl, workspace);
	/* de-normalize */
	mp32rshift(size, b->modl, shift);
}

void mp32brndres(const mp32barrett* b, uint32* result, randomGeneratorContext* rc)
{
	uint32 msz = mp32mszcnt(b->size, b->modl);

	mp32copy(b->size, b->wksp, b->modl);
	mp32subw(b->size, b->wksp, 1);

	do
	{
		rc->rng->next(rc->param, result, b->size);

		result[0] &= (0xffffffff >> msz);

		while (mp32ge(b->size, result, b->wksp))
			mp32sub(b->size, result, b->wksp);
	} while (mp32leone(b->size, result));
}

void mp32bmodres(const mp32barrett* b, uint32* result, const uint32* xdata)
{
	register uint32 rc;
	register uint32 sp = 2;
	register const uint32* src = xdata+b->size+1;
	register       uint32* dst = b->wksp+b->size+1;

	rc = mp32setmul(sp, dst, b->mu, *(--src));
	*(--dst) = rc;

	while (sp <= b->size)
	{
		sp++;
		if ((rc = *(--src)))
		{
			rc = mp32addmul(sp, dst, b->mu, rc);
			*(--dst) = rc;
		}
		else
			*(--dst) = 0;
	}
	if ((rc = *(--src)))
	{
		rc = mp32addmul(sp, dst, b->mu, rc);
		*(--dst) = rc;
	}
	else
		*(--dst) = 0;

	/* q3 is one word larger than b->modl */
	/* r2 is (2*size+1) words, of which we only needs the (size+1) lsw's */

	sp = b->size;
	rc = 0;

	dst = b->wksp+b->size+1;
	src = dst;

	*dst = mp32setmul(sp, dst+1, b->modl, *(--src));

	while (sp > 0)
	{
		mp32addmul(sp--, dst, b->modl+(rc++), *(--src));
	}

	mp32setx(b->size+1, b->wksp, b->size*2, xdata);
	mp32sub(b->size+1, b->wksp, b->wksp+b->size+1);
	while (mp32gex(b->size+1, b->wksp, b->size, b->modl))
	{
		mp32subx(b->size+1, b->wksp, b->size, b->modl);
	}
	mp32copy(b->size, result, b->wksp+1);
}

void mp32binit(mp32barrett* b, uint32 size)
{
	/* data, modulus and mu take 3*size+2 words, wksp needed = 7*size+2; total = 10*size+4 */
	b->size	= size;
	b->data	= (uint32*) calloc(size*10+4, sizeof(uint32));

	if (b->data)
	{
		b->modl = b->data+size+0;
		b->mu   = b->modl+size+1;
		b->wksp	= b->mu  +size+1;
	}
	else
	{
		b->modl = b->mu = b->wksp = (uint32*) 0;
	}
}

void mp32bzero(mp32barrett* b)
{
	b->size = 0;
	b->data = b->modl = b->mu = b->wksp = (uint32*) 0;
}

void mp32bfree(mp32barrett* b)
{
	if (b->data)
	{
		free(b->data);
		b->data = b->modl = b->mu = b->wksp = (uint32*) 0;
	}
	b->size = 0;
}

void mp32bset(mp32barrett* b, uint32 size, const uint32 *data)
{
	/* assumes that the msw of data is not zero */
	if (b->data)
		mp32bfree(b);

	if (size)
	{
		mp32binit(b, size);

		if (b->data)
		{
			mp32copy(size, b->modl, data);
			mp32bmu(b);
		}
	}
}

/* function mp32bsethex would be very useful! */

void mp32bmod(const mp32barrett* b, uint32 xsize, const uint32* xdata)
{
	register uint32  size = b->size;
	register uint32* opnd = b->wksp + size*2+2;

	mp32setx(size*2, opnd, xsize, xdata);
	mp32bmodres(b, b->data, opnd);
}

void mp32bmodsubone(const mp32barrett* b)
{
	register uint32 size = b->size;

	mp32copy(size, b->data, b->modl);
	mp32subw(size, b->data, 1);
}

void mp32bneg(const mp32barrett* b)
{
	register uint32  size = b->size;

	mp32neg(size, b->data);
	mp32add(size, b->data, b->modl);
}

void mp32baddmod(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
{
	register uint32  size = b->size;
	register uint32* opnd = b->wksp+size*2+2;

	mp32setx(2*size, opnd, xsize, xdata);
	mp32addx(2*size, opnd, ysize, ydata);

	mp32bmodres(b, b->data, opnd);
}

void mp32bmulmodres(const mp32barrett* b, uint32* result, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
{
	/* needs workspace of (size*2) in addition to what is needed by mp32bmodres (size*2+2) */
	/* xsize and ysize must be <= b->size */
	/* stores result in b->data */
	register uint32  size = b->size;
	register uint32  fill = 2*size-xsize-ysize;
	register uint32* opnd = b->wksp+size*2+2;

	if (fill)
		mp32zero(fill, opnd);

	mp32mul(opnd+fill, xsize, xdata, ysize, ydata);
	mp32bmodres(b, result, opnd);
}

void mp32bsqrmodres(const mp32barrett* b, uint32* result, uint32 xsize, const uint32* xdata)
{
	/* needs workspace of (size*2) in addition to what is needed by mp32bmodres (size*2+2) */
	/* xsize must be <= b->size */
	register uint32  size = b->size;
	register uint32  fill = 2*(size-xsize);
	register uint32* opnd = b->wksp + size*2+2;

	if (fill)
		mp32zero(fill, opnd);

	mp32sqr(opnd+fill, xsize, xdata);
	mp32bmodres(b, result, opnd);
}

void mp32bmulmod(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
{
	mp32bmulmodres(b, b->data, xsize, xdata, ysize, ydata);
}

void mp32bsqrmod(const mp32barrett* b, uint32 xsize, const uint32* xdata)
{
	mp32bsqrmodres(b, b->data, xsize, xdata);
}

void mp32bpowmod(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 psize, const uint32* pdata)
{
	/*
	 * Modular exponention
	 *
	 * Uses left-to-right exponentiation; needs no extra storage
	 *
	 */
	
	/* this routine calls mp32bmod, which needs (size*2+2), this routine needs (size*2) for sdata */

	register uint32  temp;

	mp32setw(b->size, b->data, 1);

	while (psize)
	{
		if ((temp = *(pdata++))) /* break when first non-zero word found */
			break;
		psize--;
	}

	/* if temp is still zero, then we're trying to raise x to power zero, and result stays one */
	if (temp)
	{
		register int count = 32;

		/* first skip bits until we reach a one */
		while (count)
		{
			if (temp & 0x80000000)
				break;
			temp <<= 1;
			count--;
		}

		while (psize)
		{
			while (count)
			{
				/* always square */
				mp32bnsqrmodres(b, b->data, (mp32number*) b);
				
				/* multiply by x if bit is 1 */
				if (temp & 0x80000000)
					mp32bmulmod(b, xsize, xdata, b->size, b->data);

				temp <<= 1;
				count--;
			}
			if (--psize)
			{
				count = 32;
				temp = *(pdata++);
			}
		}
	}
}

void mp32btwopowmod(const mp32barrett* b, uint32 psize, const uint32* pdata)
{
	/*
	 * Modular exponention, 2^p mod modulus, special optimization
	 *
	 * Uses left-to-right exponentiation; needs no extra storage
	 *
	 */

	/* this routine calls mp32bmod, which needs (size*2+2), this routine needs (size*2) for sdata */

	register uint32  temp;

	mp32setw(b->size, b->data, 1);

	while (psize)
	{
		if ((temp = *(pdata++))) /* break when first non-zero word found */
			break;
		psize--;
	}

	/* if temp is still zero, then we're trying to raise x to power zero, and result stays one */
	if (temp)
	{
		register int count = 32;

		/* first skip bits until we reach a one */
		while (count)
		{
			if (temp & 0x80000000)
				break;
			temp <<= 1;
			count--;
		}

		while (psize)
		{
			while (count)
			{
				/* always square */
				mp32bnsqrmodres(b, b->data, (mp32number*) b);
				
				/* multiply by two if bit is 1 */
				if (temp & 0x80000000)
				{
					if (mp32add(b->size, b->data, b->data) || mp32ge(b->size, b->data, b->modl))
					{
						/* there was carry, or the result is greater than the modulus, so we need to adjust */
						mp32sub(b->size, b->data, b->modl);
					}
				}

				temp <<= 1;
				count--;
			}
			if (psize--)
			{
				count = 32;
				temp = *(pdata++);
			}
		}
	}
}

int mp32binv(const mp32barrett* b, uint32 xsize, const uint32* xdata)
{
	/*
	 * Fact: if a element of Zn, then a is invertible if and only if gcd(a,n) = 1
	 * 
	 */

	/* where x or modl is odd, that algorithm will need (4*size+4) */

	if (mp32odd(b->size, b->modl))
	{
		/* use simplified binary extended gcd algorithm */

		register uint32  size = b->size;

		uint32* udata = b->wksp;
		uint32* vdata = udata+size+1;
		uint32* bdata = vdata+size+1;
		uint32* ddata = bdata+size+1;

		mp32setx(size+1, udata, size, b->modl);
		mp32setx(size+1, vdata, xsize, xdata);
		mp32zero(size+1, bdata);
		mp32setw(size+1, ddata, 1);

		while (1)
		{
			while (mp32even(size+1, udata))
			{
				mp32divtwo(size+1, udata);

				if (mp32odd(size+1, bdata))
					mp32subx(size+1, bdata, size, b->modl);

				mp32sdivtwo(size+1, bdata);
			}
			while (mp32even(size+1, vdata))
			{
				mp32divtwo(size+1, vdata);

				if (mp32odd(size+1, ddata))
					mp32subx(size+1, ddata, size, b->modl);

				mp32sdivtwo(size+1, ddata);
			}
			if (mp32ge(size+1, udata, vdata))
			{
				mp32sub(size+1, udata, vdata);
				mp32sub(size+1, bdata, ddata);
			}
			else
			{
				mp32sub(size+1, vdata, udata);
				mp32sub(size+1, ddata, bdata);
			}

			if (mp32z(size+1, udata))
			{
				if (mp32isone(size+1, vdata))
				{
					mp32setx(size, b->data, size+1, ddata);
					if (*ddata & 0x80000000)
						mp32add(size, b->data, b->modl);

					return 1;
				}
				return 0;
			}
		}
	}
	else
	{
		/*
		 * If x is even, then it is not invertible
		 *
		 */

		if (mp32even(xsize, xdata))
			return 0;

		/* use simplified binary extended gcd algorithm */
		
		/* INCOMPLETE */
		return 0;
	}
}

int mp32bpprime(const mp32barrett* b, randomGeneratorContext* r, int t)
{
	/*
	 * This test works for candidate probable primes >= 3, which are also not small primes 
	 *
	 * It assumes that b->modl contains the candidate prime
	 *
	 */

	/* first test if modl is odd */

	if (mp32odd(b->size, b->modl))
	{
		/*
		 * Small prime factor test:
		 * 
		 * Tables in mp32spprod contain multi-precision integers with products of small primes
		 * If the greatest common divisor of this product and the candidate is not one, then
		 * the candidate has small prime factors, or is a small prime. Neither is acceptable when
		 * we are looking for large probable primes =)
		 *
		 */
		
		if (b->size > SMALL_PRIMES_PRODUCT_MAX)
		{
			mp32setx(b->size, b->wksp+b->size, SMALL_PRIMES_PRODUCT_MAX, mp32spprod[SMALL_PRIMES_PRODUCT_MAX-1]);
			mp32gcd(b->data, b->size, b->modl, b->wksp+b->size, b->wksp);
		}
		else
		{
			mp32gcd(b->data, b->size, b->modl, mp32spprod[b->size-1], b->wksp);
		}

		if (mp32isone(b->size, b->data))
		{
			return mp32pmilrab(b, r, t);
		}
	}

	return 0;
}

void mp32brnd(const mp32barrett* b, randomGeneratorContext* rc)
{
	mp32brndres(b, b->data, rc);
}

void mp32bnmulmodres(const mp32barrett* b, uint32* result, const mp32number* x, const mp32number* y)
{
	/* needs workspace of (size*2) in addition to what is needed by mp32bmodres (size*2+2) */
	/* xsize and ysize must be <= b->size */
	/* stores result in b->data */
	register uint32  size = b->size;
	register uint32  fill = 2*size-x->size-y->size;
	register uint32* opnd = b->wksp+size*2+2;

	if (fill)
		mp32zero(fill, opnd);

	mp32mul(opnd+fill, x->size, x->data, y->size, y->data);
	mp32bmodres(b, result, opnd);
}

void mp32bnsqrmodres(const mp32barrett* b, uint32* result, const mp32number* x)
{
	/* needs workspace of (size*2) in addition to what is needed by mp32bmodres (size*2+2) */
	/* xsize must be <= b->size */
	register uint32  size = b->size;
	register uint32  fill = 2*(size-x->size);
	register uint32* opnd = b->wksp + size*2+2;

	if (fill)
		mp32zero(fill, opnd);

	mp32sqr(opnd+fill, x->size, x->data);
	mp32bmodres(b, result, opnd);
}

void mp32bnmulmod(const mp32barrett* b, const mp32number* x, const mp32number* y)
{
	mp32bnmulmodres(b, b->data, x, y);
}

void mp32bnpowmod(const mp32barrett* b, const mp32number* x, const mp32number* y)
{
	mp32bpowmod(b, x->size, x->data, y->size, y->data);
}

void mp32bnsqrmod(const mp32barrett* b, const mp32number* x)
{
	mp32bnsqrmodres(b, b->data, x);
}

void mp32bspowmod3(const mp32number* b, const uint32* x0, const uint32* p0, const uint32* x1, const uint32* p1, const uint32* x2, const uint32* p2)
{
	/* this algorithm needs (size*8) storage, which won't fit in the normal buffer */
}
