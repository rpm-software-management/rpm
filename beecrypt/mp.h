/** \ingroup MP_m
 * \file mp.h
 *
 * Multiprecision 2's complement integer routines for 32 bit cpu, header/
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

#ifndef _MP_H
#define _MP_H

#include "beecrypt.h"

#if HAVE_STRING_H
# include <string.h>
#endif
#include <stdio.h>

#include "mpopt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI /*@unused@*/
void mpcopy(size_t size, /*@out@*/ mpw* dst, const mpw* src)
	/*@modifies dst @*/;
#ifndef ASM_MP32COPY
#ifdef	__LCLINT__
# define mpcopy(size, dst, src) memmove(dst, src, ((unsigned)(size)) << 2)
#else
# define mpcopy(size, dst, src) memcpy(dst, src, (size) << 2)
#endif
#endif

/**
 */
BEECRYPTAPI /*@unused@*/
void mpmove(size_t size, /*@out@*/ mpw* dst, const mpw* src)
	/*@modifies dst @*/;
#ifndef ASM_MP32MOVE
#ifdef	__LCLINT__
# define mpmove(size, dst, src) memmove(dst, src, ((unsigned)(size)) << 2)
#else
# define mpmove(size, dst, src) memmove(dst, src, (size) << 2)
#endif
#endif

/**
 */
BEECRYPTAPI
void mpzero(size_t xsize, /*@out@*/ mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpfill(size_t xsize, /*@out@*/ mpw* xdata, uint32 val)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpodd (size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpeven(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpz  (size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mpnz (size_t xsize, const mpw* xdata)
	/*@*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
int mpeq (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mpne (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mpgt (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mplt (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
int mpge (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mple (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpeqx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int mpnex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int mpgtx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int mpltx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpgex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mplex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpisone(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpistwo(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpleone(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int mpeqmone(size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpmsbset(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int mplsbset(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpsetmsb(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
void mpsetlsb(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpclrmsb(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpclrlsb(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpand(size_t size, mpw* xdata, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpxor(size_t size, mpw* xdata, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpor(size_t size, mpw* xdata, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
void mpnot(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mpsetw(size_t xsize, /*@out@*/ mpw* xdata, uint32 y)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
void mpsetx(size_t xsize, /*@out@*/ mpw* xdata, size_t ysize, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpaddw(size_t xsize, mpw* xdata, uint32 y)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
uint32 mpadd (size_t size, mpw* xdata, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpaddx(size_t xsize, mpw* xdata, size_t ysize, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpsubw(size_t xsize, mpw* xdata, uint32 y)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpsub (size_t size, mpw* xdata, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpsubx(size_t xsize, mpw* xdata, size_t ysize, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpmultwo(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
void mpneg(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
size_t mpsize(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI
size_t mpmszcnt(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
BEECRYPTAPI
size_t mpbitcnt(size_t xsize, const mpw* xdata)
	/*@*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI /*@unused@*/
size_t mplszcnt(size_t xsize, const mpw* xdata)
	/*@*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
void mplshift(size_t xsize, mpw* xdata, uint32 count)
	/*@modifies xdata @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mprshift(size_t xsize, mpw* xdata, uint32 count)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
size_t mprshiftlsz(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
size_t mpnorm(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
uint32 mpdivpowtwo(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
void mpdivtwo (size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
void mpsdivtwo(size_t xsize, mpw* xdata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
uint32 mpsetmul   (size_t size, /*@out@*/ mpw* result, const mpw* xdata, uint32 y)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI
uint32 mpaddmul   (size_t size, /*@out@*/ mpw* result, const mpw* xdata, uint32 y)
	/*@modifies result @*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
mpw mpaddsqrtrc(size_t size, /*@out@*/ mpw* result, const mpw* xdata)
	/*@modifies result @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mpmul(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI
void mpsqr(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI
void mpgcd_w(size_t size, const mpw* xdata, const mpw* ydata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
uint32 mpnmodw(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata, uint32 y, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpnmod(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mpndivmod(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpprint(/*@null@*/ FILE * fp, size_t xsize, /*@null@*/ const mpw* xdata)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpprintln(/*@null@*/ FILE * fp, size_t xsize, /*@null@*/ const mpw* xdata)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif
