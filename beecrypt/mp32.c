/*
 * mp32.c
 *
 * Multiprecision 2's complement integer routines for 32 bit cpu, code
 *
 * Copyright (c) 1997, 1998, 1999, 2000 Virtual Unlimited B.V.
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

#include <stdio.h>

#ifndef ASM_MP32ZERO
void mp32zero(register uint32 xsize, register uint32* xdata)
{
	while (xsize--)
		*(xdata++) = 0;
}
#endif

#ifndef ASM_MP32FILL
void mp32fill(register uint32 xsize, register uint32* xdata, register uint32 val)
{
	while (xsize--)
		*(xdata++) = val;
}
#endif

#ifndef ASM_MP32ODD
int mp32odd(register uint32 xsize, register const uint32* xdata)
{
	return (xdata[xsize-1] & 0x1);
}
#endif

#ifndef ASM_MP32EVEN
int mp32even(register uint32 xsize, register const uint32* xdata)
{
	return !(xdata[xsize-1] & 0x1);
}
#endif

#ifndef ASM_MP32Z
int mp32z(register uint32 xsize, register const uint32* xdata)
{
	while (xsize--)
		if (*(xdata++))
			return 0;
	return 1;
}
#endif

#ifndef ASM_MP32NZ
int mp32nz(register uint32 xsize, register const uint32* xdata)
{
	while (xsize--)
		if (*(xdata++))
			return 1;
	return 0;
}
#endif

#ifndef ASM_MP32EQ
int mp32eq(register uint32 size, register const uint32* xdata, register const uint32* ydata)
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

#ifndef ASM_MP32EQX
int mp32eqx(register uint32 xsize, register const uint32* xdata, register uint32 ysize, register const uint32* ydata)
{
	if (xsize > ysize)
	{
		register int diff = xsize - ysize;
		return mp32eq(ysize, xdata + diff, ydata) && mp32z(diff, xdata);
	}
	else if (xsize < ysize)
	{
		register int diff = ysize - xsize;
		return mp32eq(xsize, ydata + diff, xdata) && mp32z(diff, ydata);
	}
	else
		return mp32eq(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MP32NE
int mp32ne(register uint32 size, register const uint32* xdata, register const uint32* ydata)
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

#ifndef ASM_MP32NEX
int mp32nex(register uint32 xsize, register const uint32* xdata, register uint32 ysize, register const uint32*ydata)
{
	if (xsize > ysize)
	{
		register int diff = xsize - ysize;
		return mp32nz(diff, xdata) || mp32ne(ysize, xdata + diff, ydata);
	}
	else if (xsize < ysize)
	{
		register int diff = ysize - xsize;
		return mp32nz(diff, ydata) || mp32ne(xsize, ydata + diff, xdata);
	}
	else
		return mp32ne(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MP32GT
int mp32gt(register uint32 size, register const uint32* xdata, register const uint32* ydata)
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

#ifndef ASM_MP32GTX
int mp32gtx(register uint32 xsize, register const uint32* xdata, register uint32 ysize, register const uint32* ydata)
{
	if (xsize > ysize)
	{
		register uint32 diff = xsize - ysize;
		return mp32nz(diff, xdata) || mp32gt(ysize, xdata + diff, ydata);
	}
	else if (xsize < ysize)
	{
		register uint32 diff = ysize - xsize;
		return mp32z(diff, ydata) && mp32gt(xsize, xdata, ydata + diff);
	}
	else
		return mp32gt(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MP32LT
int mp32lt(register uint32 size, register const uint32* xdata, register const uint32* ydata)
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

#ifndef ASM_MP32LTX
int mp32ltx(register uint32 xsize, register const uint32* xdata, register uint32 ysize, register const uint32* ydata)
{
	if (xsize > ysize)
	{
		register uint32 diff = xsize - ysize;
		return mp32z(diff, xdata) && mp32lt(ysize, xdata + diff, ydata);
	}
	else if (xsize < ysize)
	{
		register uint32 diff = ysize - xsize;
		return mp32nz(diff, ydata) || mp32lt(xsize, xdata, ydata + diff);
	}
	else
		return mp32lt(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MP32GE
int mp32ge(register uint32 size, register const uint32* xdata, register const uint32* ydata)
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

#ifndef ASM_MP32GEX
int mp32gex(register uint32 xsize, register const uint32* xdata, register uint32 ysize, register const uint32* ydata)
{
	if (xsize > ysize)
	{
		register uint32 diff = xsize - ysize;
		return mp32nz(diff, xdata) || mp32ge(ysize, xdata + diff, ydata);
	}
	else if (xsize < ysize)
	{
		register uint32 diff = ysize - xsize;
		return mp32z(diff, ydata) && mp32ge(xsize, xdata, ydata + diff);
	}
	else
		return mp32ge(xsize, xdata, ydata);
}
#endif

#ifndef ASM_MP32LE
int mp32le(register uint32 size, register const uint32* xdata, register const uint32* ydata)
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

#ifndef ASM_MP32LEX
int mp32lex(register uint32 xsize, register const uint32* xdata, register uint32 ysize, register const uint32* ydata)
{
	if (xsize > ysize)
	{
		register uint32 diff = xsize - ysize;
		return mp32z(diff, xdata) && mp32le(ysize, xdata + diff, ydata);
	}
	else if (xsize < ysize)
	{
		register uint32 diff = ysize - xsize;
		return mp32nz(diff, ydata) || mp32le(xsize, xdata, ydata + diff);
	}
	else
		return mp32le(xsize, xdata, ydata);
}
#endif


#ifndef ASM_MP32ISONE
int mp32isone(register uint32 xsize, register const uint32* xdata)
{
	xdata += xsize;
	if (*(--xdata) == 1)
	{
		while (--xsize)
			if (*(--xdata))
				return 0;
		return 1;
	}
	return 0;
}
#endif

#ifndef ASM_MP32ISTWO
int mp32istwo(register uint32 xsize, register const uint32* xdata)
{
	xdata += xsize;
	if (*(--xdata) == 2)
	{
		while (--xsize)
			if (*(--xdata))
				return 0;
		return 1;
	}
	return 0;
}
#endif

#ifndef ASM_MP32EQMONE
int mp32eqmone(register uint32 size, register const uint32* xdata, register const uint32* ydata)
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

#ifndef ASM_MP32LEONE
int mp32leone(register uint32 xsize, register const uint32* xdata)
{
	xdata += xsize;
	if (*(--xdata) > 1)
		return 0;
	else
	{
		while (--xsize)
			if (*(--xdata))
				return 0;
		return 1;
	}
}
#endif

#ifndef ASM_MP32MSBSET
int mp32msbset(/*@unused@*/ register uint32 xsize, register const uint32* xdata)
{
	return ((*xdata) & 0x80000000);
}
#endif

#ifndef ASM_MP32LSBSET
int mp32lsbset(register uint32 xsize, register const uint32* xdata)
{
    return xdata[xsize-1] & 0x1;
}
#endif

#ifndef ASM_MP32SETMSB
void mp32setmsb(/*@unused@*/ register uint32 xsize, register uint32* xdata)
{
	*xdata |= 0x80000000;
}
#endif

#ifndef ASM_MP32SETLSB
void mp32setlsb(register uint32 xsize, register uint32* xdata)
{
	xdata[xsize-1] |= 0x00000001;
}
#endif

#ifndef ASM_MP32CLRMSB
void mp32clrmsb(/*@unused@*/ register uint32 xsize, register uint32* xdata)
{
	*xdata &= 0x7fffffff;
}
#endif

#ifndef ASM_MP32CLRLSB
void mp32clrlsb(register uint32 xsize, register uint32* xdata)
{
    xdata[xsize-1] &= 0xfffffffe;
}
#endif

#ifndef ASM_MP32XOR
void mp32xor(register uint32 size, register uint32* xdata, register const uint32* ydata)
{
	do
	{
		--size;
		xdata[size] ^= ydata[size];
	} while (size);
}
#endif

#ifndef ASM_MP32NOT
void mp32not(register uint32 xsize, register uint32* xdata)
{
	do
	{
		--xsize;
		xdata[xsize] = ~xdata[xsize];
	} while (xsize);
}
#endif

#ifndef ASM_MP32SETW
void mp32setw(register uint32 xsize, register uint32* xdata, register uint32 y)
{
	while (--xsize)
		*(xdata++) = 0;
	*(xdata++) = y;
}
#endif

#ifndef ASM_MP32SETX
void mp32setx(register uint32 xsize, register uint32* xdata, register uint32 ysize, register const uint32* ydata)
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

#ifndef ASM_MP32ADDW
uint32 mp32addw(register uint32 xsize, register uint32* xdata, register uint32 y)
{
	register uint64 temp;
	register uint32 carry = 0;

	xdata += xsize;
	temp = *(--xdata);
	temp += y;
	*xdata = (uint32) temp;
	while (--xsize && (carry = (uint32) (temp >> 32)))
	{
		temp = *(--xdata);
		temp += carry;
		*xdata = (uint32) temp;
	}
	return (uint32)(temp >> 32);
}
#endif

#ifndef ASM_MP32ADD
uint32 mp32add(register uint32 size, register uint32* xdata, register const uint32* ydata)
{
	register uint64 temp;
	register uint32 carry = 0;

	xdata += size;
	ydata += size;

	while (size--)
	{
		temp = *(--xdata);
		temp += *(--ydata);
		temp += carry;
		*xdata = (uint32) temp;
		carry = (uint32) (temp >> 32);
	}
	return carry;
}
#endif

#ifndef ASM_MP32ADDX
uint32 mp32addx(register uint32 xsize, register uint32* xdata, register uint32 ysize, register const uint32* ydata)
{
	if (xsize > ysize)
	{
		register uint32 diff = xsize - ysize;
		register uint32 carry = mp32add(ysize, xdata + diff, ydata);
		return mp32addw(diff, xdata, carry);
	}
	else
	{
		register int diff = ysize - xsize;
		return mp32add(xsize, xdata, ydata + diff);
	}
}
#endif

#ifndef ASM_MP32SUBW
uint32 mp32subw(register uint32 xsize, register uint32* xdata, register uint32 y)
{
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
#endif

#ifndef ASM_MP32SUB
uint32 mp32sub(register uint32 size, register uint32* xdata, register const uint32* ydata)
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
#endif

#ifndef ASM_MP32SUBX
uint32 mp32subx(register uint32 xsize, register uint32* xdata, register uint32 ysize, register const uint32* ydata)
{
	if (xsize > ysize)
	{
		register uint32 diff = xsize - ysize;
		register uint32 carry = mp32sub(ysize, xdata + diff, ydata);
		return mp32subw(diff, xdata, carry);
	}
	else
	{
		register uint32 diff = ysize - xsize;
		return mp32sub(xsize, xdata, ydata + diff);
	}
}
#endif

#ifndef ASM_MP32NEG
void mp32neg(register uint32 xsize, register uint32* xdata)
{
	mp32not(xsize, xdata);
	(void) mp32addw(xsize, xdata, 1);
}
#endif

#ifndef ASM_MP32SETMUL
uint32 mp32setmul(register uint32 size, register uint32* result, register const uint32* xdata, register uint32 y)
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
#endif

#ifndef ASM_MP32ADDMUL
uint32 mp32addmul(register uint32 size, register uint32* result, register const uint32* xdata, register uint32 y)
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
#endif

#ifndef ASM_MP32MUL
void mp32mul(uint32* result, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
{
	/*@-mods@*/
	/* preferred passing of parameters is x the larger of the two numbers */
	if (xsize >= ysize)
	{
		register uint32 rc;

		result += ysize;
		ydata += ysize;

		rc = mp32setmul(xsize, result, xdata, *(--ydata));
		*(--result) = rc;

		while (--ysize)
		{
			rc = mp32addmul(xsize, result, xdata, *(--ydata));
			*(--result) = rc;
		}
	}
	else
	{
		register uint32 rc;

		result += xsize;
		xdata += xsize;

		rc = mp32setmul(ysize, result, ydata, *(--xdata));
		*(--result) = rc;

		while (--xsize)
		{
			rc = mp32addmul(ysize, result, ydata, *(--xdata));
			*(--result) = rc;
		}
	}
	/*@=mods@*/
}
#endif

#ifndef ASM_MP32ADDSQRTRC
uint32 mp32addsqrtrc(register uint32 size, register uint32* result, register const uint32* xdata)
{
	register uint64 temp;
	register uint32 n, carry = 0;

	result += size*2;

	while (size--)
	{
		temp = n = xdata[size];
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
#endif

#ifndef ASM_MP32SQR
void mp32sqr(register uint32* result, register uint32 xsize, register const uint32* xdata)
{
	register uint32 carry;
	register uint32 n = xsize-1;

	/*@-mods@*/
	result += xsize;
	result[n] = 0;

	if (n)
	{
		carry = mp32setmul(n, result, xdata, xdata[n]);
		*(--result) = carry;
		while (--n)
		{
			carry = mp32addmul(n, result, xdata, xdata[n]);
			*(--result) = carry;
		}
	}

	*(--result) = 0;

	(void) mp32multwo(xsize*2, result);

	(void) mp32addsqrtrc(xsize, result, xdata);
	/*@=mods@*/
}
#endif

#ifndef ASM_MP32SIZE
uint32 mp32size(register uint32 xsize, register const uint32* xdata)
{
	while (xsize)
	{
		if (*xdata)
			return xsize;
		xdata++;
		xsize--;
	}
	return 0;
}
#endif

#ifndef ASM_MP32NORM
uint32 mp32norm(register uint32 xsize, register uint32* xdata)
{
	register uint32 shift = mp32mszcnt(xsize, xdata);
	mp32lshift(xsize, xdata, shift);
	return shift;
}
#endif

#ifndef ASM_MP32DIVPOWTWO
uint32 mp32divpowtwo(register uint32 xsize, register uint32* xdata)
{
	register uint32 shift = mp32lszcnt(xsize, xdata);
	mp32rshift(xsize, xdata, shift);
	return shift;
}
#endif

#ifndef ASM_MP32DIVTWO
void mp32divtwo(register uint32 xsize, register uint32* xdata)
{
	register uint32 temp;
	register uint32 carry = 0;

	while (xsize--)
	{
		temp = *xdata;
		*(xdata++) = (temp >> 1) | carry;
		carry = (temp << 31);
	}
}
#endif

#ifndef ASM_MP32SDIVTWO
void mp32sdivtwo(register uint32 xsize, register uint32* xdata)
{
	mp32divtwo(xsize, xdata);
	if (*xdata & 0x40000000)
		*xdata |= 0x80000000;
}
#endif

#ifndef ASM_MP32MULTWO
uint32 mp32multwo(register uint32 xsize, register uint32* xdata)
{
	register uint32 temp;
	register uint32 carry = 0;

	xdata += xsize;
	while (xsize--)
	{
		temp = *(--xdata);
		*xdata = (temp << 1) | carry;
		carry = (temp >> 31);
	}
	return carry;
}
#endif

#ifndef ASM_MP32MSZCNT
uint32 mp32mszcnt(register uint32 xsize, register const uint32* xdata)
{
	register uint32 zbits = 0;
	register uint32 i = 0;

	while (i < xsize)
	{
		register uint32 temp = xdata[i++];
		if (temp)
		{
			while (!(temp & 0x80000000))
			{
				zbits++;
				temp <<= 1;
			}
			break;
		}
		else
			zbits += 32;
	}
	return zbits;
}
#endif

#ifndef ASM_MP32LSZCNT
uint32 mp32lszcnt(register uint32 xsize, register const uint32* xdata)
{
	register uint32 zbits = 0;

	while (xsize--)
	{
		register uint32 temp = xdata[xsize];
		if (temp)
		{
			while (!(temp & 0x1))
			{
				zbits++;
				temp >>= 1;
			}
			break;
		}
		else
			zbits += 32;
	}
	return zbits;
}
#endif

#ifndef ASM_MP32LSHIFT
void mp32lshift(register uint32 xsize, register uint32* xdata, uint32 count)
{
	register uint32 words = count >> 5;

	if (words < xsize)
	{
		register uint8  lbits = (uint8) (count & 0x1f);

		/* first do the shifting, then do the moving */
		if (lbits != 0)
		{
			register uint32 temp;
			register uint32 carry = 0;
			register uint8  rbits = 32-lbits;
			register uint32 i = xsize;

			while (i > words)
			{
				temp = xdata[--i];
				xdata[i] = (temp << lbits) | carry;
				carry = (temp >> rbits);
			}
		}
		if (words)
		{
			mp32move(xsize-words, xdata, xdata+words);
			mp32zero(words, xdata+xsize-words);
		}
	}
	else
		mp32zero(xsize, xdata);
}
#endif

#ifndef ASM_MP32RSHIFT
void mp32rshift(register uint32 xsize, register uint32* xdata, uint32 count)
{
	register uint32 words = count >> 5;

	if (words < xsize)
	{
		register uint8 rbits = (uint8) (count & 0x1f);

		/* first do the shifting, then do the moving */
		if (rbits != 0)
		{
			register uint32 temp;
			register uint32 carry = 0;
			register uint8  lbits = 32-rbits;
			register uint32 i = 0;

			while (i < xsize-words)
			{
				temp = xdata[i];
				xdata[i++] = (temp >> rbits) | carry;
				carry = (temp << lbits);
			}
		}
		if (words)
		{
			mp32move(xsize-words, xdata+words, xdata);
			mp32zero(words, xdata);
		}
	}
	else
		mp32zero(xsize, xdata);
}
#endif

#ifndef ASM_MP32GCD_W
/**
 * mp32gcd_w
 *  need workspace of (size) words
 */
void mp32gcd_w(uint32 size, const uint32* xdata, const uint32* ydata, uint32* result, uint32* wksp)
{
	register uint32 shift = 0;
	register uint32 temp;

	if (mp32ge(size, xdata, ydata))
	{
		mp32copy(size, wksp, xdata);
		mp32copy(size, result, ydata);
	}
	else
	{
		mp32copy(size, wksp, ydata);
		mp32copy(size, result, xdata);
	}
		
	/* start with doing mp32divpowtwo on both workspace and result, and store the returned values */
	/* get the smallest returned values, and set shift to that */

	if ((temp = mp32lszcnt(size, wksp)))
		mp32rshift(size, wksp, temp);

	shift = temp;

	if ((temp = mp32lszcnt(size, result)))
		mp32rshift(size, result, temp);

	if (shift > temp)
		shift = temp;

	while (mp32nz(size, wksp))
	{
		if ((temp = mp32lszcnt(size, wksp)))
			mp32rshift(size, wksp, temp);

		if ((temp = mp32lszcnt(size, result)))
			mp32rshift(size, result, temp);

		if (mp32ge(size, wksp, result))
			(void) mp32sub(size, wksp, result);
		else
			(void) mp32sub(size, result, wksp);
	}
	mp32lshift(size, result, shift);
}
#endif

#ifndef ASM_MP32NMODW
uint32 mp32nmodw(uint32* result, uint32 xsize, const uint32* xdata, uint32 y, uint32* workspace)
{
	/* result size xsize, workspace size xsize+1 */
	register uint64 temp;
	register uint32 q;
	uint32 qsize = xsize-1;
	uint32* rdata = result;

	mp32copy(xsize, rdata, xdata);
	/*
		if (*rdata >= y)
			*rdata -= y;
	*/
	if (mp32ge(1, rdata, &y))
		(void) mp32sub(1, rdata, &y);

	while (qsize--)
	{
		/* printf("result = "); MP32println(xsize+1, result); */
		/* get the two high words of r into temp */
		temp = rdata[0];
		temp <<= 32;
		temp += rdata[1];
		/* printf("q = %016llx / %08lx\n", temp, msw); */
		temp /= y;
		/*
			temp *= y;
			workspace[0] = (uint32) (temp >> 32);
			workspace[1] = (uint32) (temp);
		*/
		q = (uint32) temp;

		/* printf("q = %08x\n", q); */
		/*@-evalorder@*/
		*workspace = mp32setmul(1, workspace+1, &y, q);
		/*@=evalorder@*/

		/* printf("mplt "); mp32print(2, rdata); printf(" < "); mp32println(2, workspace); */
		while (mp32lt(2, rdata, workspace))
		{
			/* printf("mp32lt! "); mp32print(2, rdata); printf(" < "); mp32println(2, workspace); */
			/* printf("decreasing q\n"); */
			(void) mp32subx(2, workspace, 1, &y);
			/* q--; */
		}
		/* printf("subtracting\n"); */
		(void) mp32sub(2, rdata, workspace);
		rdata++;
	}

	return *rdata;
}
#endif

#ifndef ASM_MP32NMOD
void mp32nmod(uint32* result, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, uint32* workspace)
{
	/* result size xsize, workspace size xsize+1 */
	register uint64 temp;
	register uint32 q;
	uint32 msw = *ydata;
	uint32 qsize = xsize-ysize;
	uint32* rdata = result;

	mp32copy(xsize, rdata, xdata);
	if (mp32ge(ysize, rdata, ydata))
		(void) mp32sub(ysize, rdata, ydata);

	while (qsize--)
	{
		/* printf("result = "); mp32println(xsize+1, result); */
		/* get the two high words of r into temp */
		temp = rdata[0];
		temp <<= 32;
		temp += rdata[1];
		/* printf("q = %016llx / %08lx\n", temp, msw); */
		temp /= msw;
		q = (uint32) temp;

		/* printf("q = %08x\n", q); */
		/*@-evalorder@*/
		*workspace = mp32setmul(ysize, workspace+1, ydata, q);
		/*@=evalorder@*/

		/* printf("mp32lt "); mp32print(ysize+1, rdata); printf(" < "); mp32println(ysize+1, workspace); */
		while (mp32lt(ysize+1, rdata, workspace))
		{
			/* printf("mp32lt! "); mp32print(ysize+1, rdata); printf(" < "); mp32println(ysize+1, workspace); */
			/* printf("decreasing q\n"); */
			(void) mp32subx(ysize+1, workspace, ysize, ydata);
			q--;
		}
		/* printf("subtracting\n"); */
		(void) mp32sub(ysize+1, rdata, workspace);
		rdata++;
	}
}
#endif

#ifndef ASM_MP32NDIVMOD
void mp32ndivmod(uint32* result, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, register uint32* workspace)
{
	/* result must be xsize+1 in length */
	/* workspace must be ysize+1 in length */
	/* expect ydata to be normalized */
	register uint64 temp;
	register uint32 q;
	uint32 msw = *ydata;
	uint32 qsize = xsize-ysize;

	mp32copy(xsize, result+1, xdata);
	if (mp32ge(ysize, result+1, ydata))
	{
		/* printf("subtracting\n"); */
		(void) mp32sub(ysize, result+1, ydata);
		*(result++) = 1;
	}
	else
		*(result++) = 0;

	while (qsize--)
	{
		/* printf("result = "); mp32println(xsize+1, result); */
		/* get the two high words of r into temp */
		temp = result[0];
		temp <<= 32;
		temp += result[1];
		/* printf("q = %016llx / %08lx\n", temp, msw); */
		temp /= msw;
		q = (uint32) temp;

		/* printf("q = %08x\n", q); */

		/*@-evalorder@*/
		*workspace = mp32setmul(ysize, workspace+1, ydata, q);
		/*@=evalorder@*/

		/* printf("mp32lt "); mp32print(ysize+1, result); printf(" < "); mp32println(ysize+1, workspace); */
		while (mp32lt(ysize+1, result, workspace))
		{
			/* printf("mp32lt! "); mp32print(ysize+1, result); printf(" < "); mp32println(ysize+1, workspace); */
			/* printf("decreasing q\n"); */
			(void) mp32subx(ysize+1, workspace, ysize, ydata);
			q--;
		}
		/* printf("subtracting\n"); */
		(void) mp32sub(ysize+1, result, workspace);
		*(result++) = q;
	}
}
#endif

/*
#ifndef ASM_MP32UNPACK
void mp32unpack(uint32 size, uint8* bytes, const uint32* bits)
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

#ifndef ASM_MP32PRINT
void mp32print(register uint32 xsize, register const uint32* xdata)
{
	while (xsize--)
		printf("%08x", *(xdata++));
	(void) fflush(stdout);
}
#endif

#ifndef ASM_MP32PRINTLN
void mp32println(register uint32 xsize, register const uint32* xdata)
{
	while (xsize--)
		printf("%08x", *(xdata++));
	printf("\n");
	(void) fflush(stdout);
}
#endif
