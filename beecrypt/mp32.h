/*
 * mp32.h
 *
 * Multiprecision 2's complement integer routines for 32 bit cpu, header
 *
 * Copyright (c) 1997-2000  Virtual Unlimited B.V.
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
#include <string.h>
#endif

#include "mp32opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ASM_MP32COPY
#define mp32copy(size, dst, src) memcpy(dst, src, (size) << 2)
#else
BEEDLLAPI
void mp32copy(uint32, uint32*, const uint32*);
#endif

#ifndef ASM_MP32MOVE
#define mp32move(size, dst, src) memmove(dst, src, (size) << 2)
#else
BEEDLLAPI
void mp32move(uint32, uint32*, const uint32*);
#endif

BEEDLLAPI
void mp32zero(uint32, uint32*);
BEEDLLAPI
void mp32fill(uint32, uint32*, uint32);

BEEDLLAPI
int mp32odd (uint32, const uint32*);
BEEDLLAPI
int mp32even(uint32, const uint32*);

BEEDLLAPI
int mp32z  (uint32, const uint32*);
BEEDLLAPI
int mp32nz (uint32, const uint32*);
BEEDLLAPI
int mp32eq (uint32, const uint32*, const uint32*);
BEEDLLAPI
int mp32ne (uint32, const uint32*, const uint32*);
BEEDLLAPI
int mp32gt (uint32, const uint32*, const uint32*);
BEEDLLAPI
int mp32lt (uint32, const uint32*, const uint32*);
BEEDLLAPI
int mp32ge (uint32, const uint32*, const uint32*);
BEEDLLAPI
int mp32le (uint32, const uint32*, const uint32*);
BEEDLLAPI
int mp32eqx(uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
int mp32nex(uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
int mp32gtx(uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
int mp32ltx(uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
int mp32gex(uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
int mp32lex(uint32, const uint32*, uint32, const uint32*);

BEEDLLAPI
int mp32isone(uint32, const uint32*);
BEEDLLAPI
int mp32leone(uint32, const uint32*);
BEEDLLAPI
int mp32eqmone(uint32, const uint32*, const uint32*);

BEEDLLAPI
int mp32msbset(uint32, const uint32*);
BEEDLLAPI
int mp32lsbset(uint32, const uint32*);

BEEDLLAPI
void mp32setmsb(uint32, uint32*);
BEEDLLAPI
void mp32setlsb(uint32, uint32*);
BEEDLLAPI
void mp32clrmsb(uint32, uint32*);
BEEDLLAPI
void mp32clrlsb(uint32, uint32*);

BEEDLLAPI
void mp32xor(uint32, uint32*, const uint32*);
BEEDLLAPI
void mp32not(uint32, uint32*);

BEEDLLAPI
void mp32setw(uint32, uint32*, uint32);
BEEDLLAPI
void mp32setx(uint32, uint32*, uint32, const uint32*);

BEEDLLAPI
uint32 mp32addw(uint32, uint32*, uint32);
BEEDLLAPI
uint32 mp32add (uint32, uint32*, const uint32*);
BEEDLLAPI
uint32 mp32addx(uint32, uint32*, uint32, const uint32*);

BEEDLLAPI
uint32 mp32subw(uint32, uint32*, uint32);
BEEDLLAPI
uint32 mp32sub (uint32, uint32*, const uint32*);
BEEDLLAPI
uint32 mp32subx(uint32, uint32*, uint32, const uint32*);

BEEDLLAPI
uint32 mp32multwo(uint32, uint32*);

BEEDLLAPI
void mp32neg(uint32, uint32*);

BEEDLLAPI
uint32 mp32size(uint32, const uint32*);

BEEDLLAPI
uint32 mp32mszcnt(uint32, const uint32*);
BEEDLLAPI
uint32 mp32lszcnt(uint32, const uint32*);

BEEDLLAPI
void mp32lshift(uint32, uint32*, uint32);
BEEDLLAPI
void mp32rshift(uint32, uint32*, uint32);

BEEDLLAPI
uint32 mp32norm(uint32, uint32*);
BEEDLLAPI
uint32 mp32divpowtwo(uint32, uint32*);

BEEDLLAPI
void mp32divtwo (uint32, uint32*);
BEEDLLAPI
void mp32sdivtwo(uint32, uint32*);

BEEDLLAPI
uint32 mp32setmul   (uint32, uint32*, const uint32*, uint32);
BEEDLLAPI
uint32 mp32addmul   (uint32, uint32*, const uint32*, uint32);
BEEDLLAPI
uint32 mp32addsqrtrc(uint32, uint32*, const uint32*);

BEEDLLAPI
void mp32mul(uint32*, uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
void mp32sqr(uint32*, uint32, const uint32*);

BEEDLLAPI
void mp32gcd(uint32*, uint32, const uint32*, const uint32*, uint32*);

BEEDLLAPI
uint32 mp32nmodw(uint32*, uint32, const uint32*, uint32, uint32*);

BEEDLLAPI
void mp32nmod(uint32*, uint32, const uint32*, uint32, const uint32*, uint32*);
BEEDLLAPI
void mp32ndivmod(uint32*, uint32, const uint32*, uint32, const uint32*, uint32*);

BEEDLLAPI
void mp32print(uint32, const uint32*);
BEEDLLAPI
void mp32println(uint32, const uint32*);

#ifdef __cplusplus
}
#endif

#endif
