/** \ingroup MP_m
 * \file mp32barrett.c
 *
 * Barrett modular reduction, code.
 *
 * For more information on this algorithm, see:
 * "Handbook of Applied Cryptography", Chapter 14.3.3
 *  Menezes, van Oorschot, Vanstone
 *  CRC Press
 */

/*
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

#include "mp32.h"
#include "mp32prime.h"
#include "mp32barrett.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

#include <stdio.h>

/**
 * mp32bzero
 */
void mp32bzero(mp32barrett* b)
{
	b->size = 0;
	b->modl = (uint32*) 0;
	b->mu = (uint32*) 0;
}

/*@-nullstate@*/	/* b->modl may be null @*/
/**
 *  Allocates the data words for an mp32barrett structure.
 *  will allocate 2*size+1 words
 */
void mp32binit(mp32barrett* b, uint32 size)
{
	b->size	= size;
	if (b->modl)
		free(b->modl);
	b->modl	= (uint32*) calloc(2*size+1, sizeof(uint32));

	if (b->modl != (uint32*) 0)
		b->mu = b->modl+size;
	else
		b->mu = (uint32*) 0;
}
/*@=nullstate@*/

/**
 * mp32bfree
 */
void mp32bfree(mp32barrett* b)
{
	if (b->modl != (uint32*) 0)
	{
		free(b->modl);
		b->modl = (uint32*) 0;
		b->mu = (uint32*) 0;
	}
	b->size = 0;
}

/*@-nullstate -compdef @*/	/* b->modl may be null @*/
void mp32bcopy(mp32barrett* b, const mp32barrett* copy)
{
	register uint32 size = copy->size;

	if (size)
	{
		if (b->modl)
		{
			if (b->size != size)
				b->modl = (uint32*) realloc(b->modl, (2*size+1) * sizeof(uint32));
		}
		else
			b->modl = (uint32*) malloc((2*size+1) * sizeof(uint32));

		if (b->modl)
		{
			b->size = size;
			b->mu = b->modl+copy->size;
			mp32copy(2*size+1, b->modl, copy->modl);
		}
		else
		{
			b->size = 0;
			b->mu = (uint32*) 0;
		}
	}
	else if (b->modl)
	{
		free(b->modl);
		b->size = 0;
		b->modl = (uint32*) 0;
		b->mu = (uint32*) 0;
	}
	else
		{};
}
/*@=nullstate =compdef @*/

/*@-nullstate -compdef @*/	/* b->modl may be null @*/
/**
 * mp32bset
 */
void mp32bset(mp32barrett* b, uint32 size, const uint32 *data)
{
	if (size > 0)
	{
		if (b->modl)
		{
			if (b->size != size)
				b->modl = (uint32*) realloc(b->modl, (2*size+1) * sizeof(uint32));
		}
		else
			b->modl = (uint32*) malloc((2*size+1) * sizeof(uint32));

		if (b->modl)
		{
			uint32* temp = (uint32*) malloc((6*size+4) * sizeof(uint32));

			b->size = size;
			b->mu = b->modl+size;
			mp32copy(size, b->modl, data);
			/*@-nullpass@*/		/* temp may be NULL */
			mp32bmu_w(b, temp);

			free(temp);
			/*@=nullpass@*/
		}
		else
		{
			b->size = 0;
			b->mu = (uint32*) 0;
		}
	}
}
/*@=nullstate =compdef @*/

/*@-nullstate -compdef @*/	/* b->modl may be null @*/
void mp32bsethex(mp32barrett* b, const char* hex)
{
	uint32 length = strlen(hex);
	uint32 size = (length+7) >> 3;
	uint8 rem = (uint8)(length & 0x7);

	if (b->modl)
	{
		if (b->size != size)
			b->modl = (uint32*) realloc(b->modl, (2*size+1) * sizeof(uint32));
	}
	else
		b->modl = (uint32*) malloc((2*size+1) * sizeof(uint32));

	if (b->modl != (uint32*) 0)
	{
		register uint32  val = 0;
		register uint32* dst = b->modl;
		register uint32* temp = (uint32*) malloc((6*size+4) * sizeof(uint32));
		register char ch;

		b->size = size;
		b->mu = b->modl+size;

		while (length-- > 0)
		{
			ch = *(hex++);
			val <<= 4;
			if (ch >= '0' && ch <= '9')
				val += (ch - '0');
			else if (ch >= 'A' && ch <= 'F')
				val += (ch - 'A') + 10;
			else if (ch >= 'a' && ch <= 'f')
				val += (ch - 'a') + 10;
			else
				{};

			if ((length & 0x7) == 0)
			{
				*(dst++) = val;
				val = 0;
			}
		}
		if (rem != 0)
			*dst = val;

		/*@-nullpass@*/		/* temp may be NULL */
		mp32bmu_w(b, temp);

		free(temp);
		/*@=nullpass@*/
	}
	else
	{
		b->size = 0;
		b->mu = 0;
	}
}
/*@=nullstate =compdef @*/

/**
 *  Computes the Barrett 'mu' coefficient.
 *  needs workspace of (6*size+4) words
 */
void mp32bmu_w(mp32barrett* b, uint32* wksp)
{
	register uint32  size = b->size;
	register uint32* divmod = wksp;
	register uint32* dividend = divmod+(size*2+2);
	register uint32* workspace = dividend+(size*2+1);
	register uint32  shift;

	/* normalize modulus before division */
	shift = mp32norm(size, b->modl);
	/* make the dividend, initialize first word to 1 (shifted); the rest is zero */
	*dividend = (uint32) (1 << shift);
	mp32zero(size*2, dividend+1);
	mp32ndivmod(divmod, size*2+1, dividend, size, b->modl, workspace);
	/*@-nullpass@*/ /* b->mu may be NULL */
	mp32copy(size+1, b->mu, divmod+1);
	/*@=nullpass@*/
	/* de-normalize */
	mp32rshift(size, b->modl, shift);
}

/**
 *  Generates a random number in the range 1 < r < b-1.
 *  need workspace of (size) words
 */
void mp32brnd_w(const mp32barrett* b, randomGeneratorContext* rc, uint32* result, uint32* wksp)
{
	uint32 msz = mp32mszcnt(b->size, b->modl);

	mp32copy(b->size, wksp, b->modl);
	(void) mp32subw(b->size, wksp, 1);

	do
	{
		/*@-noeffectuncon@*/ /* LCL: ??? */
		(void) rc->rng->next(rc->param, result, b->size);
		/*@=noeffectuncon@*/

		/*@-shiftsigned -usedef@*/
		result[0] &= (0xffffffff >> msz);
		/*@=shiftsigned =usedef@*/

		while (mp32ge(b->size, result, wksp))
			(void) mp32sub(b->size, result, wksp);
	} while (mp32leone(b->size, result));
}

/**
 *  Generates a random odd number in the range 1 < r < b-1.
 *  needs workspace of (size) words
 */
void mp32brndodd_w(const mp32barrett* b, randomGeneratorContext* rc, uint32* result, uint32* wksp)
{
	uint32 msz = mp32mszcnt(b->size, b->modl);

	mp32copy(b->size, wksp, b->modl);
	(void) mp32subw(b->size, wksp, 1);

	do
	{
		/*@-noeffectuncon@*/ /* LCL: ??? */
		(void) rc->rng->next(rc->param, result, b->size);
		/*@=noeffectuncon@*/

		/*@-shiftsigned -usedef@*/
		result[0] &= (0xffffffff >> msz);
		/*@=shiftsigned =usedef@*/
		mp32setlsb(b->size, result);

		while (mp32ge(b->size, result, wksp))
		{
			(void) mp32sub(b->size, result, wksp);
			mp32setlsb(b->size, result);
		}
	} while (mp32leone(b->size, result));
}

/**
 *  Generates a random invertible (modulo b) in the range 1 < r < b-1.
 *  needs workspace of (6*size+6) words
 */
void mp32brndinv_w(const mp32barrett* b, randomGeneratorContext* rc, uint32* result, uint32* inverse, uint32* wksp)
{
	register uint32  size = b->size;

	do
	{
		if (mp32even(size, b->modl))
			mp32brndodd_w(b, rc, result, wksp);
		else
			mp32brnd_w(b, rc, result, wksp);

	} while (mp32binv_w(b, size, result, inverse, wksp) == 0);
}

/**
 *  Computes the barrett modular reduction of a number x, which has twice the size of b.
 *  needs workspace of (2*size+2) words
 */
void mp32bmod_w(const mp32barrett* b, const uint32* xdata, uint32* result, uint32* wksp)
{
	register uint32 rc;
	register uint32 sp = 2;
	register const uint32* src = xdata+b->size+1;
	register       uint32* dst = wksp +b->size+1;

	/*@-nullpass@*/ /* b->mu may be NULL */
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
	/*@=nullpass@*/

	/* q3 is one word larger than b->modl */
	/* r2 is (2*size+1) words, of which we only needs the (size+1) lsw's */

	sp = b->size;
	rc = 0;

	dst = wksp+b->size+1;
	src = dst;

	/*@-evalorder@*/ /* --src side effect, dst/src aliases */
	*dst = mp32setmul(sp, dst+1, b->modl, *(--src));
	/*@=evalorder@*/

	while (sp > 0)
	{
		(void) mp32addmul(sp--, dst, b->modl+(rc++), *(--src));
	}

	mp32setx(b->size+1, wksp, b->size*2, xdata);
	(void) mp32sub(b->size+1, wksp, wksp+b->size+1);

	while (mp32gex(b->size+1, wksp, b->size, b->modl))
	{
		(void) mp32subx(b->size+1, wksp, b->size, b->modl);
	}
	mp32copy(b->size, result, wksp+1);
}

/**
 *  Copies (b-1) into result.
 */
void mp32bsubone(const mp32barrett* b, uint32* result)
{
	register uint32 size = b->size;

	mp32copy(size, result, b->modl);
	(void) mp32subw(size, result, 1);
}

/**
 *  Computes the negative (modulo b) of x, where x must contain a value between 0 and b-1.
 */
void mp32bneg(const mp32barrett* b, const uint32* xdata, uint32* result)
{
	register uint32  size = b->size;

	mp32copy(size, result, xdata);
	mp32neg(size, result);
	(void) mp32add(size, result, b->modl);
}

/**
 *  Computes the sum (modulo b) of x and y.
 *  needs a workspace of (4*size+2) words
 */
void mp32baddmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, uint32* result, uint32* wksp)
{
	/* xsize and ysize must be less than or equal to b->size */
	register uint32  size = b->size;
	register uint32* temp = wksp + size*2+2;

	mp32setx(2*size, temp, xsize, xdata);
	(void) mp32addx(2*size, temp, ysize, ydata);

	mp32bmod_w(b, temp, result, wksp);
}

/**
 *  Computes the difference (modulo b) of x and y.
 *  needs a workspace of (4*size+2) words
 */
void mp32bsubmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, uint32* result, uint32* wksp)
{
	/* xsize and ysize must be less than or equal to b->size */
	register uint32  size = b->size;
	register uint32* temp = wksp + size*2+2;
	
	mp32setx(2*size, temp, xsize, xdata);
	if (mp32subx(2*size, temp, ysize, ydata)) /* if there's carry, i.e. the result would be negative, add the modulus */
		(void) mp32addx(2*size, temp, size, b->modl);

	mp32bmod_w(b, temp, result, wksp);
}

/**
 *  Computes the product (modulo b) of x and y.
 *  needs a workspace of (4*size+2) words
 */
void mp32bmulmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, uint32* result, uint32* wksp)
{
	/* xsize and ysize must be <= b->size */
	register uint32  size = b->size;
	register uint32* temp = wksp + size*2+2;
	register uint32  fill = size*2-xsize-ysize;

	if (fill)
		mp32zero(fill, temp);

	mp32mul(temp+fill, xsize, xdata, ysize, ydata);
	mp32bmod_w(b, temp, result, wksp);
}

/**
 *  Computes the square (modulo b) of x.
 *  needs a workspace of (4*size+2) words
 */
void mp32bsqrmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32* result, uint32* wksp)
{
	/* xsize must be <= b->size */
	register uint32  size = b->size;
	register uint32* temp = wksp + size*2+2;
	register uint32  fill = 2*(size-xsize);

	if (fill)
		mp32zero(fill, temp);

	mp32sqr(temp+fill, xsize, xdata);
	mp32bmod_w(b, temp, result, wksp);
}

/**
 *  Precomputes the sliding window table for computing powers of x modulo b.
 *  needs workspace (4*size+2)
 *
 * Sliding Window Exponentiation technique, slightly altered from the method Applied Cryptography:
 *
 * First of all, the table with the powers of g can be reduced by about half; the even powers don't
 * need to be accessed or stored.
 *
 * Get up to K bits starting with a one, if we have that many still available
 *
 * Do the number of squarings of A in the first column, the multiply by the value in column two,
 * and finally do the number of squarings in column three.
 *
 * This table can be used for K=2,3,4 and can be extended
 *  
 *
\verbatim
	   0 : - | -       | -
	   1 : 1 |  g1 @ 0 | 0
	  10 : 1 |  g1 @ 0 | 1
	  11 : 2 |  g3 @ 1 | 0
	 100 : 1 |  g1 @ 0 | 2
	 101 : 3 |  g5 @ 2 | 0
	 110 : 2 |  g3 @ 1 | 1
	 111 : 3 |  g7 @ 3 | 0
	1000 : 1 |  g1 @ 0 | 3
	1001 : 4 |  g9 @ 4 | 0
	1010 : 3 |  g5 @ 2 | 1
	1011 : 4 | g11 @ 5 | 0
	1100 : 2 |  g3 @ 1 | 2
	1101 : 4 | g13 @ 6 | 0
	1110 : 3 |  g7 @ 3 | 1
	1111 : 4 | g15 @ 7 | 0
\endverbatim
 *
 */
static void mp32bslide_w(const mp32barrett* b, const uint32 xsize, const uint32* xdata, /*@out@*/ uint32* slide, /*@out@*/ uint32* wksp)
	/*@modifies slide, wksp @*/
{
	register uint32 size = b->size;
	mp32bsqrmod_w(b, xsize, xdata,                     slide       , wksp); /* x^2 mod b, temp */
	mp32bmulmod_w(b, xsize, xdata, size, slide       , slide+size  , wksp); /* x^3 mod b */
	mp32bmulmod_w(b,  size, slide, size, slide+size  , slide+2*size, wksp); /* x^5 mod b */
	mp32bmulmod_w(b,  size, slide, size, slide+2*size, slide+3*size, wksp); /* x^7 mod b */
	mp32bmulmod_w(b,  size, slide, size, slide+3*size, slide+4*size, wksp); /* x^9 mod b */
	mp32bmulmod_w(b,  size, slide, size, slide+4*size, slide+5*size, wksp); /* x^11 mod b */
	mp32bmulmod_w(b,  size, slide, size, slide+5*size, slide+6*size, wksp); /* x^13 mod b */
	mp32bmulmod_w(b,  size, slide, size, slide+6*size, slide+7*size, wksp); /* x^15 mod b */
	mp32setx(size, slide, xsize, xdata);                                    /* x^1 mod b */
}

/*@observer@*/ static byte mp32bslide_presq[16] = 
{ 0, 1, 1, 2, 1, 3, 2, 3, 1, 4, 3, 4, 2, 4, 3, 4 };

/*@observer@*/ static byte mp32bslide_mulg[16] =
{ 0, 0, 0, 1, 0, 2, 1, 3, 0, 4, 2, 5, 1, 6, 3, 7 };

/*@observer@*/ static byte mp32bslide_postsq[16] =
{ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 };

/**
 * mp32bpowmod_w
 * needs workspace of 4*size+2 words
 */
void mp32bpowmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 psize, const uint32* pdata, uint32* result, uint32* wksp)
{
	/*
	 * Modular exponention
	 *
	 * Uses sliding window exponentiation; needs extra storage: if K=3, needs 8*size, if K=4, needs 16*size
	 *
	 */

	/* K == 4 for the first try */
	
	uint32  size = b->size;
	uint32  temp = 0;

	while (psize)
	{
		if ((temp = *(pdata++))) /* break when first non-zero word found */
			break;
		psize--;
	}

	/* if temp is still zero, then we're trying to raise x to power zero, and result stays one */
	if (temp)
	{
		uint32* slide = (uint32*) malloc((8*size)*sizeof(uint32));

		/*@-nullpass@*/		/* slide may be NULL */
		mp32bslide_w(b, xsize, xdata, slide, wksp);

		/*@-internalglobs -mods@*/ /* noisy */
		mp32bpowmodsld_w(b, slide, psize, pdata-1, result, wksp);
		/*@=internalglobs =mods@*/

		free(slide);
		/*@=nullpass@*/
	}
}

void mp32bpowmodsld_w(const mp32barrett* b, const uint32* slide, uint32 psize, const uint32* pdata, uint32* result, uint32* wksp)
{
	/*
	 * Modular exponentiation with precomputed sliding window table, so no x is required
	 *
	 */

	uint32 size = b->size;
	uint32 temp = 0;

	mp32setw(size, result, 1);

	while (psize)
	{
		if ((temp = *(pdata++))) /* break when first non-zero word found in power */
			break;
		psize--;
	}

	/*@+charindex@*/
	/* if temp is still zero, then we're trying to raise x to power zero, and result stays one */
	if (temp)
	{
		uint8 l = 0, n = 0, count = 32;

		/* first skip bits until we reach a one */
		while (count != 0)
		{
			if (temp & 0x80000000)
				break;
			temp <<= 1;
			count--;
		}

		while (psize)
		{
			while (count != 0)
			{
				uint8 bit = (temp & 0x80000000) != 0;

				n <<= 1;
				n += bit;
				
				if (n != 0)
				{
					if (l != 0)
						l++;
					else if (bit != 0)
						l = 1;
					else
						{};

					if (l == 4)
					{
						uint8 s = mp32bslide_presq[n];
						
						while (s--)
							mp32bsqrmod_w(b, size, result, result, wksp);
						
						mp32bmulmod_w(b, size, result, size, slide+mp32bslide_mulg[n]*size, result, wksp);
						
						s = mp32bslide_postsq[n];
						
						while (s--)
							mp32bsqrmod_w(b, size, result, result, wksp);

						l = n = 0;
					}
				}
				else
					mp32bsqrmod_w(b, size, result, result, wksp);

				temp <<= 1;
				count--;
			}
			if (--psize)
			{
				count = 32;
				temp = *(pdata++);
			}
		}

		if (n != 0)
		{
			uint8 s = mp32bslide_presq[n];
			while (s--)
				mp32bsqrmod_w(b, size, result, result, wksp);
				
			mp32bmulmod_w(b, size, result, size, slide+mp32bslide_mulg[n]*size, result, wksp);
			
			s = mp32bslide_postsq[n];
			
			while (s--)
				mp32bsqrmod_w(b, size, result, result, wksp);
		}
	}	
	/*@=charindex@*/
}

/**
 * mp32btwopowmod_w
 *  needs workspace of (4*size+2) words
 */
void mp32btwopowmod_w(const mp32barrett* b, uint32 psize, const uint32* pdata, uint32* result, uint32* wksp)
{
	/*
	 * Modular exponention, 2^p mod modulus, special optimization
	 *
	 * Uses left-to-right exponentiation; needs no extra storage
	 *
	 */

	/* this routine calls mp32bmod, which needs (size*2+2), this routine needs (size*2) for sdata */

	register uint32 size = b->size;
	register uint32 temp = 0;

	mp32setw(size, result, 1);

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

		while (psize--)
		{
			while (count)
			{
				/* always square */
				mp32bsqrmod_w(b, size, result, result, wksp);
				
				/* multiply by two if bit is 1 */
				if (temp & 0x80000000)
				{
					if (mp32add(size, result, result) || mp32ge(size, result, b->modl))
					{
						/* there was carry, or the result is greater than the modulus, so we need to adjust */
						(void) mp32sub(size, result, b->modl);
					}
				}

				temp <<= 1;
				count--;
			}
			count = 32;
			temp = *(pdata++);
		}
	}
}

/**
 *  Computes the inverse (modulo b) of x, and returns 1 if x was invertible.
 *  needs workspace of (6*size+6) words
 *  @note xdata and result cannot point to the same area
 */
int mp32binv_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32* result, uint32* wksp)
{
	/*
	 * Fact: if a element of Zn, then a is invertible if and only if gcd(a,n) = 1
	 * Hence: if b->modl is even, then x must be odd, otherwise the gcd(x,n) >= 2
	 *
	 * The calling routine must guarantee this condition.
	 */

	register uint32  size = b->size;

	uint32* udata = wksp;
	uint32* vdata = udata+size+1;
	uint32* adata = vdata+size+1;
	uint32* bdata = adata+size+1;
	uint32* cdata = bdata+size+1;
	uint32* ddata = cdata+size+1;

	if (mp32odd(b->size, b->modl) && mp32even(xsize, xdata))
	{
		/* use simplified binary extended gcd algorithm */
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
					(void) mp32subx(size+1, bdata, size, b->modl);

				mp32sdivtwo(size+1, bdata);
			}
			while (mp32even(size+1, vdata))
			{
				mp32divtwo(size+1, vdata);

				if (mp32odd(size+1, ddata))
					(void) mp32subx(size+1, ddata, size, b->modl);

				mp32sdivtwo(size+1, ddata);
			}
			if (mp32ge(size+1, udata, vdata))
			{
				(void) mp32sub(size+1, udata, vdata);
				(void) mp32sub(size+1, bdata, ddata);
			}
			else
			{
				(void) mp32sub(size+1, vdata, udata);
				(void) mp32sub(size+1, ddata, bdata);
			}

			if (mp32z(size+1, udata))
			{
				if (mp32isone(size+1, vdata))
				{
					if (result)
					{
						mp32setx(size, result, size+1, ddata);
						/*@-usedef@*/
						if (*ddata & 0x80000000)
							(void) mp32add(size, result, b->modl);
						/*@=usedef@*/
					}
					return 1;
				}
				return 0;
			}
		}
	}
	else
	{
		/* use full binary extended gcd algorithm */
		mp32setx(size+1, udata, size, b->modl);
		mp32setx(size+1, vdata, xsize, xdata);
		mp32setw(size+1, adata, 1);
		mp32zero(size+1, bdata);
		mp32zero(size+1, cdata);
		mp32setw(size+1, ddata, 1);

		while (1)
		{
			while (mp32even(size+1, udata))
			{
				mp32divtwo(size+1, udata);

				if (mp32odd(size+1, adata) || mp32odd(size+1, bdata))
				{
					(void) mp32addx(size+1, adata, xsize, xdata);
					(void) mp32subx(size+1, bdata, size, b->modl);
				}

				mp32sdivtwo(size+1, adata);
				mp32sdivtwo(size+1, bdata);
			}
			while (mp32even(size+1, vdata))
			{
				mp32divtwo(size+1, vdata);

				if (mp32odd(size+1, cdata) || mp32odd(size+1, ddata))
				{
					(void) mp32addx(size+1, cdata, xsize, xdata);
					(void) mp32subx(size+1, ddata, size, b->modl);
				}

				mp32sdivtwo(size+1, cdata);
				mp32sdivtwo(size+1, ddata);
			}
			if (mp32ge(size+1, udata, vdata))
			{
				(void) mp32sub(size+1, udata, vdata);
				(void) mp32sub(size+1, adata, cdata);
				(void) mp32sub(size+1, bdata, ddata);
			}
			else
			{
				(void) mp32sub(size+1, vdata, udata);
				(void) mp32sub(size+1, cdata, adata);
				(void) mp32sub(size+1, ddata, bdata);
			}

			if (mp32z(size+1, udata))
			{
				if (mp32isone(size+1, vdata))
				{
					if (result)
					{
						mp32setx(size, result, size+1, ddata);
						/*@-usedef@*/
						if (*ddata & 0x80000000)
							(void) mp32add(size, result, b->modl);
						/*@=usedef@*/
					}
					return 1;
				}
				return 0;
			}
		}
	}
}

/**
 * needs workspace of (7*size+2) words
 */
int mp32bpprime_w(const mp32barrett* b, randomGeneratorContext* rc, int t, uint32* wksp)
{
	/*
	 * This test works for candidate probable primes >= 3, which are also not small primes.
	 *
	 * It assumes that b->modl contains the candidate prime
	 *
	 */

	uint32 size = b->size;

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
		
		if (size > SMALL_PRIMES_PRODUCT_MAX)
		{
			mp32setx(size, wksp+size, SMALL_PRIMES_PRODUCT_MAX, mp32spprod[SMALL_PRIMES_PRODUCT_MAX-1]);
			/*@-compdef@*/ /* LCL: wksp+size */
			mp32gcd_w(size, b->modl, wksp+size, wksp, wksp+2*size);
			/*@=compdef@*/
		}
		else
		{
			mp32gcd_w(size, b->modl, mp32spprod[size-1], wksp, wksp+2*size);
		}

		if (mp32isone(size, wksp))
		{
			return mp32pmilrab_w(b, rc, t, wksp);
		}
	}

	return 0;
}

void mp32bnrnd(const mp32barrett* b, randomGeneratorContext* rc, mp32number* result)
{
	register uint32  size = b->size;
	register uint32* temp = (uint32*) malloc(size * sizeof(uint32));

	mp32nfree(result);
	mp32nsize(result, size);
	/*@-nullpass@*/		/* temp may be NULL */
	/*@-usedef@*/		/* result->data unallocated? */
	mp32brnd_w(b, rc, result->data, temp);
	/*@=usedef@*/

	free(temp);
	/*@=nullpass@*/
}

void mp32bnmulmod(const mp32barrett* b, const mp32number* x, const mp32number* y, mp32number* result)
{
	register uint32  size = b->size;
	register uint32* temp = (uint32*) malloc((4*size+2) * sizeof(uint32));

	/* xsize and ysize must be <= b->size */
	register uint32  fill = 2*size-x->size-y->size;
	register uint32* opnd = temp+size*2+2;

	mp32nfree(result);
	mp32nsize(result, size);

	if (fill)
		mp32zero(fill, opnd);

	mp32mul(opnd+fill, x->size, x->data, y->size, y->data);
	/*@-nullpass@*/		/* temp may be NULL */
	/*@-usedef@*/		/* result->data unallocated? */
	mp32bmod_w(b, opnd, result->data, temp);
	/*@=usedef@*/

	free(temp);
	/*@=nullpass@*/
}

void mp32bnsqrmod(const mp32barrett* b, const mp32number* x, mp32number* result)
{
	register uint32  size = b->size;
	register uint32* temp = (uint32*) malloc(size * sizeof(uint32));

	/* xsize must be <= b->size */
	register uint32  fill = 2*(size-x->size);
	register uint32* opnd = temp + size*2+2;

	mp32nfree(result);
	mp32nsize(result, size);

	if (fill)
		mp32zero(fill, opnd);

	mp32sqr(opnd+fill, x->size, x->data);
	/*@-nullpass@*/		/* temp may be NULL */
	/*@-usedef@*/		/* result->data unallocated? */
	mp32bmod_w(b, opnd, result->data, temp);
	/*@=usedef@*/

	free(temp);
	/*@=nullpass@*/
}

void mp32bnpowmod(const mp32barrett* b, const mp32number* x, const mp32number* pow, mp32number* y)
{
	register uint32  size = b->size;
	register uint32* temp = (uint32*) malloc((4*size+2) * sizeof(uint32));

	mp32nfree(y);
	mp32nsize(y, size);

	/*@-nullpass@*/		/* temp may be NULL */
	mp32bpowmod_w(b, x->size, x->data, pow->size, pow->data, y->data, temp);

	free(temp);
	/*@=nullpass@*/
}

void mp32bnpowmodsld(const mp32barrett* b, const uint32* slide, const mp32number* pow, mp32number* y)
{
	register uint32  size = b->size;
	register uint32* temp = (uint32*) malloc((4*size+2) * sizeof(uint32));

	mp32nfree(y);
	mp32nsize(y, size);

	/*@-nullpass@*/		/* temp may be NULL */
	/*@-internalglobs -mods@*/ /* noisy */
	mp32bpowmodsld_w(b, slide, pow->size, pow->data, y->data, temp);
	/*@=internalglobs =mods@*/

	free(temp);
	/*@=nullpass@*/
}
