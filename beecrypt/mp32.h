/** \ingroup MP_m
 * \file mp32.h
 *
 * Multiprecision 2's complement integer routines for 32 bit cpu, header/
 */

/*
 * Copyright (c) 1997, 1998, 1999, 2000  Virtual Unlimited B.V.
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

#ifndef _MP32_H
#define _MP32_H

#include "beecrypt.h"

#if HAVE_STRING_H
# include <string.h>
#endif

#include "mp32opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ASM_MP32COPY
#ifdef	__LCLINT__
#define mp32copy(size, dst, src) memmove(dst, src, (size) << 2)
#else
#define mp32copy(size, dst, src) memcpy(dst, src, (size) << 2)
#endif
#else
BEEDLLAPI
void mp32copy(uint32 size, uint32* dst, const uint32* src)
	/*@modifies dst @*/;
#endif

#ifndef ASM_MP32MOVE
#define mp32move(size, dst, src) memmove(dst, src, (size) << 2)
#else
BEEDLLAPI
void mp32move(uint32 size, uint32* dst, const uint32* src)
	/*@modifies dst @*/;
#endif

BEEDLLAPI
void mp32zero(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;
BEEDLLAPI
void mp32fill(uint32 xsize, uint32* xdata, uint32 val)
	/*@modifies xdata @*/;

BEEDLLAPI
int mp32odd (uint32 xsize, const uint32* xdata)
	/*@*/;
BEEDLLAPI
int mp32even(uint32 xsize, const uint32* xdata)
	/*@*/;

BEEDLLAPI
int mp32z  (uint32 xsize, const uint32* xdata)
	/*@*/;
BEEDLLAPI
int mp32nz (uint32 xsize, const uint32* xdata)
	/*@*/;

BEEDLLAPI
int mp32eq (uint32 size, const uint32* xdata, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32ne (uint32 size, const uint32* xdata, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32gt (uint32 size, const uint32* xdata, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32lt (uint32 size, const uint32* xdata, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32ge (uint32 size, const uint32* xdata, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32le (uint32 size, const uint32* xdata, const uint32* ydata)
	/*@*/;

BEEDLLAPI
int mp32eqx(uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32nex(uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32gtx(uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32ltx(uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32gex(uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@*/;
BEEDLLAPI
int mp32lex(uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@*/;

BEEDLLAPI
int mp32isone(uint32 xsize, const uint32* xdata)
	/*@*/;
BEEDLLAPI
int mp32istwo(uint32 xsize, const uint32* xdata)
	/*@*/;
BEEDLLAPI
int mp32leone(uint32 xsize, const uint32* xdata)
	/*@*/;
BEEDLLAPI
int mp32eqmone(uint32 size, const uint32* xdata, const uint32* ydata)
	/*@*/;

BEEDLLAPI
int mp32msbset(uint32 xsize, const uint32* xdata)
	/*@*/;
BEEDLLAPI
int mp32lsbset(uint32 xsize, const uint32* xdata)
	/*@*/;

BEEDLLAPI
void mp32setmsb(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;
BEEDLLAPI
void mp32setlsb(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;
BEEDLLAPI
void mp32clrmsb(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;
BEEDLLAPI
void mp32clrlsb(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;

BEEDLLAPI
void mp32xor(uint32 size, uint32* xdata, const uint32* ydata)
	/*@modifies xdata @*/;
BEEDLLAPI
void mp32not(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;

BEEDLLAPI
void mp32setw(uint32 xsize, uint32* xdata, uint32 y)
	/*@modifies xdata @*/;
BEEDLLAPI
void mp32setx(uint32 xsize, uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@modifies xdata @*/;

BEEDLLAPI
uint32 mp32addw(uint32 xsize, uint32* xdata, uint32 y)
	/*@modifies xdata @*/;
BEEDLLAPI
uint32 mp32add (uint32 size, uint32* xdata, const uint32* ydata)
	/*@modifies xdata @*/;
BEEDLLAPI
uint32 mp32addx(uint32 xsize, uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@modifies xdata @*/;

BEEDLLAPI
uint32 mp32subw(uint32 xsize, uint32* xdata, uint32 y)
	/*@modifies xdata @*/;
BEEDLLAPI
uint32 mp32sub (uint32 size, uint32* xdata, const uint32* ydata)
	/*@modifies xdata @*/;
BEEDLLAPI
uint32 mp32subx(uint32 xsize, uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@modifies xdata @*/;

BEEDLLAPI
uint32 mp32multwo(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;

BEEDLLAPI
void mp32neg(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;

BEEDLLAPI
uint32 mp32size(uint32 xsize, const uint32* xdata)
	/*@*/;

BEEDLLAPI
uint32 mp32mszcnt(uint32 xsize, const uint32* xdata)
	/*@*/;
BEEDLLAPI
uint32 mp32lszcnt(uint32 xsize, const uint32* xdata)
	/*@*/;

BEEDLLAPI
void mp32lshift(uint32 xsize, uint32* xdata, uint32 count)
	/*@modifies xdata @*/;
BEEDLLAPI
void mp32rshift(uint32 xsize, uint32* xdata, uint32 count)
	/*@modifies xdata @*/;

BEEDLLAPI
uint32 mp32norm(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;
BEEDLLAPI
uint32 mp32divpowtwo(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;

BEEDLLAPI
void mp32divtwo (uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;
BEEDLLAPI
void mp32sdivtwo(uint32 xsize, uint32* xdata)
	/*@modifies xdata @*/;

BEEDLLAPI
uint32 mp32setmul   (uint32 size, /*@out@*/ uint32* result, const uint32* xdata, uint32 y)
	/*@modifies result @*/;
BEEDLLAPI
uint32 mp32addmul   (uint32 size, /*@out@*/ uint32* result, const uint32* xdata, uint32 y)
	/*@modifies result @*/;
BEEDLLAPI
uint32 mp32addsqrtrc(uint32 size, /*@out@*/ uint32* result, const uint32* xdata)
	/*@modifies result @*/;

BEEDLLAPI
void mp32mul(/*@out@*/ uint32* result, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata)
	/*@modifies result @*/;
BEEDLLAPI
void mp32sqr(/*@out@*/ uint32* result, uint32 xsize, const uint32* xdata)
	/*@modifies result @*/;

BEEDLLAPI
void mp32gcd_w(uint32 size, const uint32* xdata, const uint32* ydata, /*@out@*/ uint32* result, uint32* wksp)
	/*@modifies result, wksp @*/;

BEEDLLAPI
uint32 mp32nmodw(/*@out@*/ uint32* result, uint32 xsize, const uint32* xdata, uint32 y, uint32* wksp)
	/*@modifies result, wksp @*/;

BEEDLLAPI
void mp32nmod(/*@out@*/ uint32* result, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32ndivmod(/*@out@*/ uint32* result, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, uint32* wksp)
	/*@modifies result, wksp @*/;

BEEDLLAPI
void mp32print(uint32 xsize, const uint32* xdata)
	/*@modifies fileSystem @*/;
BEEDLLAPI
void mp32println(uint32 xsize, const uint32* xdata)
	/*@modifies fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif
