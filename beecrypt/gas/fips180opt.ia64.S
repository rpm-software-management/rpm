/*
 * fips180opt.ia64.S
 *
 * Assembler optimized SHA-1 routines for ia64 (Intel Itanium)
 *
 * Warning: this code is incomplete and only contains a rough prototype!
 *
 * Compile target is GNU Assembler
 *
 * Copyright (c) 2001 Virtual Unlimited B.V.
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

#include "config.gas.h"

#define saved_pfs	r14
#define saved_lc	r15

#define param		r16

	.file	"fips180opt.ia64.S"

	.text

	.equ	K00,	0x5a827999
	.equ	K20,	0x6ed9eba1
	.equ	K40,	0x8f1bbcdc
	.equ	K60,	0xca62c1d6

	.equ    PARAM_H,        0
	.equ    PARAM_DATA,     20

/* for optimization, I have to see how I can parallellize the code

	e = ROTL32(a, 5) + ((b&(c^d))^d) + e + w + K
	b = ROTR32(b, 2);

step1: load w, tmp0 = mix a, tmp1 = c xor d, e += K;;
step2: tmp0 >>= 27, tmp1 &= b, e += w, b = mix b;;
step3: b >>= 2, e += tmp0, tmp1 ^= d;;
step4: e += tmp1, load next w, tmp0 = mix d, tmp1 = b xor d, d += K;;
step5: etc.

	d = ROTL32(d, 5) + ((a&(b^c))^c) + d + w + K
	a = ROTR32(a, 2)
*/

	.macro	subround1	a b c d e w
	ld4 r19 = [\w],4
	add \e = $K00,\e
	xor r21 = \c,\d
	mix4.r r20 = \a,\a;;
	add \e = \e,r19
	and r21 = r21,\b
	shr.u r20 = 27,r20
	mix4.r r22 = \b,\b;;
	add \e = r20,\e
	xor r21 = r21,\d
	shr.u \b = 2,r22;;
	add \e = r21,\e
	.endm

	.macro	subround2	a b c d e w
	ld4 r19 = [\w],4
	add \e = $K20,\e
	xor r21 = \b,\c
	mix4.r r20 = \a,\a;;
	add \e = \e,r19
	xor r21 = r21,\d
	shr.u r20 = 27,r20
	mix4.r \b = \b,\b;;
	add \e = r20,\e
	shr.u \b = 2,\b;;
	add \e = r21,\e
	.endm

	.macro	subround3	a b c d e w
	ld4 r19 = [\w],4
	add \e = $K40,\e
	xor r21 = \b,\c
	and r22 = \b,\c
	mix4.r r20 = \a,\a;;
	add \e = \e,r19
	and r21 = r21,\d
	shr.u r20 = 27,r20
	mix4.r \b = \b,\b;;
	add \e = r20,\e
	or r21 = r21,r22
	shr.u \b = 2,\b;;
	add \e = r21,\e
	.endm

	.macro	subround4	a b c d e w
	ld4 r19 = [\w],4
	add \e = $K60,\e
	xor r21 = \b,\c
	mix4.r r20 = \a,\a;;
	add \e = \e,r19
	xor r21 = r21,\d
	shr.u r20 = 27,r20
	mix4.r \b = \b,\b;;
	add \e = r20,\e
	shr.u \b = 2,\b;;
	add \e = r21,\e
	.endm

	.align	32
	.global	sha1Process#
	.proc	sha1Process#

sha1Process:
	alloc saved_pfs = ar.pfs,2,0,0,0
	mov saved_lc = ar.lc

/*	r16 will be h */
/*	r17 will be pdata */
/*	There must be something neat I can do to speed up expansion (xor/rotate)

	The following should work, if we use 24 rotating registers; speedup should be dramatic
	preload with swapped values 0-15

	rought draft: have to translate this to more precise rotating registers and predicates.

	/----------\
	|xor[2],[0]|
	+----------+----------\
	|xor[8]    |xor[3],[1]|
	+----------+----------+----------\
	|xor[13]   |xor[9]    |xor[4],[2]|
	+----------+----------+----------+----------\
	|mix4.r[16]|xor[14]   |xor[10]   |xor[5],[3]|
	+----------+----------+----------+----------+-----------\
	|shr[16]   |mix4.r[17]|xor[15]   |xor[11]   |xor[6],[4] |
	+----------+----------+----------+----------+-----------+----------\
	|store[16] |shr[17]   |mix4.r[18]|xor[16]   |xor[12]    |xor[7],[5]|
	\----------+----------+----------+----------+-----------+----------+----------\
	           |store[17] |shr[18]   |mix4.r[19]|xor[17]    |xor[13]   |xor[8],[6]|
	           \----------+----------+----------+-----------+----------+----------+----------\
	                      |store[18] |shr[19]   |mix4.r[20] |xor[18]   |xor[14]   |xor[9],[7]|
	                      \----------+----------+-----------+----------+----------+----------+----------\
	                                 |          |           |          |          |          |          |
*/
	alloc saved_pfs = ar.pfs,3,21,0,24

	/* look into big-endian loads, followed by little-endian stores */
#if !WORD_BIGENDIAN
	// save UM.be 
	// set UM.be to one
#endif
	/*
.L00:
	ld4 r32 = [ra],4
	
br.ctop.sptk .L00;;
#if !WORD_BIGENDIAN
	// restore UM.be
	/*
	mov ra = rd
	mov rb = rd;;
	add rb = 4,rd;;
	st4 [ra],8 = r48
	st4 [rb],8 = r47;;
	st4 [ra],8 = r46
	st4 [rb],8 = r45;;
	st4 [ra],8 = r44
	st4 [rb],8 = r43;;
	st4 [ra],8 = r42
	st4 [rb],8 = r41;;
	st4 [ra],8 = r40
	st4 [rb],8 = r39;;
	st4 [ra],8 = r38
	st4 [rb],8 = r37;;
	st4 [ra],8 = r36
	st4 [rb],8 = r35;;
	st4 [ra],8 = r34
	st4 [rb],8 = r33;;
	*/
#endif
	/* also add a conditional which will save the original swapped words! */
	/* the expansion loop will translate to something like this: */
.L01:
	/* put three xors together */
	(p16) xor r32 = r46,r48
	(p17) xor r33 = r33,r41
	(p18) xor r34 = r34,r37
	(p19) mix4.r r35 = r35,r35
	(p20) shr.u r36 = 31,r36
	(p21) st4 [],4 = r37
	br.ctop.sptk .L01;;

	etc.
*/
	mov ar.lc = r15
	mov ar.pfs = r14
	br.ret.sptk b0
	.endp	sha1Process#

