/*@-sizeoftype -type@*/
/** \ingroup MP_m
 * \file mpbarrett.c
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

#include "system.h"
#include "mp.h"
#include "mpprime.h"
#include "mpbarrett.h"
#include "debug.h"

/**
 * mpbzero
 */
void mpbzero(mpbarrett* b)
{
	b->size = 0;
	b->modl = (mpw*) 0;
	b->mu = (mpw*) 0;
}

/*@-nullstate@*/	/* b->modl may be null @*/
/**
 *  Allocates the data words for an mpbarrett structure.
 *  will allocate 2*size+1 words
 */
void mpbinit(mpbarrett* b, size_t size)
{
	b->size	= size;
	if (b->modl)
		free(b->modl);
	b->modl	= (mpw*) calloc(2*size+1, sizeof(*b->modl));

	if (b->modl != (mpw*) 0)
		b->mu = b->modl+size;
	else
		b->mu = (mpw*) 0;
}
/*@=nullstate@*/

/**
 * mpbfree
 */
void mpbfree(mpbarrett* b)
{
	if (b->modl != (mpw*) 0)
	{
		free(b->modl);
		b->modl = (mpw*) 0;
		b->mu = (mpw*) 0;
	}
	b->size = 0;
}

/*@-boundswrite@*/
/*@-nullstate -compdef @*/	/* b->modl may be null @*/
void mpbcopy(mpbarrett* b, const mpbarrett* copy)
{
	register size_t size = copy->size;

	if (size)
	{
		if (b->modl)
		{
			if (b->size != size)
				b->modl = (mpw*) realloc(b->modl, (2*size+1) * sizeof(*b->modl));
		}
		else
			b->modl = (mpw*) malloc((2*size+1) * sizeof(*b->modl));

		if (b->modl)
		{
			b->size = size;
			b->mu = b->modl+copy->size;
			mpcopy(2*size+1, b->modl, copy->modl);
		}
		else
		{
			b->size = 0;
			b->mu = (mpw*) 0;
		}
	}
	else if (b->modl)
	{
		free(b->modl);
		b->size = 0;
		b->modl = (mpw*) 0;
		b->mu = (mpw*) 0;
	}
	else
		{};
}
/*@=nullstate =compdef @*/
/*@=boundswrite@*/

/*@-boundswrite@*/
/*@-nullstate -compdef @*/	/* b->modl may be null @*/
/**
 * mpbset
 */
void mpbset(mpbarrett* b, size_t size, const mpw* data)
{
	if (size > 0)
	{
		if (b->modl)
		{
			if (b->size != size)
				b->modl = (mpw*) realloc(b->modl, (2*size+1) * sizeof(*b->modl));
		}
		else
			b->modl = (mpw*) malloc((2*size+1) * sizeof(*b->modl));

		if (b->modl)
		{
			mpw* temp = (mpw*) malloc((6*size+4) * sizeof(*temp));

			b->size = size;
			b->mu = b->modl+size;
			mpcopy(size, b->modl, data);
			/*@-nullpass@*/		/* temp may be NULL */
			mpbmu_w(b, temp);

			free(temp);
			/*@=nullpass@*/
		}
		else
		{
			b->size = 0;
			b->mu = (mpw*) 0;
		}
	}
}
/*@=nullstate =compdef @*/
/*@=boundswrite@*/

/*@-boundswrite@*/
/*@-nullstate -compdef @*/	/* b->modl may be null @*/
void mpbsethex(mpbarrett* b, const char* hex)
{
	size_t length = strlen(hex);
	size_t size = (length+7) >> 3;
	uint8 rem = (uint8)(length & 0x7);

	if (b->modl)
	{
		if (b->size != size)
			b->modl = (mpw*) realloc(b->modl, (2*size+1) * sizeof(*b->modl));
	}
	else
		b->modl = (mpw*) malloc((2*size+1) * sizeof(*b->modl));

	if (b->modl != (mpw*) 0)
	{
		register size_t  val = 0;
		register mpw* dst = b->modl;
		register mpw* temp = (mpw*) malloc((6*size+4) * sizeof(*temp));
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
		mpbmu_w(b, temp);

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
/*@=boundswrite@*/

/**
 *  Computes the Barrett 'mu' coefficient.
 *  needs workspace of (6*size+4) words
 */
/*@-boundswrite@*/
void mpbmu_w(mpbarrett* b, mpw* wksp)
{
	register size_t size = b->size;
	register size_t shift;
	register mpw* divmod = wksp;
	register mpw* dividend = divmod+(size*2+2);
	register mpw* workspace = dividend+(size*2+1);

	/* normalize modulus before division */
	shift = mpnorm(size, b->modl);
	/* make the dividend, initialize first word to 1 (shifted); the rest is zero */
	*dividend = ((mpw) MP_LSBMASK << shift);
	mpzero(size*2, dividend+1);
	mpndivmod(divmod, size*2+1, dividend, size, b->modl, workspace);
	/*@-nullpass@*/ /* b->mu may be NULL */
	mpcopy(size+1, b->mu, divmod+1);
	/*@=nullpass@*/
	/* de-normalize */
	mprshift(size, b->modl, shift);
}
/*@=boundswrite@*/

/**
 *  Generates a random number in the range 1 < r < b-1.
 *  need workspace of (size) words
 */
/*@-boundswrite@*/
void mpbrnd_w(const mpbarrett* b, randomGeneratorContext* rc, mpw* result, mpw* wksp)
{
	size_t msz = mpmszcnt(b->size, b->modl);

	mpcopy(b->size, wksp, b->modl);
	(void) mpsubw(b->size, wksp, 1);

	do
	{
		/*@-noeffectuncon@*/ /* LCL: ??? */
		(void) rc->rng->next(rc->param, (byte*) result, MP_WORDS_TO_BYTES(b->size));
		/*@=noeffectuncon@*/

		/*@-shiftimplementation -usedef@*/
		result[0] &= (MP_ALLMASK >> msz);
		/*@=shiftimplementation =usedef@*/

		while (mpge(b->size, result, wksp))
			(void) mpsub(b->size, result, wksp);
	} while (mpleone(b->size, result));
}
/*@=boundswrite@*/

/**
 *  Generates a random odd number in the range 1 < r < b-1.
 *  needs workspace of (size) words
 */
/*@-boundswrite@*/
void mpbrndodd_w(const mpbarrett* b, randomGeneratorContext* rc, mpw* result, mpw* wksp)
{
	uint32 msz = mpmszcnt(b->size, b->modl);

	mpcopy(b->size, wksp, b->modl);
	(void) mpsubw(b->size, wksp, 1);

	do
	{
		/*@-noeffectuncon@*/ /* LCL: ??? */
		(void) rc->rng->next(rc->param, (byte*) result, MP_WORDS_TO_BYTES(b->size));
		/*@=noeffectuncon@*/

		/*@-shiftimplementation -usedef@*/
		result[0] &= (MP_ALLMASK >> msz);
		/*@=shiftimplementation =usedef@*/
		mpsetlsb(b->size, result);

		while (mpge(b->size, result, wksp))
		{
			(void) mpsub(b->size, result, wksp);
			mpsetlsb(b->size, result);
		}
	} while (mpleone(b->size, result));
}
/*@=boundswrite@*/

/**
 *  Generates a random invertible (modulo b) in the range 1 < r < b-1.
 *  needs workspace of (6*size+6) words
 */
void mpbrndinv_w(const mpbarrett* b, randomGeneratorContext* rc, mpw* result, mpw* inverse, mpw* wksp)
{
	register size_t size = b->size;

	do
	{
		if (mpeven(size, b->modl))
			mpbrndodd_w(b, rc, result, wksp);
		else
			mpbrnd_w(b, rc, result, wksp);

	} while (mpbinv_w(b, size, result, inverse, wksp) == 0);
}

/**
 *  Computes the barrett modular reduction of a number x, which has twice the size of b.
 *  needs workspace of (2*size+2) words
 */
/*@-boundswrite@*/
void mpbmod_w(const mpbarrett* b, const mpw* data, mpw* result, mpw* wksp)
{
	register mpw rc;
	register size_t sp = 2;
	register const mpw* src = data+b->size+1;
	register       mpw* dst = wksp+b->size+1;

	/*@-nullpass@*/ /* b->mu may be NULL */
	rc = mpsetmul(sp, dst, b->mu, *(--src));
	*(--dst) = rc;

	while (sp <= b->size)
	{
		sp++;
		if ((rc = *(--src)))
		{
			rc = mpaddmul(sp, dst, b->mu, rc);
			*(--dst) = rc;
		}
		else
			*(--dst) = 0;
	}
	if ((rc = *(--src)))
	{
		rc = mpaddmul(sp, dst, b->mu, rc);
		*(--dst) = rc;
	}
	else
		*(--dst) = 0;
	/*@=nullpass@*/

	sp = b->size;
	rc = 0;

	dst = wksp+b->size+1;
	src = dst;

	/*@-evalorder@*/ /* --src side effect, dst/src aliases */
	*dst = mpsetmul(sp, dst+1, b->modl, *(--src));
	/*@=evalorder@*/

	while (sp > 0)
		(void) mpaddmul(sp--, dst, b->modl+(rc++), *(--src));

	mpsetx(b->size+1, wksp, b->size*2, data);
	(void) mpsub(b->size+1, wksp, wksp+b->size+1);

	while (mpgex(b->size+1, wksp, b->size, b->modl))
		(void) mpsubx(b->size+1, wksp, b->size, b->modl);
	mpcopy(b->size, result, wksp+1);
}
/*@=boundswrite@*/

/**
 *  Copies (b-1) into result.
 */
/*@-boundswrite@*/
void mpbsubone(const mpbarrett* b, mpw* result)
{
	register size_t size = b->size;

	mpcopy(size, result, b->modl);
	(void) mpsubw(size, result, 1);
}
/*@=boundswrite@*/

/**
 *  Computes the negative (modulo b) of x, where x must contain a value between 0 and b-1.
 */
/*@-boundswrite@*/
void mpbneg(const mpbarrett* b, const mpw* data, mpw* result)
{
	register size_t  size = b->size;

	mpcopy(size, result, data);
	mpneg(size, result);
	(void) mpadd(size, result, b->modl);
}
/*@=boundswrite@*/

/**
 *  Computes the sum (modulo b) of x and y.
 *  needs a workspace of (4*size+2) words
 */
void mpbaddmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* result, mpw* wksp)
{
	/* xsize and ysize must be less than or equal to b->size */
	register size_t  size = b->size;
	register mpw* temp = wksp + size*2+2;

	mpsetx(2*size, temp, xsize, data);
	(void) mpaddx(2*size, temp, ysize, ydata);

	mpbmod_w(b, temp, result, wksp);
}

/**
 *  Computes the difference (modulo b) of x and y.
 *  needs a workspace of (4*size+2) words
 */
void mpbsubmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* result, mpw* wksp)
{
	/* xsize and ysize must be less than or equal to b->size */
	register size_t  size = b->size;
	register mpw* temp = wksp + size*2+2;
	
	mpsetx(2*size, temp, xsize, xdata);
	if (mpsubx(2*size, temp, ysize, ydata)) /* if there's carry, i.e. the result would be negative, add the modulus */
		(void) mpaddx(2*size, temp, size, b->modl);

	mpbmod_w(b, temp, result, wksp);
}

/**
 *  Computes the product (modulo b) of x and y.
 *  needs a workspace of (4*size+2) words
 */
void mpbmulmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* result, mpw* wksp)
{
	/* xsize and ysize must be <= b->size */
	register size_t  size = b->size;
	register mpw* temp = wksp + size*2+2;
	register mpw  fill = size*2-xsize-ysize;

	if (fill)
		mpzero(fill, temp);

	mpmul(temp+fill, xsize, xdata, ysize, ydata);
	/*@-compdef@*/	/* *temp undefined */
	mpbmod_w(b, temp, result, wksp);
	/*@=compdef@*/
}

/**
 *  Computes the square (modulo b) of x.
 *  needs a workspace of (4*size+2) words
 */
void mpbsqrmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, mpw* result, mpw* wksp)
{
	/* xsize must be <= b->size */
	register size_t  size = b->size;
	register mpw* temp = wksp + size*2+2;
	register mpw  fill = 2*(size-xsize);

	if (fill)
		mpzero(fill, temp);

	mpsqr(temp+fill, xsize, xdata);
	/*@-compdef@*/	/* *temp undefined */
	mpbmod_w(b, temp, result, wksp);
	/*@=compdef@*/
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
static void mpbslide_w(const mpbarrett* b, size_t xsize, const mpw* xdata, /*@out@*/ mpw* slide, /*@out@*/ mpw* wksp)
	/*@modifies slide, wksp @*/
{
	register size_t size = b->size;
	mpbsqrmod_w(b, xsize, xdata,                     slide       , wksp); /* x^2 mod b, temp */
	mpbmulmod_w(b, xsize, xdata, size, slide       , slide+size  , wksp); /* x^3 mod b */
	mpbmulmod_w(b,  size, slide, size, slide+size  , slide+2*size, wksp); /* x^5 mod b */
	mpbmulmod_w(b,  size, slide, size, slide+2*size, slide+3*size, wksp); /* x^7 mod b */
	mpbmulmod_w(b,  size, slide, size, slide+3*size, slide+4*size, wksp); /* x^9 mod b */
	mpbmulmod_w(b,  size, slide, size, slide+4*size, slide+5*size, wksp); /* x^11 mod b */
	mpbmulmod_w(b,  size, slide, size, slide+5*size, slide+6*size, wksp); /* x^13 mod b */
	mpbmulmod_w(b,  size, slide, size, slide+6*size, slide+7*size, wksp); /* x^15 mod b */
	mpsetx(size, slide, xsize, xdata);                                    /* x^1 mod b */
}

/*@observer@*/ /*@unchecked@*/
static byte mpbslide_presq[16] = 
{ 0, 1, 1, 2, 1, 3, 2, 3, 1, 4, 3, 4, 2, 4, 3, 4 };

/*@observer@*/ /*@unchecked@*/
static byte mpbslide_mulg[16] =
{ 0, 0, 0, 1, 0, 2, 1, 3, 0, 4, 2, 5, 1, 6, 3, 7 };

/*@observer@*/ /*@unchecked@*/
static byte mpbslide_postsq[16] =
{ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 };

/**
 * mpbpowmod_w
 * needs workspace of 4*size+2 words
 */
/*@-boundsread@*/
void mpbpowmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t psize, const mpw* pdata, mpw* result, mpw* wksp)
{
	/*
	 * Modular exponention
	 *
	 * Uses sliding window exponentiation; needs extra storage: if K=3, needs 8*size, if K=4, needs 16*size
	 *
	 */

	/* K == 4 for the first try */
	
	size_t size = b->size;
	mpw temp = 0;

	while (psize)
	{
		if ((temp = *(pdata++))) /* break when first non-zero word found */
			break;
		psize--;
	}

	/* if temp is still zero, then we're trying to raise x to power zero, and result stays one */
	if (temp)
	{
		mpw* slide = (mpw*) malloc((8*size)*sizeof(*slide));

		/*@-nullpass@*/		/* slide may be NULL */
		mpbslide_w(b, xsize, xdata, slide, wksp);

		/*@-internalglobs -mods@*/ /* noisy */
		mpbpowmodsld_w(b, slide, psize, pdata-1, result, wksp);
		/*@=internalglobs =mods@*/

		free(slide);
		/*@=nullpass@*/
	}
}
/*@=boundsread@*/

/*@-boundsread@*/
void mpbpowmodsld_w(const mpbarrett* b, const mpw* slide, size_t psize, const mpw* pdata, mpw* result, mpw* wksp)
{
	/*
	 * Modular exponentiation with precomputed sliding window table, so no x is required
	 *
	 */

	size_t size = b->size;
	mpw temp = 0;

	mpsetw(size, result, 1);

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
		short l = 0, n = 0, count = MP_WBITS;

		/* first skip bits until we reach a one */
		while (count != 0)
		{
			if (temp & MP_MSBMASK)
				break;
			temp <<= 1;
			count--;
		}

		while (psize)
		{
			while (count != 0)
			{
				byte bit = (temp & MP_MSBMASK) ? 1 : 0;

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
						byte s = mpbslide_presq[n];
						
						while (s--)
							mpbsqrmod_w(b, size, result, result, wksp);
						
						mpbmulmod_w(b, size, result, size, slide+mpbslide_mulg[n]*size, result, wksp);
						
						s = mpbslide_postsq[n];
						
						while (s--)
							mpbsqrmod_w(b, size, result, result, wksp);

						l = n = 0;
					}
				}
				else
					mpbsqrmod_w(b, size, result, result, wksp);

				temp <<= 1;
				count--;
			}
			if (--psize)
			{
				count = MP_WBITS;
				temp = *(pdata++);
			}
		}

		if (n != 0)
		{
			byte s = mpbslide_presq[n];
			while (s--)
				mpbsqrmod_w(b, size, result, result, wksp);
				
			mpbmulmod_w(b, size, result, size, slide+mpbslide_mulg[n]*size, result, wksp);
			
			s = mpbslide_postsq[n];
			
			while (s--)
				mpbsqrmod_w(b, size, result, result, wksp);
		}
	}	
	/*@=charindex@*/
}
/*@=boundsread@*/

/**
 * mpbtwopowmod_w
 *  needs workspace of (4*size+2) words
 */
/*@-boundsread@*/
void mpbtwopowmod_w(const mpbarrett* b, size_t psize, const mpw* pdata, mpw* result, mpw* wksp)
{
	/*
	 * Modular exponention, 2^p mod modulus, special optimization
	 *
	 * Uses left-to-right exponentiation; needs no extra storage
	 *
	 */

	/* this routine calls mpbmod, which needs (size*2+2), this routine needs (size*2) for sdata */

	register size_t size = b->size;
	register mpw temp = 0;

	mpsetw(size, result, 1);

	while (psize)
	{
		if ((temp = *(pdata++))) /* break when first non-zero word found */
			break;
		psize--;
	}

	/* if temp is still zero, then we're trying to raise x to power zero, and result stays one */
	if (temp)
	{
		register int count = MP_WBITS;

		/* first skip bits until we reach a one */
		while (count)
		{
			if (temp & MP_MSBMASK)
				break;
			temp <<= 1;
			count--;
		}

		while (psize--)
		{
			while (count)
			{
				/* always square */
				mpbsqrmod_w(b, size, result, result, wksp);
				
				/* multiply by two if bit is 1 */
				if (temp & MP_MSBMASK)
				{
					if (mpadd(size, result, result) || mpge(size, result, b->modl))
					{
						/* there was carry, or the result is greater than the modulus, so we need to adjust */
						(void) mpsub(size, result, b->modl);
					}
				}

				temp <<= 1;
				count--;
			}
			count = MP_WBITS;
			temp = *(pdata++);
		}
	}
}
/*@=boundsread@*/

#ifdef	DYING
/**
 *  Computes the inverse (modulo b) of x, and returns 1 if x was invertible.
 *  needs workspace of (6*size+6) words
 *  @note xdata and result cannot point to the same area
 */
int mpbinv_w(const mpbarrett* b, size_t xsize, const mpw* xdata, mpw* result, mpw* wksp)
{
	/*
	 * Fact: if a element of Zn, then a is invertible if and only if gcd(a,n) = 1
	 * Hence: if b->modl is even, then x must be odd, otherwise the gcd(x,n) >= 2
	 *
	 * The calling routine must guarantee this condition.
	 */

	register size_t size = b->size;

	mpw* udata = wksp;
	mpw* vdata = udata+size+1;
	mpw* adata = vdata+size+1;
	mpw* bdata = adata+size+1;
	mpw* cdata = bdata+size+1;
	mpw* ddata = cdata+size+1;

	mpsetx(size+1, udata, size, b->modl);
	mpsetx(size+1, vdata, xsize, xdata);
	mpzero(size+1, bdata);
	mpsetw(size+1, ddata, 1);

	if (mpodd(size, b->modl))
	{
		/* use simplified binary extended gcd algorithm */

		while (1)
		{
			while (mpeven(size+1, udata))
			{
				mpdivtwo(size+1, udata);

				if (mpodd(size+1, bdata))
					(void) mpsubx(size+1, bdata, size, b->modl);

				mpsdivtwo(size+1, bdata);
			}
			while (mpeven(size+1, vdata))
			{
				mpdivtwo(size+1, vdata);

				if (mpodd(size+1, ddata))
					(void) mpsubx(size+1, ddata, size, b->modl);

				mpsdivtwo(size+1, ddata);
			}
			if (mpge(size+1, udata, vdata))
			{
				(void) mpsub(size+1, udata, vdata);
				(void) mpsub(size+1, bdata, ddata);
			}
			else
			{
				(void) mpsub(size+1, vdata, udata);
				(void) mpsub(size+1, ddata, bdata);
			}

			if (mpz(size+1, udata))
			{
				if (mpisone(size+1, vdata))
				{
					if (result)
					{
						mpsetx(size, result, size+1, ddata);
						/*@-usedef@*/
						if (*ddata & MP_MSBMASK)
						{
							/* keep adding the modulus until we get a carry */
							while (!mpadd(size, result, b->modl));
						}
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
		mpsetw(size+1, adata, 1);
		mpzero(size+1, cdata);

		while (1)
		{
			while (mpeven(size+1, udata))
			{
				mpdivtwo(size+1, udata);

				if (mpodd(size+1, adata) || mpodd(size+1, bdata))
				{
					(void) mpaddx(size+1, adata, xsize, xdata);
					(void) mpsubx(size+1, bdata, size, b->modl);
				}

				mpsdivtwo(size+1, adata);
				mpsdivtwo(size+1, bdata);
			}
			while (mpeven(size+1, vdata))
			{
				mpdivtwo(size+1, vdata);

				if (mpodd(size+1, cdata) || mpodd(size+1, ddata))
				{
					(void) mpaddx(size+1, cdata, xsize, xdata);
					(void) mpsubx(size+1, ddata, size, b->modl);
				}

				mpsdivtwo(size+1, cdata);
				mpsdivtwo(size+1, ddata);
			}
			if (mpge(size+1, udata, vdata))
			{
				(void) mpsub(size+1, udata, vdata);
				(void) mpsub(size+1, adata, cdata);
				(void) mpsub(size+1, bdata, ddata);
			}
			else
			{
				(void) mpsub(size+1, vdata, udata);
				(void) mpsub(size+1, cdata, adata);
				(void) mpsub(size+1, ddata, bdata);
			}

			if (mpz(size+1, udata))
			{
				if (mpisone(size+1, vdata))
				{
					if (result)
					{
						mpsetx(size, result, size+1, ddata);
						/*@-usedef@*/
						if (*ddata & MP_MSBMASK)
						{
							/* keep adding the modulus until we get a carry */
							while (!mpadd(size, result, b->modl));
						}
						/*@=usedef@*/
					}
					return 1;
				}
				return 0;
			}
		}
	}
}
#else

/*@unchecked@*/
static int _debug = 0;

#undef	FULL_BINARY_EXTENDED_GCD

/**
 *  Computes the inverse (modulo b) of x, and returns 1 if x was invertible.
 */
/*@-boundsread@*/
int mpbinv_w(const mpbarrett* b, size_t xsize, const mpw* xdata, mpw* result, mpw* wksp)
{
	size_t  ysize = b->size+1;
 	int k;
	mpw* u  = wksp;
	mpw* v  =  u+ysize;
	mpw* u1 =  v+ysize;
	mpw* v1 = u1+ysize;
	mpw* t1 = v1+ysize;
	mpw* u3 = t1+ysize;
	mpw* v3 = u3+ysize;
	mpw* t3 = v3+ysize;

#ifdef	FULL_BINARY_EXTENDED_GCD
	mpw* u2 = t3+ysize;
	mpw* v2 = u2+ysize;
	mpw* t2 = v2+ysize;
#endif

	mpsetx(ysize, u, xsize, xdata);
	mpsetx(ysize, v, b->size, b->modl);

	/* Y1. Find power of 2. */
	for (k = 0; mpeven(ysize, u) && mpeven(ysize, v); k++) {
		mpdivtwo(ysize, u);
		mpdivtwo(ysize, v);
	}

	/* Y2. Initialize. */
	mpsetw(ysize, u1, 1);
	mpsetx(ysize, v1, ysize, v);
	mpsetx(ysize, u3, ysize, u);
	mpsetx(ysize, v3, ysize, v);

#ifdef	FULL_BINARY_EXTENDED_GCD
	mpzero(ysize, u2);
	mpsetw(ysize, v2, 1);
	(void) mpsub(ysize, v2, u);
#endif

if (_debug < 0) {
/*@-modfilesys@*/
fprintf(stderr, "       u: "), mpprintln(stderr, ysize, u);
fprintf(stderr, "       v: "), mpprintln(stderr, ysize, v);
fprintf(stderr, "      u1: "), mpprintln(stderr, ysize, u1);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      u2: "), mpprintln(stderr, ysize, u2);
#endif
fprintf(stderr, "      u3: "), mpprintln(stderr, ysize, u3);
fprintf(stderr, "      v1: "), mpprintln(stderr, ysize, v1);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      v2: "), mpprintln(stderr, ysize, v2);
#endif
fprintf(stderr, "      v3: "), mpprintln(stderr, ysize, v3);
/*@=modfilesys@*/
}

	if (mpodd(ysize, u)) {
		mpzero(ysize, t1);
#ifdef	FULL_BINARY_EXTENDED_GCD
		mpzero(ysize, t2);
		mpsubw(ysize, t2, 1);
#endif
		mpzero(ysize, t3);
		(void) mpsub(ysize, t3, v);
		goto Y4;
	} else {
		mpsetw(ysize, t1, 1);
#ifdef	FULL_BINARY_EXTENDED_GCD
		mpzero(ysize, t2);
#endif
		mpsetx(ysize, t3, ysize, u);
	}

	do {
	    do {
#ifdef	FULL_BINARY_EXTENDED_GCD
		if (mpodd(ysize, t1) ||  mpodd(ysize, t2)) {
			(void) mpadd(ysize, t1, v);
			(void) mpsub(ysize, t2, u);
		}
#else
		/* XXX this assumes v is odd, true for DSA inversion. */
		if (mpodd(ysize, t1))
			(void) mpadd(ysize, t1, v);
#endif

		mpsdivtwo(ysize, t1);
#ifdef	FULL_BINARY_EXTENDED_GCD
		mpsdivtwo(ysize, t2);
#endif
		mpsdivtwo(ysize, t3);
Y4:
if (_debug < 0) {
/*@-modfilesys@*/
fprintf(stderr, "-->Y4 t3: "), mpprintln(stderr, ysize, t3);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      t2: "), mpprintln(stderr, ysize, t2);
#endif
fprintf(stderr, "      t1: "), mpprintln(stderr, ysize, t1);
/*@=modfilesys@*/
}
	    } while (mpeven(ysize, t3));

	    /* Y5. Reset max(u3,v3). */
	    if (!(*t3 & MP_MSBMASK)) {
		mpsetx(ysize, u1, ysize, t1);
#ifdef	FULL_BINARY_EXTENDED_GCD
		mpsetx(ysize, u2, ysize, t2);
#endif
		mpsetx(ysize, u3, ysize, t3);
if (_debug < 0) {
/*@-modfilesys@*/
fprintf(stderr, "-->Y5 u1: "), mpprintln(stderr, ysize, u1);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      u2: "), mpprintln(stderr, ysize, u2);
#endif
fprintf(stderr, "      u3: "), mpprintln(stderr, ysize, u3);
/*@=modfilesys@*/
}
	    } else {
		mpsetx(ysize, v1, ysize, v);
		(void) mpsub(ysize, v1, t1);
#ifdef	FULL_BINARY_EXTENDED_GCD
		mpsetx(ysize, v2, ysize, u);
		mpneg(ysize, v2);
		(void) mpsub(ysize, v2, t2);
#endif
		mpzero(ysize, v3);
		(void) mpsub(ysize, v3, t3);
if (_debug < 0) {
/*@-modfilesys@*/
fprintf(stderr, "-->Y5 v1: "), mpprintln(stderr, ysize, v1);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      v2: "), mpprintln(stderr, ysize, v2);
#endif
fprintf(stderr, "      v3: "), mpprintln(stderr, ysize, v3);
/*@=modfilesys@*/
}
	    }

	    /* Y6. Subtract. */
	    mpsetx(ysize, t1, ysize, u1);
	    (void) mpsub(ysize, t1, v1);
#ifdef	FULL_BINARY_EXTENDED_GCD
	    mpsetx(ysize, t2, ysize, u2);
	    (void) mpsub(ysize, t2, v2);
#endif
	    mpsetx(ysize, t3, ysize, u3);
	    (void) mpsub(ysize, t3, v3);

	    if (*t1 & MP_MSBMASK) {
		(void) mpadd(ysize, t1, v);
#ifdef	FULL_BINARY_EXTENDED_GCD
		(void) mpsub(ysize, t2, u);
#endif
	    }

if (_debug < 0) {
/*@-modfilesys@*/
fprintf(stderr, "-->Y6 t1: "), mpprintln(stderr, ysize, t1);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      t2: "), mpprintln(stderr, ysize, t2);
#endif
fprintf(stderr, "      t3: "), mpprintln(stderr, ysize, t3);
/*@=modfilesys@*/
}

	} while (mpnz(ysize, t3));

	if (!mpisone(ysize, u3) || !mpisone(ysize, v3))
		return 0;

	if (result) {
		while (--k > 0)
			(void) mpadd(ysize, u1, u1);
		mpsetx(b->size, result, ysize, u1);
	}

if (_debug) {
/*@-modfilesys@*/
if (result)
fprintf(stderr, "=== EXIT: "), mpprintln(stderr, b->size, result);
fprintf(stderr, "      u1: "), mpprintln(stderr, ysize, u1);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      u2: "), mpprintln(stderr, ysize, u2);
#endif
fprintf(stderr, "      u3: "), mpprintln(stderr, ysize, u3);
fprintf(stderr, "      v1: "), mpprintln(stderr, ysize, v1);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      v2: "), mpprintln(stderr, ysize, v2);
#endif
fprintf(stderr, "      v3: "), mpprintln(stderr, ysize, v3);
fprintf(stderr, "      t1: "), mpprintln(stderr, ysize, t1);
#ifdef	FULL_BINARY_EXTENDED_GCD
fprintf(stderr, "      t2: "), mpprintln(stderr, ysize, t2);
#endif
fprintf(stderr, "      t3: "), mpprintln(stderr, ysize, t3);
/*@=modfilesys@*/
}

	return 1;
}
/*@=boundsread@*/

#endif

/**
 * needs workspace of (7*size+2) words
 */
/*@-boundsread@*/
int mpbpprime_w(const mpbarrett* b, randomGeneratorContext* rc, int t, mpw* wksp)
{
	/*
	 * This test works for candidate probable primes >= 3, which are also not small primes.
	 *
	 * It assumes that b->modl contains the candidate prime
	 *
	 */

	size_t size = b->size;

	/* first test if modl is odd */

	if (mpodd(b->size, b->modl))
	{
		/*
		 * Small prime factor test:
		 * 
		 * Tables in mpspprod contain multi-precision integers with products of small primes
		 * If the greatest common divisor of this product and the candidate is not one, then
		 * the candidate has small prime factors, or is a small prime. Neither is acceptable when
		 * we are looking for large probable primes =)
		 *
		 */
		
		if (size > SMALL_PRIMES_PRODUCT_MAX)
		{
			/*@-globs@*/
			mpsetx(size, wksp+size, SMALL_PRIMES_PRODUCT_MAX, mpspprod[SMALL_PRIMES_PRODUCT_MAX-1]);
			/*@=globs@*/
			/*@-compdef@*/ /* LCL: wksp+size */
			mpgcd_w(size, b->modl, wksp+size, wksp, wksp+2*size);
			/*@=compdef@*/
		}
		else
		{
			/*@-globs@*/
			mpgcd_w(size, b->modl, mpspprod[size-1], wksp, wksp+2*size);
			/*@=globs@*/
		}

		if (mpisone(size, wksp))
		{
			return mppmilrab_w(b, rc, t, wksp);
		}
	}

	return 0;
}
/*@=boundsread@*/

void mpbnrnd(const mpbarrett* b, randomGeneratorContext* rc, mpnumber* result)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc(size * sizeof(*temp));

	mpnfree(result);
	mpnsize(result, size);
	/*@-nullpass@*/		/* temp may be NULL */
	/*@-usedef@*/		/* result->data unallocated? */
	mpbrnd_w(b, rc, result->data, temp);
	/*@=usedef@*/

	free(temp);
	/*@=nullpass@*/
}

void mpbnmulmod(const mpbarrett* b, const mpnumber* x, const mpnumber* y, mpnumber* result)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc((4*size+2) * sizeof(*temp));

	/* xsize and ysize must be <= b->size */
	register size_t  fill = 2*size-x->size-y->size;
	/*@-nullptrarith@*/	/* temp may be NULL */
	register mpw* opnd = temp+size*2+2;
	/*@=nullptrarith@*/

	mpnfree(result);
	mpnsize(result, size);

	if (fill)
		mpzero(fill, opnd);

	mpmul(opnd+fill, x->size, x->data, y->size, y->data);
	/*@-nullpass@*/		/* temp may be NULL */
	/*@-usedef -compdef @*/	/* result->data unallocated? */
	mpbmod_w(b, opnd, result->data, temp);
	/*@=usedef =compdef @*/

	free(temp);
	/*@=nullpass@*/
}

void mpbnsqrmod(const mpbarrett* b, const mpnumber* x, mpnumber* result)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc(size * sizeof(*temp));

	/* xsize must be <= b->size */
	register size_t  fill = 2*(size-x->size);
	/*@-nullptrarith@*/	/* temp may be NULL */
	register mpw* opnd = temp + size*2+2;
	/*@=nullptrarith@*/

	mpnfree(result);
	mpnsize(result, size);

	if (fill)
		mpzero(fill, opnd);

	mpsqr(opnd+fill, x->size, x->data);
	/*@-nullpass@*/		/* temp may be NULL */
	/*@-usedef -compdef @*/	/* result->data unallocated? */
	mpbmod_w(b, opnd, result->data, temp);
	/*@=usedef =compdef @*/

	free(temp);
	/*@=nullpass@*/
}

void mpbnpowmod(const mpbarrett* b, const mpnumber* x, const mpnumber* pow, mpnumber* y)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc((4*size+2) * sizeof(*temp));

	mpnfree(y);
	mpnsize(y, size);

	/*@-nullpass@*/		/* temp may be NULL */
	mpbpowmod_w(b, x->size, x->data, pow->size, pow->data, y->data, temp);

	free(temp);
	/*@=nullpass@*/
}

void mpbnpowmodsld(const mpbarrett* b, const mpw* slide, const mpnumber* pow, mpnumber* y)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc((4*size+2) * sizeof(*temp));

	mpnfree(y);
	mpnsize(y, size);

	/*@-nullpass@*/		/* temp may be NULL */
	/*@-internalglobs -mods@*/ /* noisy */
	mpbpowmodsld_w(b, slide, pow->size, pow->data, y->data, temp);
	/*@=internalglobs =mods@*/

	free(temp);
	/*@=nullpass@*/
}
/*@=sizeoftype =type@*/
