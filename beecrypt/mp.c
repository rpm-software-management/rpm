/*
 * Copyright (c) 2002, 2003 Bob Deblier
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

/*!\file mp.c
 * \brief Multi-precision integer routines.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP_m
 */

#include "system.h"
#include "beecrypt.h"
#include "mpopt.h"
#include "mp.h"
#include "debug.h"

#ifndef ASM_MPZERO
void mpzero(size_t size, mpw* data)
{
	while (size--)
		*(data++) = 0;
}
#endif

#ifndef ASM_MPFILL
void mpfill(size_t size, mpw* data, mpw fill)
{
	while (size--)
		*(data++) = fill;
}
#endif

#ifndef ASM_MPODD
int mpodd(size_t size, const mpw* data)
{
	return (int)(data[size-1] & 0x1);
}
#endif

#ifndef ASM_MPEVEN
int mpeven(size_t size, const mpw* data)
{
	return !(int)(data[size-1] & 0x1);
}
#endif

#ifndef ASM_MPZ
int mpz(size_t size, const mpw* data)
{
	while (size--)
		if (*(data++))
			return 0;
	return 1;
}
#endif

#ifndef ASM_MPNZ
int mpnz(size_t size, const mpw* data)
{
	while (size--)
		if (*(data++))
			return 1;
	return 0;
}
#endif

#ifndef ASM_MPEQ
int mpeq(size_t size, const mpw* xdata, const mpw* ydata)
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
#endif

#ifndef ASM_MPEQX
int mpeqx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
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
int mpne(size_t size, const mpw* xdata, const mpw* ydata)
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
#endif

#ifndef ASM_MPNEX
int mpnex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
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
int mpgt(size_t size, const mpw* xdata, const mpw* ydata)
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
#endif

#ifndef ASM_MPGTX
int mpgtx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
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
int mplt(size_t size, const mpw* xdata, const mpw* ydata)
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
#endif

#ifndef ASM_MPLTX
int mpltx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
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
int mpge(size_t size, const mpw* xdata, const mpw* ydata)
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
#endif

#ifndef ASM_MPGEX
int mpgex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
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
#endif

#ifndef ASM_MPLE
int mple(size_t size, const mpw* xdata, const mpw* ydata)
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
#endif

#ifndef ASM_MPLEX
int mplex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
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
#endif


#ifndef ASM_MPISONE
int mpisone(size_t size, const mpw* data)
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
#endif

#ifndef ASM_MPISTWO
int mpistwo(size_t size, const mpw* data)
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
#endif

#ifndef ASM_MPEQMONE
int mpeqmone(size_t size, const mpw* xdata, const mpw* ydata)
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
#endif

#ifndef ASM_MPLEONE
int mpleone(size_t size, const mpw* data)
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
#endif

#ifndef ASM_MPMSBSET
int mpmsbset(/*@unused@*/ size_t size, const mpw* data)
{
	return (int)((*data) >> (MP_WBITS-1));
}
#endif

#ifndef ASM_MPLSBSET
int mplsbset(size_t size, const mpw* data)
{
    return (int)(data[size-1] & 0x1);
}
#endif

#ifndef ASM_MPSETMSB
void mpsetmsb(/*@unused@*/ size_t size, mpw* data)
{
	*data |= MP_MSBMASK;
}
#endif

#ifndef ASM_MPSETLSB
void mpsetlsb(size_t size, mpw* data)
{
	data[size-1] |= MP_LSBMASK;
}
#endif

#ifndef ASM_MPCLRMSB
void mpclrmsb(/*@unused@*/ size_t size, mpw* data)
{
	*data &= ~ MP_MSBMASK;
}
#endif

#ifndef ASM_MPCLRLSB
void mpclrlsb(size_t size, mpw* data)
{
    data[size-1] &= ~ MP_LSBMASK;
}
#endif

#ifndef ASM_MPAND
void mpand(size_t size, mpw* xdata, const mpw* ydata)
{
	while (size--)
		xdata[size] &= ydata[size];
}
#endif

#ifndef ASM_MPXOR
void mpxor(size_t size, mpw* xdata, const mpw* ydata)
{
	while (size--)
		xdata[size] ^= ydata[size];
}
#endif

#ifndef ASM_MPOR
void mpor(size_t size, mpw* xdata, const mpw* ydata)
{
	while (size--)
		xdata[size] |= ydata[size];
}
#endif

#ifndef ASM_MPNOT
void mpnot(size_t size, mpw* data)
{
	while (size--)
		data[size] = ~data[size];
}
#endif

#ifndef ASM_MPSETW
void mpsetw(size_t size, mpw* xdata, mpw y)
{
	while (--size)
		*(xdata++) = 0;
	*(xdata++) = y;
}
#endif

#ifndef ASM_MPSETX
void mpsetx(size_t xsize, mpw* xdata, size_t ysize, const mpw* ydata)
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
#endif

#ifndef ASM_MPADDW
int mpaddw(size_t size, mpw* xdata, mpw y)
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
#endif

#ifndef ASM_MPADD
int mpadd(size_t size, mpw* xdata, const mpw* ydata)
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
#endif

#ifndef ASM_MPADDX
int mpaddx(size_t xsize, mpw* xdata, size_t ysize, const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
/*@-evalorder@*/
		return mpaddw(diff, xdata, (mpw) mpadd(ysize, xdata+diff, ydata));
/*@=evalorder@*/
	}
	else
	{
		register size_t diff = ysize - xsize;
		return mpadd(xsize, xdata, ydata+diff);
	}
}
#endif

#ifndef ASM_MPSUBW
int mpsubw(size_t size, mpw* xdata, mpw y)
{
	register mpw load, temp;
	register int carry = 0;

	xdata += size-1;

	load = *xdata;
	temp = load - y;
	*(xdata--) = temp;
	carry = (load < temp);

	while (--size && carry)
	{
		load = *xdata;
		temp = load - 1;
		*(xdata--) = temp;
		carry = (load < temp);	
	}
	return carry;
}
#endif

#ifndef ASM_MPSUB
int mpsub(size_t size, mpw* xdata, const mpw* ydata)
{
	register mpw load, temp;
	register int carry = 0;

	xdata += size-1;
	ydata += size-1;

	while (size--)
	{
		temp = *(ydata--);
		load = *xdata;
		temp = carry ? (load - temp - 1) : (load - temp);
		*(xdata--) = temp;
		carry = carry ? (load <= temp) : (load < temp);
	}
	return carry;
}
#endif

#ifndef ASM_MPSUBX
int mpsubx(size_t xsize, mpw* xdata, size_t ysize, const mpw* ydata)
{
	if (xsize > ysize)
	{
		register size_t diff = xsize - ysize;
/*@-evalorder@*/
		return mpsubw(diff, xdata, (mpw) mpsub(ysize, xdata+diff, ydata));
/*@=evalorder@*/
	}
	else
	{
		register size_t diff = ysize - xsize;
		return mpsub(xsize, xdata, ydata+diff);
	}
}
#endif

#ifndef ASM_MPNEG
void mpneg(size_t size, mpw* data)
{
	mpnot(size, data);
	(void) mpaddw(size, data, 1);
}
#endif

#ifndef ASM_MPSETMUL
mpw mpsetmul(size_t size, mpw* result, const mpw* data, mpw y)
{
	#if HAVE_MPDW
	register mpdw temp;
	register mpw  carry = 0;

	data += size;
	result += size;

	while (size--)
	{
		temp = *(--data);
		temp *= y;
		temp += carry;
		*(--result) = (mpw) temp;
		carry = (mpw)(temp >> MP_WBITS);
	}
	#else
	register mpw temp, load, carry = 0;
	register mphw ylo, yhi;

	ylo = (mphw)  y;
	yhi = (mphw) (y >> MP_HWBITS);

	data += size;
	result += size;

	while (size--)
	{
		register mphw xlo, xhi;
		register mpw rlo, rhi;

		xlo = (mphw) (temp = *(--data));
		xhi = (mphw) (temp >> MP_HWBITS);

		rlo = (mpw) xlo * ylo;
		rhi = (mpw) xhi * yhi;
		load = rlo;
		temp = (mpw) xhi * ylo;
		rlo += (temp << MP_HWBITS);
		rhi += (temp >> MP_HWBITS) + (load > rlo);
		load = rlo;
		temp = (mpw) xlo * yhi;
		rlo += (temp << MP_HWBITS);
		rhi += (temp >> MP_HWBITS) + (load > rlo);
		load = rlo;
		temp = rlo + carry;
		carry = rhi + (load > temp);
		*(--result) = temp;
	}
	#endif
	return carry;
}
#endif

#ifndef ASM_MPADDMUL
mpw mpaddmul(size_t size, mpw* result, const mpw* data, mpw y)
{
	#if HAVE_MPDW
	register mpdw temp;
	register mpw  carry = 0;

	data += size;
	result += size;

	while (size--)
	{
		temp = *(--data);
		temp *= y;
		temp += carry;
		temp += *(--result);
		*result = (mpw) temp;
		carry = (mpw)(temp >> MP_WBITS);
	}
	#else
	register mpw temp, load, carry = 0;
	register mphw ylo, yhi;

	ylo = (mphw)  y;
	yhi = (mphw) (y >> MP_HWBITS);

	data += size;
	result += size;

	while (size--)
	{
		register mphw xlo, xhi;
		register mpw rlo, rhi;

		xlo = (mphw) (temp = *(--data));
		xhi = (mphw) (temp >> MP_HWBITS);

		rlo = (mpw) xlo * ylo;
		rhi = (mpw) xhi * yhi;
		load = rlo;
		temp = (mpw) xhi * ylo;
		rlo += (temp << MP_HWBITS);
		rhi += (temp >> MP_HWBITS) + (load > rlo);
		load = rlo;
		temp = (mpw) xlo * yhi;
		rlo += (temp << MP_HWBITS);
		rhi += (temp >> MP_HWBITS) + (load > rlo);
		load = rlo;
		rlo += carry;
		temp = (load > rlo);
		load = rhi;
		rhi += temp;
		carry = (load > rhi);
		load = rlo;
		rlo += *(--result);
		*result = rlo;
		carry += rhi + (load > rlo);
	}
	#endif
	return carry;
}
#endif

#ifndef ASM_MPMUL
void mpmul(mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
{
	/* preferred passing of parameters is x the larger of the two numbers */
	if (xsize >= ysize)
	{
		register mpw rc;

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
		register mpw rc;

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
}
#endif

#ifndef ASM_MPADDSQRTRC
void mpaddsqrtrc(size_t size, mpw* result, const mpw* data)
{
	#if HAVE_MPDW
	register mpdw temp;
	register mpw  load, carry = 0;

	result += (size << 1);

	while (size--)
	{
		temp = load = data[size];
		temp *= load;
		temp += carry;
		temp += *(--result);
		*result = (mpw) temp;
		temp >>= MP_WBITS;
		temp += *(--result);
		*result = (mpw) temp;
		carry = (mpw)(temp >> MP_WBITS);
	}
	#else
	register mpw temp, load, carry = 0;

	result += (size << 1);

	while (size--)
	{
		register mphw xlo, xhi;
		register mpw rlo, rhi;

		xlo = (mphw) (temp = data[size]);
		xhi = (mphw) (temp >> MP_HWBITS);

		rlo = (mpw) xlo * xlo;
		rhi = (mpw) xhi * xhi;
		temp = (mpw) xhi * xlo;
		load = rlo;
		rlo += (temp << MP_HWBITS);
		rhi += (temp >> MP_HWBITS) + (load > rlo);
		load = rlo;
		rlo += (temp << MP_HWBITS);
		rhi += (temp >> MP_HWBITS) + (load > rlo);
		load = rlo;
		rlo += carry;
		rhi += (load > rlo);
		load = rlo;
		rlo += *(--result);
		*result = rlo;
		temp = (load > rlo);
		load = rhi;
		rhi += temp;
		carry = (load > rhi);
		load = rhi;
		rhi += *(--result);
		*result = rhi;
		carry += (load > rhi);
	}
	#endif
}
#endif

#ifndef ASM_MPSQR
void mpsqr(mpw* result, size_t size, const mpw* data)
{
	register mpw rc;
	register size_t n = size-1;

	result += size;
	result[n] = 0;

	if (n)
	{
		rc = mpsetmul(n, result, data, data[n]);
		*(--result) = rc;
		while (--n)
		{
			rc = mpaddmul(n, result, data, data[n]);
			*(--result) = rc;
		}
	}

	*(--result) = 0;

	(void) mpmultwo(size*2, result);

	(void) mpaddsqrtrc(size, result, data);
}
#endif

#ifndef ASM_MPSIZE
size_t mpsize(size_t size, const mpw* data)
{
	while (size)
	{
		if (*data)
			return size;
		data++;
		size--;
	}
	return 0;
}
#endif

#ifndef ASM_MPBITS
size_t mpbits(size_t size, const mpw* data)
{
	return MP_WORDS_TO_BITS(size) - mpmszcnt(size, data); 
}
#endif

#ifndef ASM_MPNORM
size_t mpnorm(size_t size, mpw* data)
{
	register size_t shift = mpmszcnt(size, data);
	mplshift(size, data, shift);
	return shift;
}
#endif

#ifndef ASM_MPDIVTWO
void mpdivtwo(size_t size, mpw* data)
{
	register mpw temp, carry = 0;

	while (size--)
	{
		temp = *data;
		*(data++) = (temp >> 1) | carry;
		carry = (temp << (MP_WBITS-1));
	}
}
#endif

#ifndef ASM_MPSDIVTWO
void mpsdivtwo(size_t size, mpw* data)
{
	int carry = mpmsbset(size, data);
	mpdivtwo(size, data);
	if (carry)
		mpsetmsb(size, data);
}
#endif

#ifndef ASM_MPMULTWO
int mpmultwo(size_t size, mpw* data)
{
	register mpw temp, carry = 0;

	data += size;
	while (size--)
	{
		temp = *(--data);
		*data = (temp << 1) | carry;
		carry = (temp >> (MP_WBITS-1));
	}
	return (int) carry;
}
#endif

#ifndef ASM_MPMSZCNT
size_t mpmszcnt(size_t size, const mpw* data)
{
	register size_t zbits = 0;
	register size_t i = 0;

	while (i < size)
	{
		register mpw temp = data[i++];
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
size_t mpbitcnt(size_t size, const mpw* data)
{
	register mpw xmask = (mpw)((*data & MP_MSBMASK) ? -1 : 0);
	register size_t nbits = MP_WBITS * size;
	register size_t i = 0;

	while (i < size) {
		register mpw temp = (data[i++] ^ xmask);
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
size_t mplszcnt(size_t size, const mpw* data)
{
	register size_t zbits = 0;

	while (size--)
	{
		register mpw temp = data[size];
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
void mplshift(size_t size, mpw* data, size_t count)
{
	register size_t words = MP_BITS_TO_WORDS(count);

	if (words < size)
	{
		register unsigned short lbits = (unsigned short) (count & (MP_WBITS-1));

		/* first do the shifting, then do the moving */
		if (lbits != 0)
		{
			register mpw temp, carry = 0;
			register unsigned int rbits = MP_WBITS - lbits;
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
#endif

#ifndef ASM_MPRSHIFT
void mprshift(size_t size, mpw* data, size_t count)
{
	register size_t words = MP_BITS_TO_WORDS(count);

	if (words < size)
	{
		register unsigned short rbits = (unsigned short) (count & (MP_WBITS-1));

		/* first do the shifting, then do the moving */
		if (rbits != 0)
		{
			register mpw temp, carry = 0;
			register unsigned int lbits = MP_WBITS - rbits;
			register size_t i = 0;

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
#endif

#ifndef ASM_MPRSHIFTLSZ
/* x must be != 0 */
size_t mprshiftlsz(size_t size, mpw* data)
{
	register mpw* slide = data+size-1;
	register size_t  zwords = 0; /* counter for 'all zero bit' words */
	register unsigned int lbits, rbits = 0; /* counter for 'least significant zero' bits */
	register mpw  temp, carry = 0;

	data = slide;

	/* count 'all zero' words and move src pointer */
	while (size--)
	{
		/* test if we a non-zero word */
		if ((carry = *(slide--)))
		{
			/* count 'least signification zero bits and set zbits counter */
			while (!(carry & MP_LSBMASK))
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
	lbits = MP_WBITS - rbits;

	/* shift data */
	while (size--)
	{
		temp = *(slide--);
		*(data--) = (temp << lbits) | carry;
		carry = (temp >> rbits);
	}

	/* store the final carry */
	*(data--) = carry;

	/* store the return value in size */
	size = MP_WORDS_TO_BITS(zwords) + rbits;

	/* zero the (zwords) most significant words */
	while (zwords--)
		*(data--) = 0;

	return size;
}
#endif

/* try an alternate version here, with descending sizes */
/* also integrate lszcnt and rshift properly into one function */
#ifndef ASM_MPGCD_W
/**
 * mpgcd_w
 *  need workspace of (size) words
 */
void mpgcd_w(size_t size, const mpw* xdata, const mpw* ydata, mpw* result, mpw* wksp)
{
	register size_t shift = 0, temp;

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
	if ((temp = MP_BITS_TO_WORDS(shift)))
	{
		size += temp;
		result -= temp;
	}

	mplshift(size, result, shift);
}
#endif

#ifndef ASM_MPPNDIV
mpw mppndiv(mpw xhi, mpw xlo, mpw y)
{
	register mpw result = 0;
	register unsigned int count = MP_WBITS;
	register unsigned int carry = 0;

	while (count--)
	{
		if (((unsigned)carry) | (unsigned)(xhi >= y))
		{
			xhi -= y;
			result |= 1;
		}
		carry = (xhi >> (MP_WBITS-1));
		xhi <<= 1;
		xhi |= (xlo >> (MP_WBITS-1));
		xlo <<= 1;
		result <<= 1;
	}
	if (((unsigned)carry) | (unsigned)(xhi >= y))
	{
		xhi -= y;
		result |= 1;
	}
	return result;
}
#endif

#ifndef ASM_MPNMODW
mpw mpnmodw(mpw* result, size_t xsize, const mpw* xdata, mpw y, mpw* workspace)
{
	/* result size xsize, workspace size xsize+1 */
	register mpw q;
	mpw  qsize = xsize-1;
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
		q = mppndiv(rdata[0], rdata[1], y);

/*@-evalorder@*/
		*workspace = mpsetmul(1, workspace+1, &y, q);
/*@=evalorder@*/

		while (mplt(2, rdata, workspace))
		{
			(void) mpsubx(2, workspace, 1, &y);
			/* q--; */
		}
		(void) mpsub(2, rdata, workspace);
		rdata++;
	}

	return *rdata;
}
#endif

#ifndef ASM_MPNMOD
void mpnmod(mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* workspace)
{
	/* result size xsize, workspace size xsize+1 */
	mpw q;
	mpw msw = *ydata;
	mpw qsize = xsize-ysize;
	mpw* rdata = result;

	mpcopy(xsize, rdata, xdata);
	if (mpge(ysize, rdata, ydata))
		(void) mpsub(ysize, rdata, ydata);

	while (qsize--)
	{
		q = mppndiv(rdata[0], rdata[1], msw);

/*@-evalorder@*/
		*workspace = mpsetmul(ysize, workspace+1, ydata, q);
/*@=evalorder@*/

		while (mplt(ysize+1, rdata, workspace))
		{
			(void) mpsubx(ysize+1, workspace, ysize, ydata);
			q--;
		}
		(void) mpsub(ysize+1, rdata, workspace);
		rdata++;
	}
}
#endif

#ifndef ASM_MPNDIVMOD
void mpndivmod(mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* workspace)
{
	/* result must be xsize+1 in length */
	/* workspace must be ysize+1 in length */
	/* expect ydata to be normalized */
	mpw q;
	mpw msw = *ydata;
	size_t qsize = xsize-ysize;

	*result = (mpge(ysize, xdata, ydata) ? 1 : 0);
	mpcopy(xsize, result + 1, xdata);
	if (*result)
		(void) mpsub(ysize, result, ydata);
	result++;

	while (qsize--)
	{
		q = mppndiv(result[0], result[1], msw);

/*@-evalorder@*/
		*workspace = mpsetmul(ysize, workspace+1, ydata, q);
/*@=evalorder@*/

		while (mplt(ysize+1, result, workspace))
		{
			(void) mpsubx(ysize+1, workspace, ysize, ydata);
			q--;
		}
		(void) mpsub(ysize+1, result, workspace);
		*(result++) = q;
	}
}
#endif

void mpprint(size_t size, const mpw* data)
{
	mpfprint(stdout, size, data);
}

void mpprintln(size_t size, const mpw* data)
{
	mpfprintln(stdout, size, data);
}

void mpfprint(FILE * f, size_t size, const mpw* data)
{
	if (data == NULL)
	 	return;
	if (f == NULL)
		f = stderr;
	while (size--)
	{
		#if (MP_WBITS == 32)
		fprintf(f, "%08x", (unsigned) *(data++));
		#elif (MP_WBITS == 64)
		# if WIN32
		fprintf(f, "%016I64x", *(data++));
		# elif SIZEOF_UNSIGNED_LONG == 8
		fprintf(f, "%016lx", *(data++));
		# else
		fprintf(f, "%016llx", *(data++));
		# endif
		#else
		# error
		#endif
	}
	fprintf(f, "\n");
	(void) fflush(f);
}

void mpfprintln(FILE * f, size_t size, const mpw* data)
{
	if (data == NULL)
	 	return;
	if (f == NULL)
		f = stderr;
	while (size--)
	{
		#if (MP_WBITS == 32)
		fprintf(f, "%08x", (unsigned) *(data++));
		#elif (MP_WBITS == 64)
		# if WIN32
		fprintf(f, "%016I64x", *(data++));
		# elif SIZEOF_UNSIGNED_LONG == 8
		fprintf(f, "%016lx", *(data++));
		# else
		fprintf(f, "%016llx", *(data++));
		# endif
		#else
		# error
		#endif
	}
	fprintf(f, "\n");
	(void) fflush(f);
}

int i2osp(byte *osdata, size_t ossize, const mpw* idata, size_t isize)
{
	size_t required = MP_WORDS_TO_BYTES(isize);

	/* check if size is large enough */
	if (ossize >= required)
	{
		/* yes, we can proceed */
		if (ossize > required)
		{	/* fill initial bytes with zero */
			memset(osdata, 0, ossize-required);
			osdata += ossize-required;
		}
		if (required)
		{	/* fill remaining bytes with endian-adjusted data */
			#if !WORDS_BIGENDIAN
			while (required)
			{
				mpw w = *(idata++);
				byte shift = MP_WBITS;

				while (shift != 0)
				{
					shift -= 8;
					*(osdata++) = (byte)(w >> shift);
				}
				required -= MP_WBYTES;
			}
			#else
			memcpy(osdata, idata, required);
			#endif
		}
		return 0;
	}
	return -1;
}

int os2ip(mpw* idata, size_t isize, const byte* osdata, /*@unused@*/ size_t ossize)
{
	size_t required = MP_BYTES_TO_WORDS(isize + MP_WBYTES - 1);

	if (isize >= required)
	{
		/* yes, we can proceed */
		if (isize > required)
		{	/* fill initials words with zero */
			mpzero(isize-required, idata);
			idata += isize-required;
		}
		if (required)
		{	/* fill remaining words with endian-adjusted data */
			#if !WORDS_BIGENDIAN
			while (required)
			{
				mpw w = 0;
				byte b = MP_WBYTES;

				while (b--)
				{
					w <<= 8;
					w |= *(osdata++);
				}
				*(idata++) = w;
				required--;
			}
			#else
			memcpy(idata, osdata, MP_WORDS_TO_BYTES(required));
			#endif
		}
		return 0;
	}
	return -1;
}

int hs2ip(mpw* idata, size_t isize, const char* hsdata, size_t hssize)
{
	size_t required = MP_NIBBLES_TO_WORDS(hssize + MP_WNIBBLES - 1);

	if (isize >= required)
	{
		register size_t i;


		if (isize > required)
		{	/* fill initial words with zero */
			for (i = required; i < isize; i++)
				*(idata++) = 0;
		}
		while (hssize)
		{
			register mpw w = 0;
			register size_t chunk = hssize & (MP_WNIBBLES - 1);
			register char ch;

			if (chunk == 0) chunk = MP_WNIBBLES;

			for (i = 0; i < chunk; i++)
			{
				ch = *(hsdata++);
				w <<= 4;
				if (ch >= '0' && ch <= '9')
					w += (ch - '0');
				else if (ch >= 'A' && ch <= 'F')
					w += (ch - 'A') + 10;
				else if (ch >= 'a' && ch <= 'f')
					w += (ch - 'a') + 10;
			}
			*(idata++) = w;
			hssize -= chunk;
		}
		return 0;
	}
	return -1;
}
