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

/*!\file mp.h
 * \brief Multi-precision integer routines, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP_m
 */

#ifndef _MP_H
#define _MP_H

#include "beecrypt.h"
#include "mpopt.h"

#define MP_HWBITS	(MP_WBITS >> 1)
#define MP_WBYTES	(MP_WBITS >> 3)
#define MP_WNIBBLES	(MP_WBITS >> 2)

#if (MP_WBITS == 64)
# define MP_WORDS_TO_BITS(x)	((x) << 6)
# define MP_WORDS_TO_NIBBLES(x)	((x) << 4)
# define MP_WORDS_TO_BYTES(x)	((x) << 3)
# define MP_BITS_TO_WORDS(x)	((x) >> 6)
# define MP_NIBBLES_TO_WORDS(x)	((x) >> 4)
# define MP_BYTES_TO_WORDS(x)	((x) >> 3)
#elif (MP_WBITS == 32)
# define MP_WORDS_TO_BITS(x)	((x) << 5)
# define MP_WORDS_TO_NIBBLES(x)	((x) << 3)
# define MP_WORDS_TO_BYTES(x)	((x) << 2)
# define MP_BITS_TO_WORDS(x)	((x) >> 5) 
# define MP_NIBBLES_TO_WORDS(x)	((x) >> 3)
# define MP_BYTES_TO_WORDS(x)	((x) >> 2)
#else
# error
#endif

#define MP_MSBMASK	(((mpw) 0x1) << (MP_WBITS-1))
#define MP_LSBMASK	 ((mpw) 0x1)
#define MP_ALLMASK	~((mpw) 0x0)

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI /*@unused@*/
void mpcopy(size_t size, /*@out@*/ mpw* dst, const mpw* src)
	/*@modifies dst @*/;
#ifndef ASM_MPCOPY
# define mpcopy(size, dst, src) \
	/*@-aliasunique -mayaliasunique @*/ \
	memcpy(dst, src, MP_WORDS_TO_BYTES((unsigned)size)) \
	/*@=aliasunique =mayaliasunique @*/
#endif

/**
 */
BEECRYPTAPI /*@unused@*/
void mpmove(size_t size, /*@out@*/ mpw* dst, const mpw* src)
	/*@modifies dst @*/;
#ifndef ASM_MPMOVE
# define mpmove(size, dst, src) memmove(dst, src, MP_WORDS_TO_BYTES((unsigned)size))
#endif

/**
 */
BEECRYPTAPI
void mpzero(size_t size, /*@out@*/ mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpfill(size_t size, /*@out@*/ mpw* data, mpw fill)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
int mpodd (size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpeven(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpz  (size_t size, const mpw* data)
	/*@*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mpnz (size_t size, const mpw* data)
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
int mpisone(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpistwo(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpleone(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int mpeqmone(size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 */
BEECRYPTAPI
int mpmsbset(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int mplsbset(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpsetmsb(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
void mpsetlsb(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpclrmsb(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpclrlsb(size_t size, mpw* data)
	/*@modifies data @*/;

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
void mpnot(size_t size, mpw* data)
	/*@modifies data @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mpsetw(size_t xsize, /*@out@*/ mpw* xdata, mpw y)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
void mpsetx(size_t xsize, /*@out@*/ mpw* xdata, size_t ysize, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpaddw(size_t size, mpw* xdata, mpw y)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpadd (size_t size, mpw* xdata, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpaddx(size_t xsize, mpw* xdata, size_t ysize, const mpw* ydata)
	/*@modifies xdata @*/;

/**
 */
BEECRYPTAPI
int mpsubw(size_t size, mpw* xdata, mpw y)
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
int mpmultwo(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
void mpneg(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
size_t mpsize(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI
size_t mpbits(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI
size_t mpmszcnt(size_t size, const mpw* data)
	/*@*/;

/**
 */
BEECRYPTAPI /*@unused@*/
size_t mpbitcnt(size_t size, const mpw* data)
	/*@*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI /*@unused@*/
size_t mplszcnt(size_t size, const mpw* data)
	/*@*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mplshift(size_t size, mpw* data, size_t count)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
void mprshift(size_t size, mpw* data, size_t count)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
size_t mprshiftlsz(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
size_t mpnorm(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
void mpdivtwo (size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
void mpsdivtwo(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 */
BEECRYPTAPI
mpw mpsetmul   (size_t size, /*@out@*/ mpw* result, const mpw* data, mpw y)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI
mpw mpaddmul   (size_t size, /*@out@*/ mpw* result, const mpw* data, mpw y)
	/*@modifies result @*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
mpw mpaddsqrtrc(size_t size, /*@out@*/ mpw* result, const mpw* data)
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
void mpsqr(/*@out@*/ mpw* result, size_t size, const mpw* data)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI
void mpgcd_w(size_t size, const mpw* xdata, const mpw* ydata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
mpw mppndiv(mpw xhi, mpw xlo, mpw y)
	/*@*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI /*@unused@*/
mpw mpnmodw(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata, mpw y, /*@out@*/ mpw* workspace)
	/*@modifies result, workspace @*/;

/**
 */
BEECRYPTAPI
void mpnmod(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, /*@out@*/ mpw* workspace)
	/*@modifies result, workspace @*/;

/**
 */
BEECRYPTAPI
void mpndivmod(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, /*@out@*/ mpw* workspace)
	/*@modifies result, workspace @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpprint(/*@null@*/ FILE * fp, size_t size, /*@null@*/ const mpw* data)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

/**
 */
BEECRYPTAPI
void mpprintln(/*@null@*/ FILE * fp, size_t size, /*@null@*/ const mpw* data)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

/**
 */
BEECRYPTAPI
int i2osp(/*@out@*/ byte *osdata, size_t ossize, const mpw* idata, size_t isize)
	/*@modifies osdata @*/;

/**
 */
BEECRYPTAPI
int os2ip(/*@out@*/ mpw *idata, size_t isize, const byte* osdata, size_t ossize)
	/*@modifies idata @*/;

/**
 */
BEECRYPTAPI
int hs2ip(/*@out@*/ mpw* idata, size_t isize, const char* hsdata, size_t hssize)
	/*@modifies idata @*/;

#ifdef __cplusplus
}
#endif

#endif
