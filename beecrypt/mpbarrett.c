/*
 * Copyright (c) 1997, 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

/*!\file mpbarrett.c
 * \brief Multi-precision integer routines using Barrett modular reduction.
 *        For more information on this algorithm, see:
 *        "Handbook of Applied Cryptography", Chapter 14.3.3
 *        Menezes, van Oorschot, Vanstone
 *        CRC Press
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP__m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/beecrypt.h"
#include "beecrypt/mpprime.h"
#include "beecrypt/mpnumber.h"
#include "beecrypt/mpbarrett.h"

/*
 * mpbzero
 */
void mpbzero(mpbarrett* b)
{
	b->size = 0;
	b->modl = b->mu = (mpw*) 0;
}

/*
 * mpbinit
 * \brief allocates the data words for an mpbarrett structure
 *        will allocate 2*size+1 words
 */
void mpbinit(mpbarrett* b, size_t size)
{
	b->size	= size;
	b->modl	= (mpw*) calloc(2*size+1, sizeof(mpw));

	if (b->modl != (mpw*) 0)
		b->mu = b->modl+size;
	else
		b->mu = (mpw*) 0;
}

/*
 * mpbfree
 */
void mpbfree(mpbarrett* b)
{
	if (b->modl != (mpw*) 0)
	{
		free(b->modl);
		b->modl = b->mu = (mpw*) 0;
	}
	b->size = 0;
}

void mpbcopy(mpbarrett* b, const mpbarrett* copy)
{
	register size_t size = copy->size;

	if (size)
	{
		if (b->modl)
		{
			if (b->size != size)
				b->modl = (mpw*) realloc(b->modl, (2*size+1) * sizeof(mpw));
		}
		else
			b->modl = (mpw*) malloc((2*size+1) * sizeof(mpw));

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
		b->modl = b->mu = (mpw*) 0;
	}
}

void mpbwipe(mpbarrett* b)
{
	if (b->modl != (mpw*) 0)
		mpzero(2*(b->size)+1, b->modl);
}

/*
 * mpbset
 */
void mpbset(mpbarrett* b, size_t size, const mpw *data)
{
	if (size > 0)
	{
		if (b->modl)
		{
			if (b->size != size)
				b->modl = (mpw*) realloc(b->modl, (2*size+1) * sizeof(mpw));
		}
		else
			b->modl = (mpw*) malloc((2*size+1) * sizeof(mpw));

		if (b->modl)
		{
			mpw* temp = (mpw*) malloc((6*size+4) * sizeof(mpw));

			b->size = size;
			b->mu = b->modl+size;
			mpcopy(size, b->modl, data);
			mpbmu_w(b, temp);

			free(temp);
		}
		else
		{
			b->size = 0;
			b->mu = (mpw*) 0;
		}
	}
}

int mpbsetbin(mpbarrett* b, const byte* osdata, size_t ossize)
{
	int rc = -1;
	size_t size;
																				
	/* skip zero bytes */
	while (!(*osdata) && ossize)
	{
		osdata++;
		ossize--;
	}
																				
	size = MP_BYTES_TO_WORDS(ossize + MP_WBYTES - 1);
                                                                                
	if (b->modl)
	{
		if (b->size != size)
			b->modl = (mpw*) realloc(b->modl, (2*size+1) * sizeof(mpw));
	}
	else
		b->modl = (mpw*) malloc((2*size+1) * sizeof(mpw));

	if (b->modl)
	{
		register mpw* temp = (mpw*) malloc((6*size+4) * sizeof(mpw));

		b->size = size;
		b->mu = b->modl+size;

		rc = os2ip(b->modl, size, osdata, ossize);

		mpbmu_w(b, temp);

		free(temp);
	}

	return rc;
}

int mpbsethex(mpbarrett* b, const char* hex)
{
	int rc = -1;
	size_t len = strlen(hex);
	size_t size = MP_NIBBLES_TO_WORDS(len + MP_WNIBBLES - 1);

	if (b->modl)
	{
		if (b->size != size)
			b->modl = (mpw*) realloc(b->modl, (2*size+1) * sizeof(mpw));
	}
	else
		b->modl = (mpw*) malloc((2*size+1) * sizeof(mpw));

	if (b->modl)
	{
		register mpw* temp = (mpw*) malloc((6*size+4) * sizeof(mpw));

		b->size = size;
		b->mu = b->modl+size;

		rc = hs2ip(b->modl, size, hex, len);

		mpbmu_w(b, temp);

		free(temp);
	}
	else
	{
		b->size = 0;
		b->mu = 0;
	}

	return rc;
}

/*
 * mpbmu_w
 *  computes the Barrett 'mu' coefficient
 *  needs workspace of (6*size+4) words
 */
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
	mpcopy(size+1, b->mu, divmod+1);
	/* de-normalize */
	mprshift(size, b->modl, shift);
}

/*
 * mpbrnd_w
 *  generates a random number in the range 1 < r < b-1
 *  need workspace of (size) words
 */
void mpbrnd_w(const mpbarrett* b, randomGeneratorContext* rc, mpw* result, mpw* wksp)
{
	size_t msz = mpmszcnt(b->size, b->modl);

	mpcopy(b->size, wksp, b->modl);
	mpsubw(b->size, wksp, 1);

	do
	{
		rc->rng->next(rc->param, (byte*) result, MP_WORDS_TO_BYTES(b->size));

		result[0] &= (MP_ALLMASK >> msz);

		while (mpge(b->size, result, wksp))
			mpsub(b->size, result, wksp);
	} while (mpleone(b->size, result));
}

/*
 * mpbrndodd_w
 *  generates a random odd number in the range 1 < r < b-1
 *  needs workspace of (size) words
 */
void mpbrndodd_w(const mpbarrett* b, randomGeneratorContext* rc, mpw* result, mpw* wksp)
{
	size_t msz = mpmszcnt(b->size, b->modl);

	mpcopy(b->size, wksp, b->modl);
	mpsubw(b->size, wksp, 1);

	do
	{
		rc->rng->next(rc->param, (byte*) result, MP_WORDS_TO_BYTES(b->size));

		result[0] &= (MP_ALLMASK >> msz);
		mpsetlsb(b->size, result);

		while (mpge(b->size, result, wksp))
		{
			mpsub(b->size, result, wksp);
			mpsetlsb(b->size, result);
		}
	} while (mpleone(b->size, result));
}

/*
 * mpbrndinv_w
 *  generates a random invertible (modulo b) in the range 1 < r < b-1
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

	} while (mpextgcd_w(size, b->modl, result, inverse, wksp) == 0);
}

/*
 * mpbmod_w
 *  computes the barrett modular reduction of a number x, which has twice the size of b
 *  needs workspace of (2*size+2) words
 */
void mpbmod_w(const mpbarrett* b, const mpw* data, mpw* result, mpw* wksp)
{
	register mpw rc;
	register size_t sp = 2;
	register const mpw* src = data+b->size+1;
	register       mpw* dst = wksp+b->size+1;

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

	sp = b->size;
	rc = 0;

	dst = wksp+b->size+1;
	src = dst;

	*dst = mpsetmul(sp, dst+1, b->modl, *(--src));

	while (sp > 0)
		mpaddmul(sp--, dst, b->modl+(rc++), *(--src));

	mpsetx(b->size+1, wksp, b->size*2, data);
	mpsub(b->size+1, wksp, wksp+b->size+1);

	while (mpgex(b->size+1, wksp, b->size, b->modl))
		mpsubx(b->size+1, wksp, b->size, b->modl);

	mpcopy(b->size, result, wksp+1);
}

/*
 * mpbsubone
 *  copies (b-1) into result
 */
void mpbsubone(const mpbarrett* b, mpw* result)
{
	register size_t size = b->size;

	mpcopy(size, result, b->modl);
	mpsubw(size, result, 1);
}

/*
 * mpbneg
 *  computes the negative (modulo b) of x, where x must contain a value between 0 and b-1
 */
void mpbneg(const mpbarrett* b, const mpw* data, mpw* result)
{
	register size_t size = b->size;

	mpcopy(size, result, data);
	mpneg(size, result);
	mpadd(size, result, b->modl);
}

/*
 * mpbaddmod_w
 *  computes the sum (modulo b) of x and y
 *  needs a workspace of (4*size+2) words
 */
void mpbaddmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* result, mpw* wksp)
{
	/* xsize and ysize must be less than or equal to b->size */
	register size_t  size = b->size;
	register mpw* temp = wksp + size*2+2;

	mpsetx(2*size, temp, xsize, xdata);
	mpaddx(2*size, temp, ysize, ydata);

	mpbmod_w(b, temp, result, wksp);
}

/*
 * mpbsubmod_w
 *  computes the difference (modulo b) of x and y
 *  needs a workspace of (4*size+2) words
 */
void mpbsubmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* result, mpw* wksp)
{
	/* xsize and ysize must be less than or equal to b->size */
	register size_t  size = b->size;
	register mpw* temp = wksp + size*2+2;
	
	mpsetx(2*size, temp, xsize, xdata);
	if (mpsubx(2*size, temp, ysize, ydata)) /* if there's carry, i.e. the result would be negative, add the modulus */
		mpaddx(2*size, temp, size, b->modl);

	mpbmod_w(b, temp, result, wksp);
}

/*
 * mpmulmod_w
 *  computes the product (modulo b) of x and y
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
	mpbmod_w(b, temp, result, wksp);
}

/*
 * mpbsqrmod_w
 *  computes the square (modulo b) of x
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
	mpbmod_w(b, temp, result, wksp);
}

/*
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
 *     0 : - | -       | -
 *     1 : 1 |  g1 @ 0 | 0
 *    10 : 1 |  g1 @ 0 | 1
 *    11 : 2 |  g3 @ 1 | 0
 *   100 : 1 |  g1 @ 0 | 2
 *   101 : 3 |  g5 @ 2 | 0
 *   110 : 2 |  g3 @ 1 | 1
 *   111 : 3 |  g7 @ 3 | 0
 *  1000 : 1 |  g1 @ 0 | 3
 *  1001 : 4 |  g9 @ 4 | 0
 *  1010 : 3 |  g5 @ 2 | 1
 *  1011 : 4 | g11 @ 5 | 0
 *  1100 : 2 |  g3 @ 1 | 2
 *  1101 : 4 | g13 @ 6 | 0
 *  1110 : 3 |  g7 @ 3 | 1
 *  1111 : 4 | g15 @ 7 | 0
 *
 */

/*
 * mpbslide_w
 *  precomputes the sliding window table for computing powers of x modulo b
 *  needs workspace (4*size+2)
 */
void mpbslide_w(const mpbarrett* b, size_t xsize, const mpw* xdata, mpw* slide, mpw* wksp)
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

static byte mpbslide_presq[16] = 
{ 0, 1, 1, 2, 1, 3, 2, 3, 1, 4, 3, 4, 2, 4, 3, 4 };

static byte mpbslide_mulg[16] =
{ 0, 0, 0, 1, 0, 2, 1, 3, 0, 4, 2, 5, 1, 6, 3, 7 };

static byte mpbslide_postsq[16] =
{ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 };

/*
 * needs workspace of 4*size+2 words
 */
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
	mpw temp;

	while (psize)
	{
		if ((temp = *(pdata++))) /* break when first non-zero word found */
			break;
		psize--;
	}

	/* if temp is still zero, then we're trying to raise x to power zero, and result stays one */
	if (temp)
	{
		mpw* slide = (mpw*) malloc((8*size)*sizeof(mpw));

		mpbslide_w(b, xsize, xdata, slide, wksp);

		mpbpowmodsld_w(b, slide, psize, pdata-1, result, wksp);

		free(slide);
	}
}

void mpbpowmodsld_w(const mpbarrett* b, const mpw* slide, size_t psize, const mpw* pdata, mpw* result, mpw* wksp)
{
	/*
	 * Modular exponentiation with precomputed sliding window table, so no x is required
	 *
	 */

	size_t size = b->size;
	mpw temp;

	mpsetw(size, result, 1);

	while (psize)
	{
		if ((temp = *(pdata++))) /* break when first non-zero word found in power */
			break;
		psize--;
	}

	/* if temp is still zero, then we're trying to raise x to power zero, and result stays one */
	if (temp)
	{
		short l = 0, n = 0, count = MP_WBITS;

		/* first skip bits until we reach a one */
		while (count)
		{
			if (temp & MP_MSBMASK)
				break;
			temp <<= 1;
			count--;
		}

		while (psize)
		{
			while (count)
			{
				byte bit = (temp & MP_MSBMASK) ? 1 : 0;

				n <<= 1;
				n += bit;
				
				if (n)
				{
					if (l)
						l++;
					else if (bit)
						l = 1;

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

		if (n)
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
}

/*
 * mpbtwopowmod_w
 *  needs workspace of (4*size+2) words
 */
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
						mpsub(size, result, b->modl);
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

/*
 * needs workspace of (7*size+2) words
 */
int mpbpprime_w(const mpbarrett* b, randomGeneratorContext* r, int t, mpw* wksp)
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
			mpsetx(size, wksp+size, SMALL_PRIMES_PRODUCT_MAX, mpspprod[SMALL_PRIMES_PRODUCT_MAX-1]);
			mpgcd_w(size, b->modl, wksp+size, wksp, wksp+2*size);
		}
		else
		{
			mpgcd_w(size, b->modl, mpspprod[size-1], wksp, wksp+2*size);
		}

		if (mpisone(size, wksp))
		{
			return mppmilrab_w(b, r, t, wksp);
		}
	}

	return 0;
}

void mpbnrnd(const mpbarrett* b, randomGeneratorContext* rc, mpnumber* result)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc(size * sizeof(mpw));

	mpnfree(result);
	mpnsize(result, size);
	mpbrnd_w(b, rc, result->data, temp);

	free(temp);
}

void mpbnmulmod(const mpbarrett* b, const mpnumber* x, const mpnumber* y, mpnumber* result)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc((4*size+2) * sizeof(mpw));

	/* xsize and ysize must be <= b->size */
	register size_t  fill = 2*size-x->size-y->size;
	register mpw* opnd = temp+size*2+2;

	mpnfree(result);
	mpnsize(result, size);

	if (fill)
		mpzero(fill, opnd);

	mpmul(opnd+fill, x->size, x->data, y->size, y->data);
	mpbmod_w(b, opnd, result->data, temp);

	free(temp);
}

void mpbnsqrmod(const mpbarrett* b, const mpnumber* x, mpnumber* result)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc(size * sizeof(mpw));

	/* xsize must be <= b->size */
	register size_t  fill = 2*(size-x->size);
	register mpw* opnd = temp + size*2+2;

	if (fill)
		mpzero(fill, opnd);

	mpsqr(opnd+fill, x->size, x->data);
	mpnsize(result, size);
	mpbmod_w(b, opnd, result->data, temp);

	free(temp);
}

void mpbnpowmod(const mpbarrett* b, const mpnumber* x, const mpnumber* pow, mpnumber* y)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc((4*size+2) * sizeof(mpw));

	mpnfree(y);
	mpnsize(y, size);

	mpbpowmod_w(b, x->size, x->data, pow->size, pow->data, y->data, temp);

	free(temp);
}

void mpbnpowmodsld(const mpbarrett* b, const mpw* slide, const mpnumber* pow, mpnumber* y)
{
	register size_t  size = b->size;
	register mpw* temp = (mpw*) malloc((4*size+2) * sizeof(mpw));

	mpnfree(y);
	mpnsize(y, size);

	mpbpowmodsld_w(b, slide, pow->size, pow->data, y->data, temp);

	free(temp);
}

size_t mpbbits(const mpbarrett* b)
{
	return mpbits(b->size, b->modl);
}
