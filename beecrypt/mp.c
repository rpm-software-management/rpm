/** \ingroup MP_m
 * \file mp.c
 *
 * Multiprecision integer routines.
 */

/*
 * Copyright (c) 2002, 2003 Bob Deblier
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
#include "beecrypt.h"
#include "mpopt.h"
#include "mp.h"
#include "debug.h"

#ifndef ASM_MPZERO
/*@-boundswrite@*/
void mpzero(register size_t size, register mpw* data)
{
	while (size--)
		*(data++) = 0;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPFILL
/*@-boundswrite@*/
void mpfill(register size_t size, register mpw* data, register mpw fill)
{
	while (size--)
		*(data++) = fill;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPODD
/*@-boundsread@*/
int mpodd(register size_t size, register const mpw* data)
{
	return (int)(data[size-1] & 0x1);
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPEVEN
/*@-boundsread@*/
int mpeven(register size_t size, register const mpw* data)
{
	return !(int)(data[size-1] & 0x1);
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPZ
/*@-boundsread@*/
int mpz(register size_t size, register const mpw* data)
{
	while (size--)
		if (*(data++))
			return 0;
	return 1;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPNZ
/*@-boundsread@*/
int mpnz(register size_t size, register const mpw* data)
{
	while (size--)
		if (*(data++))
			return 1;
	return 0;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPEQ
/*@-boundsread@*/
int mpeq(register size_t size, register const mpw* xdata, register const mpw* ydata)
{
	while (size--)
	{
		if (*xdata == *ydata)
		{
			xdata++;
			ydata++;
		}
		else
			return 0;
	}
	return 1;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPEQX
int mpeqx(register size_t xsize, register const mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
		return mpeq(ysize, xdata+diff, ydata) && mpz(diff, xdata);
	}
	else if (xsize < ysize)
	{
		register size_t diff = ysize - xsize;
		return mpeq(xsize, ydata+diff, xdata) && mpz(diff, ydata);
	}
	else
		return mpeq(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MPNE
/*@-boundsread@*/
int mpne(register size_t size, register const mpw* xdata, register const mpw* ydata)
{
	while (size--)
	{
		if (*xdata == *ydata)
		{
			xdata++;
			ydata++;
		}
		else
			return 1;
	}
	return 0;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPNEX
int mpnex(register size_t xsize, register const mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
		return mpnz(diff, xdata) || mpne(ysize, xdata+diff, ydata);
	}
	else if (xsize < ysize)
	{
		register size_t diff = ysize - xsize;
		return mpnz(diff, ydata) || mpne(xsize, ydata+diff, xdata);
	}
	else
		return mpne(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MPGT
/*@-boundsread@*/
int mpgt(register size_t size, register const mpw* xdata, register const mpw* ydata)
{
	while (size--)
	{
		if (*xdata < *ydata)
			return 0;
		if (*xdata > *ydata)
			return 1;
		xdata++; ydata++;
	}
	return 0;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPGTX
int mpgtx(register size_t xsize, register const mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
		return mpnz(diff, xdata) || mpgt(ysize, xdata + diff, ydata);
	}
	else if (xsize < ysize)
	{
		register size_t diff = ysize - xsize;
		return mpz(diff, ydata) && mpgt(xsize, xdata, ydata + diff);
	}
	else
		return mpgt(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MPLT
/*@-boundsread@*/
int mplt(register size_t size, register const mpw* xdata, register const mpw* ydata)
{
	while (size--)
	{
		if (*xdata > *ydata)
			return 0;
		if (*xdata < *ydata)
			return 1;
		xdata++; ydata++;
	}
	return 0;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPLTX
int mpltx(register size_t xsize, register const mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
		return mpz(diff, xdata) && mplt(ysize, xdata+diff, ydata);
	}
	else if (xsize < ysize)
	{
		register size_t diff = ysize - xsize;
		return mpnz(diff, ydata) || mplt(xsize, xdata, ydata+diff);
	}
	else
		return mplt(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MPGE
/*@-boundsread@*/
int mpge(register size_t size, register const mpw* xdata, register const mpw* ydata)
{
	while (size--)
	{
		if (*xdata < *ydata)
			return 0;
		if (*xdata > *ydata)
			return 1;
		xdata++; ydata++;
	}
	return 1;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPGEX
/*@-boundsread@*/
int mpgex(register size_t xsize, register const mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
		return mpnz(diff, xdata) || mpge(ysize, xdata+diff, ydata);
	}
	else if (xsize < ysize)
	{
		register size_t diff = ysize - xsize;
		return mpz(diff, ydata) && mpge(xsize, xdata, ydata+diff);
	}
	else
		return mpge(xsize, xdata, ydata);
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPLE
/*@-boundsread@*/
int mple(register size_t size, register const mpw* xdata, register const mpw* ydata)
{
	while (size--)
	{
		if (*xdata < *ydata)
			return 1;
		if (*xdata > *ydata)
			return 0;
		xdata++; ydata++;
	}
	return 1;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPLEX
/*@-boundsread@*/
int mplex(register size_t xsize, register const mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
		return mpz(diff, xdata) && mple(ysize, xdata+diff, ydata);
	}
	else if (xsize < ysize)
	{
		register size_t diff = ysize - xsize;
		return mpnz(diff, ydata) || mple(xsize, xdata, ydata+diff);
	}
	else
		return mple(xsize, xdata, ydata);
}
/*@=boundsread@*/
#endif


#ifndef ASM_MPISONE
/*@-boundsread@*/
int mpisone(register size_t size, register const mpw* data)
{
	data += size;
	if (*(--data) == 1)
	{
		while (--size)
			if (*(--data))
				return 0;
		return 1;
	}
	return 0;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPISTWO
/*@-boundsread@*/
int mpistwo(register size_t size, register const mpw* data)
{
	data += size;
	if (*(--data) == 2)
	{
		while (--size)
			if (*(--data))
				return 0;
		return 1;
	}
	return 0;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPEQMONE
/*@-boundsread@*/
int mpeqmone(register size_t size, register const mpw* xdata, register const mpw* ydata)
{
    xdata += size;
    ydata += size;

    if (*(--xdata)+1 == *(--ydata))
    {
        while (--size)
            if (*(--xdata) != *(--ydata))
                return 0;
        return 1;
    }
    return 0;
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPLEONE
/*@-boundsread@*/
int mpleone(register size_t size, register const mpw* data)
{
	data += size;
	if (*(--data) > 1)
		return 0;
	else
	{
		while (--size)
			if (*(--data))
				return 0;
		return 1;
	}
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPMSBSET
int mpmsbset(/*@unused@*/ register size_t size, register const mpw* data)
{
/*@-boundsread@*/
	return (int)((*data) >> (MP_WBITS-1));
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPLSBSET
int mplsbset(register size_t size, register const mpw* data)
{
/*@-boundsread@*/
    return (int)(data[size-1] & 0x1);
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPSETMSB
void mpsetmsb(/*@unused@*/ register size_t size, register mpw* data)
{
/*@-boundsread@*/
	*data |= MP_MSBMASK;
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPSETLSB
void mpsetlsb(register size_t size, register mpw* data)
{
/*@-boundsread@*/
	data[size-1] |= MP_LSBMASK;
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPCLRMSB
void mpclrmsb(/*@unused@*/ register size_t size, register mpw* data)
{
/*@-boundsread@*/
	*data &= ~ MP_MSBMASK;
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPCLRLSB
void mpclrlsb(register size_t size, register mpw* data)
{
/*@-boundsread@*/
    data[size-1] &= ~ MP_LSBMASK;
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPAND
void mpand(register size_t size, register mpw* xdata, register const mpw* ydata)
{
/*@-boundsread@*/
	while (size--)
		xdata[size] &= ydata[size];
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPXOR
void mpxor(register size_t size, register mpw* xdata, register const mpw* ydata)
{
/*@-boundsread@*/
	while (size--)
		xdata[size] ^= ydata[size];
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPOR
void mpor(register size_t size, register mpw* xdata, register const mpw* ydata)
{
/*@-boundsread@*/
	while (size--)
		xdata[size] |= ydata[size];
/*@=boundsread@*/
}
#endif

#ifndef ASM_MPNOT
/*@-boundswrite@*/
void mpnot(register size_t size, register mpw* data)
{
	while (size--)
		data[size] = ~data[size];
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPSETW
/*@-boundswrite@*/
void mpsetw(register size_t xsize, register mpw* xdata, register mpw y)
{
	while (--xsize)
		*(xdata++) = 0;
	*(xdata++) = y;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPSETX
/*@-boundswrite@*/
void mpsetx(register size_t xsize, register mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	while (xsize > ysize)
	{
		xsize--;
		*(xdata++) = 0;
	}
	while (ysize > xsize)
	{
		ysize--;
		ydata++;
	}
	while (xsize--)
		*(xdata++) = *(ydata++);
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPADDW
/*@-boundswrite@*/
int mpaddw(register size_t size, register mpw* xdata, register mpw y)
{
	register mpw load, temp;
	register int carry = 0;

	xdata += size-1;

	load = *xdata;
	temp = load + y;
	*(xdata--) = temp;
	carry = (load > temp);

	while (--size && carry)
	{
		load = *xdata;
		temp = load + 1;
		*(xdata--) = temp;
		carry = (load > temp);
	}
	return carry;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPADD
/*@-boundswrite@*/
int mpadd(register size_t size, register mpw* xdata, register const mpw* ydata)
{
	register mpw load, temp;
	register int carry = 0;

	xdata += size-1;
	ydata += size-1;

	while (size--)
	{
		temp = *(ydata--);
		load = *xdata;
		temp = carry ? (load + temp + 1) : (load + temp);
		*(xdata--) = temp;
		carry = carry ? (load >= temp) : (load > temp);
	}
	return carry;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPADDX
int mpaddx(register size_t xsize, register mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
		return mpaddw(diff, xdata, (mpw) mpadd(ysize, xdata+diff, ydata));
	}
	else
	{
		register size_t diff = ysize - xsize;
		return mpadd(xsize, xdata, ydata+diff);
	}
}
#endif

#ifndef ASM_MPSUBW
/*@-boundswrite@*/
uint32 mpsubw(register size_t xsize, register mpw* xdata, register mpw y)
int
	register uint64 temp;
	register uint32 carry = 0;

	xdata += xsize;
	temp = *(--xdata);
	temp -= y;
	*xdata = (uint32) temp;
	carry = (temp >> 32) ? 1 : 0;
	while (--xsize && carry)
	{
		temp = *(--xdata);
		temp -= carry;
		*xdata = (uint32) temp;
		carry = (temp >> 32) ? 1 : 0;
	}
	return carry;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPSUB
/*@-boundswrite@*/
uint32 mpsub(register size_t size, register mpw* xdata, register const mpw* ydata)
{
	register uint64 temp;
	register uint32 carry = 0;

	xdata += size;
	ydata += size;

	while (size--)
	{
		temp = *(--xdata);
		temp -= *(--ydata);
		temp -= carry;
		*xdata = (uint32) temp;
		carry = (temp >> 32) != 0;
	}
	return carry;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPSUBX
uint32 mpsubx(register size_t xsize, register mpw* xdata, register size_t ysize, register const mpw* ydata)
{
	if (xsize > ysize)
	{
		register uint32 diff = xsize - ysize;
		register uint32 carry = mpsub(ysize, xdata + diff, ydata);
		return mpsubw(diff, xdata, carry);
	}
	else
	{
		register uint32 diff = ysize - xsize;
		return mpsub(xsize, xdata, ydata + diff);
	}
}
#endif

#ifndef ASM_MPNEG
void mpneg(register size_t xsize, register mpw* xdata)
{
	mpnot(xsize, xdata);
	(void) mpaddw(xsize, xdata, 1);
}
#endif

#ifndef ASM_MPSETMUL
/*@-boundswrite@*/
uint32 mpsetmul(register size_t size, register mpw* result, register const mpw* xdata, register mpw y)
{
	register uint64 temp;
	register uint32 carry = 0;

	xdata  += size;
	result += size;

	while (size--)
	{
		temp = *(--xdata);
		temp *= y;
		temp += carry;
		*(--result) = (uint32) temp;
		carry = (uint32) (temp >> 32);
	}
	return carry;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPADDMUL
/*@-boundswrite@*/
uint32 mpaddmul(register size_t size, register mpw* result, register const mpw* xdata, register mpw y)
{
	register uint64 temp;
	register uint32 carry = 0;

	xdata  += size;
	result += size;

	while (size--)
	{
		temp = *(--xdata);
		temp *= y;
		temp += carry;
		temp += *(--result);
		*result = (uint32) temp;
		carry = (uint32) (temp >> 32);
	}
	return carry;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPMUL
/*@-boundswrite@*/
void mpmul(mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
{
	/*@-mods@*/
	/* preferred passing of parameters is x the larger of the two numbers */
	if (xsize >= ysize)
	{
		register uint32 rc;

		result += ysize;
		ydata += ysize;

		rc = mpsetmul(xsize, result, xdata, *(--ydata));
		*(--result) = rc;

		while (--ysize)
		{
			rc = mpaddmul(xsize, result, xdata, *(--ydata));
			*(--result) = rc;
		}
	}
	else
	{
		register uint32 rc;

		result += xsize;
		xdata += xsize;

		rc = mpsetmul(ysize, result, ydata, *(--xdata));
		*(--result) = rc;

		while (--xsize)
		{
			rc = mpaddmul(ysize, result, ydata, *(--xdata));
			*(--result) = rc;
		}
	}
	/*@=mods@*/
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPADDSQRTRC
/*@-boundswrite@*/
mpw mpaddsqrtrc(register size_t size, register mpw* result, register const mpw* data)
{
	register uint64 temp;
	register uint32 n, carry = 0;

	result += size*2;

	while (size--)
	{
		temp = n = data[size];
		temp *= n;
		temp += carry;
		temp += *(--result);
		*result = (uint32) temp;
		temp >>= 32;
		temp += *(--result);
		*result = (uint32) temp;
		carry = (uint32) (temp >> 32);
	}
	return carry;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPSQR
/*@-boundswrite@*/
void mpsqr(register mpw* result, register size_t size, register const mpw* data)
{
	register size_t n = size-1;

	/*@-mods@*/
	result += size;
	result[n] = 0;

	if (n)
	{
		*(--result) = mpsetmul(n, result, data, data[n]);
		while (--n)
			*(--result) = mpaddmul(n, result, data, data[n]);
	}

	*(--result) = 0;

	(void) mpmultwo(size*2, result);

	(void) mpaddsqrtrc(size, result, data);
	/*@=mods@*/
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPSIZE
size_t mpsize(register size_t size, register const mpw* data)
{
	while (size)
	{
/*@-boundsread@*/
		if (*data)
			return size;
/*@=boundsread@*/
		data++;
		size--;
	}
	return 0;
}
#endif

#ifndef ASM_MPNORM
uint32 mpnorm(register size_t size, register mpw* data)
{
	register size_t shift = mpmszcnt(size, data);
	mplshift(size, data, shift);
	return shift;
}
#endif

#ifndef ASM_MPDIVPOWTWO
/* need to eliminate this function, as it is not aptly named */
uint32 mpdivpowtwo(register size_t size, register mpw* data)
{
	return mprshiftlsz(size, data);
}
#endif

#ifndef ASM_MPDIVTWO
/*@-boundswrite@*/
void mpdivtwo(register size_t size, register mpw* data)
{
	register mpw temp;
	register mpw carry = 0;

	while (size--)
	{
		temp = *data;
		*(data++) = (temp >> 1) | carry;
		carry = (temp << (MP_WBITS-1));
	}
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPSDIVTWO
void mpsdivtwo(register size_t size, register mpw* data)
{
	int carry = mpmsbset(size, data);
	mpdivtwo(size, data);
	if (carry)
		mpsetmsb(size, data);
}
#endif

#ifndef ASM_MPMULTWO
/*@-boundswrite@*/
uint32 mpmultwo(register size_t size, register mpw* data)
{
	register mpw temp;
	register mpw carry = 0;

	data += size;
	while (size--)
	{
		temp = *(--data);
		*data = (temp << 1) | carry;
		carry = (temp >> (MP_WBITS -1));
	}
	return (int) carry;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPMSZCNT
uint32 mpmszcnt(register size_t size, register const mpw* data)
{
	register size_t zbits = 0;
	register size_t i = 0;

	while (i < size)
	{
/*@-boundsread@*/
		register mpw temp = data[i++];
/*@=boundsread@*/
		if (temp)
		{
			while (!(temp & MP_MSBMASK))
			{
				zbits++;
				temp <<= 1;
			}
			break;
		}
		else
			zbits += MP_WBITS;
	}
	return zbits;
}
#endif

#ifndef ASM_MPBITCNT
size_t mpbitcnt(register size_t size, register const mpw* data)
{
	register mpw xmask = (mpw)((*data & MP_MSBMASK) ? -1 : 0);
	register size_t nbits = MP_WBITS * size;
	register size_t i = 0;

	while (i < size) {
/*@-boundsread@*/
		register mpw temp = (data[i++] ^ xmask);
/*@=boundsread@*/
		if (temp) {
			while (!(temp & MP_MSBMASK)) {
				nbits--;
				temp <<= 1;
			}
			break;
		} else
			nbits -= MP_WBITS;
	}
	return nbits;
}
#endif

#ifndef ASM_MPLSZCNT
size_t mplszcnt(register size_t size, register const mpw* data)
{
	register size_t zbits = 0;

	while (size--)
	{
/*@-boundsread@*/
		register mpw temp = data[size];
/*@=boundsread@*/
		if (temp)
		{
			while (!(temp & MP_LSBMASK))
			{
				zbits++;
				temp >>= 1;
			}
			break;
		}
		else
			zbits += MP_WBITS;
	}
	return zbits;
}
#endif

#ifndef ASM_MPLSHIFT
/*@-boundswrite@*/
void mplshift(register size_t size, register mpw* data, size_t count)
{
	register size_t words = MP_BITS_TO_WORDS(count);

	if (words < size)
	{
		register short lbits = (short) (count & (MP_WBITS-1));

		/* first do the shifting, then do the moving */
		if (lbits != 0)
		{
			register mpw temp;
			register mpw carry = 0;
			register short rbits = MP_WBITS - lbits;
			register size_t i = size;

			while (i > words)
			{
				temp = data[--i];
				data[i] = (temp << lbits) | carry;
				carry = (temp >> rbits);
			}
		}
		if (words)
		{
			mpmove(size-words, data, data+words);
			mpzero(words, data+size-words);
		}
	}
	else
		mpzero(size, data);
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPRSHIFT
/*@-boundswrite@*/
void mprshift(register size_t size, register mpw* data, uint32 count)
{
	register size_t words = MP_BITS_TO_WORDS(count);

	if (words < size)
	{
		register uint8 rbits = (uint8) (count & 0x1f);

		/* first do the shifting, then do the moving */
		if (rbits != 0)
		{
			register uint32 temp;
			register uint32 carry = 0;
			register uint8  lbits = 32-rbits;
			register uint32 i = 0;

			while (i < size-words)
			{
				temp = data[i];
				data[i++] = (temp >> rbits) | carry;
				carry = (temp << lbits);
			}
		}
		if (words)
		{
			mpmove(size-words, data+words, data);
			mpzero(words, data);
		}
	}
	else
		mpzero(size, data);
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPRSHIFTLSZ
/* x must be != 0 */
/*@-boundswrite@*/
uint32 mprshiftlsz(register size_t size, register mpw* data)
{
	register mpw* slide = data+size-1;
	register uint32  zwords = 0; /* counter for 'all zero bit' words */
	register uint32  lbits, rbits = 0; /* counter for 'least significant zero' bits */
	register uint32  temp, carry = 0;

	data = slide;

	/* count 'all zero' words and move src pointer */
	while (size--)
	{
		/* test if we a non-zero word */
		if ((carry = *(slide--)))
		{
			/* count 'least signification zero bits and set zbits counter */
			while (!(carry & 0x1))
			{
				carry >>= 1;
				rbits++;
			}
			break;
		}
		zwords++;
	}

	/* shouldn't happen, but let's test anyway */
	if (size == 0)
		return 0;

	/* prepare right-shifting of data */
	lbits = 32-rbits;

	/* shift data */
	while (size--)
	{
		temp = *(slide--);
		*(data--) = (temp << lbits) | carry;
		carry = (temp >> rbits);
	}

	/* store the final carry */
	*(data--) = carry;

	/* store the return value in temp */
	temp = (zwords << 5) + rbits;

	/* zero the (zwords) most significant words */
	while (zwords--)
		*(data--) = 0;

	return temp;
}
/*@=boundswrite@*/
#endif

/* try an alternate version here, with descending sizes */
/* also integrate lszcnt and rshift properly into one function */
#ifndef ASM_MPGCD_W
/*@-boundswrite@*/
/**
 * mpgcd_w
 *  need workspace of (size) words
 */
void mpgcd_w(size_t size, const mpw* xdata, const mpw* ydata, mpw* result, mpw* wksp)
{
	register uint32 shift = 0;
	register uint32 temp;

	if (mpge(size, xdata, ydata))
	{
		mpcopy(size, wksp, xdata);
		mpcopy(size, result, ydata);
	}
	else
	{
		mpcopy(size, wksp, ydata);
		mpcopy(size, result, xdata);
	}
		
	/* get the smallest returned values, and set shift to that */

	shift = mprshiftlsz(size, wksp);

	temp = mprshiftlsz(size, result);

	if (shift > temp)
		shift = temp;

	while (mpnz(size, wksp))
	{
		(void) mprshiftlsz(size, wksp);
		(void) mprshiftlsz(size, result);

		if (mpge(size, wksp, result))
			(void) mpsub(size, wksp, result);
		else
			(void) mpsub(size, result, wksp);

		/* slide past zero words in both operands by increasing pointers and decreasing size */
		if ((*wksp == 0) && (*result == 0))
		{
			size--;
			wksp++;
			result++;
		}
	}

	/* figure out if we need to slide the result pointer back */
	if ((temp = shift >> 5))
	{
		size += temp;
		result -= temp;
	}

	mplshift(size, result, shift);
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPNMODW
/*@-boundswrite@*/
uint32 mpnmodw(mpw* result, size_t xsize, const mpw* xdata, mpw y, mpw* wksp)
{
	/* result size xsize, wksp size xsize+1 */
	register uint64 temp;
	register uint32 q;
	size_t qsize = xsize-1;
	mpw* rdata = result;

	mpcopy(xsize, rdata, xdata);
	/*
		if (*rdata >= y)
			*rdata -= y;
	*/
	if (mpge(1, rdata, &y))
		(void) mpsub(1, rdata, &y);

	while (qsize--)
	{
		/* fprintf(stderr, "result = "); MP32println(stderr, xsize+1, result); */
		/* get the two high words of r into temp */
		temp = rdata[0];
		temp <<= 32;
		temp += rdata[1];
		/* fprintf(stderr, "q = %016llx / %08lx\n", temp, msw); */
		temp /= y;
		/*
			temp *= y;
			wksp[0] = (uint32) (temp >> 32);
			wksp[1] = (uint32) (temp);
		*/
		q = (uint32) temp;

		/* fprintf(stderr, "q = %08x\n", q); */
		/*@-evalorder@*/
		*wksp = mpsetmul(1, wksp+1, &y, q);
		/*@=evalorder@*/

		/* fprintf(stderr, "mplt "); mpprint(2, rdata); fprintf(stderr, " < "); mpprintln(stderr, 2, wksp); */
		while (mplt(2, rdata, wksp))
		{
			/* fprintf(stderr, "mplt! "); mpprint(2, rdata); fprintf(stderr, " < "); mpprintln(stderr, 2, wksp); */
			/* fprintf(stderr, "decreasing q\n"); */
			(void) mpsubx(2, wksp, 1, &y);
			/* q--; */
		}
		/* fprintf(stderr, "subtracting\n"); */
		(void) mpsub(2, rdata, wksp);
		rdata++;
	}

	return *rdata;
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPNMOD
/*@-boundswrite@*/
void mpnmod(mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* wksp)
{
	/* result size xsize, wksp size xsize+1 */
	register uint64 temp;
	register uint32 q;
	mpw msw = *ydata;
	size_t qsize = xsize-ysize;
	mpw* rdata = result;

	mpcopy(xsize, rdata, xdata);
	if (mpge(ysize, rdata, ydata))
		(void) mpsub(ysize, rdata, ydata);

	while (qsize--)
	{
		/* fprintf(stderr, "result = "); mpprintln(stderr, xsize+1, result); */
		/* get the two high words of r into temp */
		temp = rdata[0];
		temp <<= 32;
		temp += rdata[1];
		/* fprintf(stderr, "q = %016llx / %08lx\n", temp, msw); */
		temp /= msw;
		q = (uint32) temp;

		/* fprintf(stderr, "q = %08x\n", q); */
		/*@-evalorder@*/
		*wksp = mpsetmul(ysize, wksp+1, ydata, q);
		/*@=evalorder@*/

		/* fprintf(stderr, "mplt "); mpprint(ysize+1, rdata); fprintf(stderr, " < "); mpprintln(stderr, ysize+1, wksp); */
		while (mplt(ysize+1, rdata, wksp))
		{
			/* fprintf(stderr, "mplt! "); mpprint(ysize+1, rdata); fprintf(stderr, " < "); mpprintln(stderr, ysize+1, wksp); */
			/* fprintf(stderr, "decreasing q\n"); */
			(void) mpsubx(ysize+1, wksp, ysize, ydata);
			q--;
		}
		/* fprintf(stderr, "subtracting\n"); */
		(void) mpsub(ysize+1, rdata, wksp);
		rdata++;
	}
}
/*@=boundswrite@*/
#endif

#ifndef ASM_MPNDIVMOD
/*@-boundswrite@*/
void mpndivmod(mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, register mpw* wksp)
{
	/* result must be xsize+1 in length */
	/* wksp must be ysize+1 in length */
	/* expect ydata to be normalized */
	register uint64 temp;
	register uint32 q;
	mpw msw = *ydata;
	size_t qsize = xsize-ysize;

	mpcopy(xsize, result+1, xdata);
	/*@-compdef@*/ /* LCL: result+1 undefined */
	if (mpge(ysize, result+1, ydata))
	{
		/* fprintf(stderr, "subtracting\n"); */
		(void) mpsub(ysize, result+1, ydata);
		*(result++) = 1;
	}
	else
		*(result++) = 0;
	/*@=compdef@*/

	/*@-usedef@*/	/* LCL: result[0] is set */
	while (qsize--)
	{
		/* fprintf(stderr, "result = "); mpprintln(stderr, xsize+1, result); */
		/* get the two high words of r into temp */
		temp = result[0];
		temp <<= 32;
		temp += result[1];
		/* fprintf(stderr, "q = %016llx / %08lx\n", temp, msw); */
		temp /= msw;
		q = (uint32) temp;

		/* fprintf(stderr, "q = %08x\n", q); */

		/*@-evalorder@*/
		*wksp = mpsetmul(ysize, wksp+1, ydata, q);
		/*@=evalorder@*/

		/* fprintf(stderr, "mplt "); mpprint(ysize+1, result); fprintf(stderr, " < "); mpprintln(stderr, ysize+1, wksp); */
		while (mplt(ysize+1, result, wksp))
		{
			/* fprintf(stderr, "mplt! "); mpprint(ysize+1, result); fprintf(stderr, " < "); mpprintln(stderr, ysize+1, wksp); */
			/* fprintf(stderr, "decreasing q\n"); */
			(void) mpsubx(ysize+1, wksp, ysize, ydata);
			q--;
		}
		/* fprintf(stderr, "subtracting\n"); */
		(void) mpsub(ysize+1, result, wksp);
		*(result++) = q;
	}
	/*@=usedef@*/
}
/*@=boundswrite@*/
#endif

/*
#ifndef ASM_MPUNPACK
void mpunpack(size_t size, uint8* bytes, const mpw* bits)
{
	register uint32 temp;
	register int i;
	
	while (size--)
	{
		temp = *(bits++);

		for (i = 0; i < 31; i++)
		{
			bytes
		}
	}
}
#endif
*/

#ifndef ASM_MPPRINT
/*@-boundsread@*/
void mpprint(register FILE * fp, register size_t xsize, register const mpw* xdata)
{
	if (xdata == NULL)
	 	return;
	if (fp == NULL)
		fp = stderr;
	while (xsize--)
		fprintf(fp, "%08x", *(xdata++));
	(void) fflush(fp);
}
/*@=boundsread@*/
#endif

#ifndef ASM_MPPRINTLN
/*@-boundsread@*/
void mpprintln(register FILE * fp, register size_t xsize, register const mpw* xdata)
{
	if (xdata == NULL)
	 	return;
	if (fp == NULL)
		fp = stderr;
	while (xsize--)
		fprintf(fp, "%08x", *(xdata++));
	fprintf(fp, "\n");
	(void) fflush(fp);
}
/*@=boundsread@*/
#endif
