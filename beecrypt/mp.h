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
 * This function zeroes a multi-precision integer of a given size.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 */
BEECRYPTAPI
void mpzero(size_t size, /*@out@*/ mpw* data)
	/*@modifies data @*/;

/**
 * This function fills each word of a multi-precision integer with a
 *  given value.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @param fill		The value fill the data with.
 */
BEECRYPTAPI /*@unused@*/
void mpfill(size_t size, /*@out@*/ mpw* data, mpw fill)
	/*@modifies data @*/;

/**
 * This function tests if a multi-precision integer is odd.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if odd, 0 if even
 */
BEECRYPTAPI
int mpodd (size_t size, const mpw* data)
	/*@*/;

/**
 * This function tests if a multi-precision integer is even.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if even, 0 if odd
 */
BEECRYPTAPI
int mpeven(size_t size, const mpw* data)
	/*@*/;

/**
 * This function tests if a multi-precision integer is zero.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if zero, 0 if not zero
 */
BEECRYPTAPI
int mpz  (size_t size, const mpw* data)
	/*@*/;

/**
 * This function tests if a multi-precision integer is not zero.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if not zero, 0 if zero
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mpnz (size_t size, const mpw* data)
	/*@*/;
/*@=exportlocal@*/

/**
 * This function tests if two multi-precision integers of the same size
 *  are equal.
 * @param size		The size of the multi-precision integers.
 * @param xdata		The first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if equal, 0 if not equal
 */
BEECRYPTAPI
int mpeq (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 * This function tests if two multi-precision integers of the same size
 *  differ.
 * @param size		The size of the multi-precision integers.
 * @param xdata		The first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return 1		if not equal, 0 if equal
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mpne (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;
/*@=exportlocal@*/

/**
 * This function tests if the first of two multi-precision integers
 *  of the same size is greater than the second.
 * @note The comparison treats the arguments as unsigned.
 * @param size		The size of the multi-precision integers.
 * @param xdata		The first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if greater, 0 if less or equal
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mpgt (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;
/*@=exportlocal@*/

/**
 * This function tests if the first of two multi-precision integers
 *  of the same size is less than the second.
 * @note The comparison treats the arguments as unsigned.
 * @param size		The size of the multi-precision integers.
 * @param xdata		The first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if less, 0 if greater or equal
 */
/*@-exportlocal@*/
BEECRYPTAPI
int mplt (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;
/*@=exportlocal@*/

/**
 * This function tests if the first of two multi-precision integers
 *  of the same size is greater than or equal to the second.
 * @note The comparison treats the arguments as unsigned.
 * @param size		The size of the multi-precision integers.
 * @param xdata		The first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if greater or equal, 0 if less
 */
BEECRYPTAPI
int mpge (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 * This function tests if the first of two multi-precision integers
 *  of the same size is less than or equal to the second.
 * @note The comparison treats the arguments as unsigned.
 * @param size		The size of the multi-precision integers.
 * @param xdata		The first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if less or equal, 0 if greater
 */
BEECRYPTAPI
int mple (size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 * This function tests if two multi-precision integers of different
 *  size are equal.
 * @param xsize		The size of the first multi-precision integer.
 * @param xdata		The first multi-precision integer.
 * @param ysize		The size of the first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if equal, 0 if not equal
 */
BEECRYPTAPI
int mpeqx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 * This function tests if two multi-precision integers of different
 *  size differ.
 * @param xsize		The size of the first multi-precision integer.
 * @param xdata		The first multi-precision integer.
 * @param ysize		The size of the first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if not equal, 0 if equal
 */
BEECRYPTAPI /*@unused@*/
int mpnex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 * This function tests if the first of two multi-precision integers
 *  of different size is greater than the second.
 * @note The comparison treats the arguments as unsigned.
 * @param xsize		The size of the first multi-precision integer.
 * @param xdata		The first multi-precision integer.
 * @param ysize		The size of the second multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if greater, 0 if less or equal
 */
BEECRYPTAPI /*@unused@*/
int mpgtx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 * This function tests if the first of two multi-precision integers
 *  of different size is less than the second.
 * @note The comparison treats the arguments as unsigned.
 * @param xsize		The size of the first multi-precision integer.
 * @param xdata		The first multi-precision integer.
 * @param ysize		The size of the second multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if less, 0 if greater or equal
 */
BEECRYPTAPI /*@unused@*/
int mpltx(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 * This function tests if the first of two multi-precision integers
 *  of different size is greater than or equal to the second.
 * @note The comparison treats the arguments as unsigned.
 * @param xsize		The size of the first multi-precision integer.
 * @param xdata		The first multi-precision integer.
 * @param ysize		The size of the second multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if greater or equal, 0 if less
 */
BEECRYPTAPI
int mpgex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 * This function tests if the first of two multi-precision integers
 *  of different size is less than or equal to the second.
 * @note The comparison treats the arguments as unsigned.
 * @param xsize		The size of the first multi-precision integer.
 * @param xdata		The first multi-precision integer.
 * @param ysize		The size of the second multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if less or equal, 0 if greater
 */
BEECRYPTAPI
int mplex(size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@*/;

/**
 * This function tests if the value of a multi-precision integer is
 *  equal to one.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if equal to one, 0 otherwise.
 */
BEECRYPTAPI
int mpisone(size_t size, const mpw* data)
	/*@*/;

/**
 * This function tests if the value of a multi-precision integer is
 *  equal to two.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if equal to two, 0 otherwise.
 */
BEECRYPTAPI
int mpistwo(size_t size, const mpw* data)
	/*@*/;

/**
 * This function tests if the value of a multi-precision integer is
 *  less than or equal to one.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if less than or equal to one, 0 otherwise.
 */
BEECRYPTAPI
int mpleone(size_t size, const mpw* data)
	/*@*/;

/**
 * This function tests if multi-precision integer x is equal to y minus one.
 * @param size		The size of the multi-precision integers.
 * @param xdata		The first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		1 if less than or equal to (y-1), 0 otherwise.
 */
BEECRYPTAPI /*@unused@*/
int mpeqmone(size_t size, const mpw* xdata, const mpw* ydata)
	/*@*/;

/**
 * This function tests the most significant bit of a multi-precision integer.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if set, 0 if not set
 */
BEECRYPTAPI
int mpmsbset(size_t size, const mpw* data)
	/*@*/;

/**
 * This function tests the least significant bit of a multi-precision integer.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 * @return		1 if set, 0 if not set
 */
BEECRYPTAPI /*@unused@*/
int mplsbset(size_t size, const mpw* data)
	/*@*/;

/**
 * This function sets the most significant bit of a multi-precision integer.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 */
BEECRYPTAPI /*@unused@*/
void mpsetmsb(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 * This function sets the least significant bit of a multi-precision integer.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 */
BEECRYPTAPI
void mpsetlsb(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 * This function clears the most significant bit of a multi-precision integer.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 */
BEECRYPTAPI /*@unused@*/
void mpclrmsb(size_t size, mpw* data)
	/*@modifies data @*/;

/**
 * This function clears the least significant bit of a multi-precision integer.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
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
 * This function flips all bits of a multi-precision integer.
 * @param size		The size of the multi-precision integer.
 * @param data		The multi-precision integer data.
 */
/*@-exportlocal@*/
BEECRYPTAPI
void mpnot(size_t size, mpw* data)
	/*@modifies data @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mpsetw(size_t size, /*@out@*/ mpw* xdata, mpw y)
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
 * This function adds two multi-precision integers of equal size.
 * The performed operation is in pseudocode: x += y
 * @param size		The size of the multi-precision integers.
 * @param xdata		The first multi-precision integer.
 * @param ydata		The second multi-precision integer.
 * @return		The carry-over value of the operation (either 0 or 1).
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
 * This function performs a multi-precision multiply-setup.
 *
 * This function is used in the computation of a full multi-precision
 * multiplication. By using it we can shave off a few cycles; otherwise we'd
 * have to zero the least significant half of the result first and use
 * another call to the slightly slower mpaddmul function.
 *
 * @param size		The size of multi-precision integer multiplier.
 * @param result	The place where result will be accumulated.
 * @param data		The multi-precision integer multiplier.
 * @param y		The multiplicand.
 * @return		The carry-over multi-precision word.
 */
BEECRYPTAPI
mpw mpsetmul   (size_t size, /*@out@*/ mpw* result, const mpw* data, mpw y)
	/*@modifies result @*/;

/**
 * This function performs a mult-precision multiply-accumulate.
 *
 * This function is used in the computation of a full multi-precision
 * multiplication. It computes the product-by-one-word and accumulates it with
 * the previous result.
 *
 * @param size		The size of multi-precision integer multiplier.
 * @param result	The place where result will be accumulated.
 * @param data		The multi-precision integer multiplier.
 * @param y		The multiplicand.
 * @return		The carry-over multi-precision word.
 */
BEECRYPTAPI
mpw mpaddmul   (size_t size, /*@out@*/ mpw* result, const mpw* data, mpw y)
	/*@modifies result @*/;

/**
 * This function is used in the calculation of a multi-precision
 * squaring.
 */
/*@-exportlocal@*/
BEECRYPTAPI
void mpaddsqrtrc(size_t size, /*@out@*/ mpw* result, const mpw* data)
	/*@modifies result @*/;
/*@=exportlocal@*/

/**
 * This function computes a full multi-precision product.
 */
BEECRYPTAPI
void mpmul(/*@out@*/ mpw* result, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata)
	/*@modifies result @*/;

/**
 * This function computes a full multi-precision square.
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
void mpprint(size_t size, /*@null@*/ const mpw* data)
	/*@globals stdout, fileSystem @*/
	/*@modifies stdout, fileSystem @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpprintln(size_t size, /*@null@*/ const mpw* data)
	/*@globals stdout, fileSystem @*/
	/*@modifies stdout, fileSystem @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpfprint(/*@null@*/ FILE * f, size_t size, /*@null@*/ const mpw* data)
	/*@globals fileSystem @*/
	/*@modifies *f, fileSystem @*/;

/**
 */
BEECRYPTAPI
void mpfprintln(/*@null@*/ FILE * f, size_t size, /*@null@*/ const mpw* data)
	/*@globals fileSystem @*/
	/*@modifies *f, fileSystem @*/;

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
