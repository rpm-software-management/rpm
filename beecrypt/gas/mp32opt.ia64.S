/*
 * mp32opt.ia64.S
 *
 * Assembler optimized multiprecision integer routines for ia64 (Intel Itanium)
 *
 * Compile target is GNU Assembler
 *
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
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

/*
 * I will need to implement 64 bit multiprecision assembler-optimized routines
 * before this platform can be tested adequately. The current 32 bit ones suffer
 * from loading into integer registers, conversion to floating point, doing the xma
 * and converting back to integer; 64 bit values can be loaded directly into
 * floating point registers, which should shave off a lot of cycles.
 */

#include "config.gas.h"

#define saved_pfs	r14
#define saved_lc	r15

#define size		r16
#define dst			r17
#define src			r18

	.file	"mp32opt.ia64.S"

	.text

	.explicit

	.align	32
	.global	mp32zero#
	.proc	mp32zero#

mp32zero:
	alloc saved_pfs = ar.pfs,2,0,0,0
	mov saved_lc = ar.lc
	sub size = in0,r0,1;;
	mov src = in1
	mov ar.lc = size;;
.L00:
	st4 [src] = r0,4
	br.ctop.sptk .L00
	;;
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
	.endp	mp32zero#


	.align	32
	.global	mp32copy#
	.proc	mp32copy#

mp32copy:
	alloc saved_pfs = ar.pfs,3,5,0,8
	mov saved_lc = ar.lc
	sub size = in0,r0,1
	mov dst = in1
	mov src = in2;;
	mov ar.lc = size
	mov ar.ec = 2
	mov pr.rot = (1 << 16);;
.L01:
	(p17) st4 [dst] = r33,4
	(p16) ld4 r32 = [src],4;;
	br.ctop.sptk .L01;;
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
	.endp	mp32copy#


	.if		0
	.align	32
	.global	mp32z
	.type	mp32z,@function

mp32z:
	alloc r14 = ar.pfs,2,6,0,8
	mov r15 = ar.lc
	sub r16= in0,r0,1
	mov r17 = in1
	mov r18 = in2;;
	mov r8 = 1
	mov pr.rot = 1 << 16
	mov ar.ec = 2
	mov ar.lc = r16;;
.L02:
	(p16) ld4 r32 = [r18],4
	(p18) cmp.eq p0,p32 = r34,r0
	(p33) mov r8 = r0
	(p33) br.exit
	.endif


	.align	32
	.global	mp32add#
	.proc	mp32add#

mp32add:
	alloc r14 = ar.pfs,3,0,0,0
	mov r15 = ar.lc
	# adjust size by -1
	sub r16 = in0,r0,1
	# clear carry
	mov r8 = r0;;
	# load addresses
	shladd r17 = r16,2,in1
	shladd r18 = r16,2,in2
	# load loop count
	mov ar.lc = r16;;
.L20:
	ld4 r20 = [r18],-4
	ld4 r19 = [r17]
	tbit.z p1,p2 = r8,32;;
	(p1) add r8 = r19,r20
	(p2) add r8 = r19,r20,1;;
	st4 [r17] = r8,-4
	br.cloop.sptk .L20;;
	extr.u r8 = r8,32,1
	mov ar.lc = r15
	mov ar.pfs = r14
	br.ret.sptk b0
	.endp	mp32add#


	.align	32
	.global	mp32sub#
	.proc	mp32sub#

mp32sub:
	alloc r14 = ar.pfs,3,0,0,0
	mov r15 = ar.lc
	# adjust size by -1
	sub r16 = in0,r0,1
	# clear carry
	mov r8 = r0;;
	# load addresses
	shladd r17 = r16,2,in1
	shladd r18 = r16,2,in2
	# load loop count
	mov ar.lc = r16;;
.L30:
	ld4 r20 = [r18],-4
	ld4 r19 = [r17]
	tbit.z p1,p2 = r8,32;;
	(p1) sub r8 = r19,r20
	(p2) sub r8 = r19,r20,1;;
	st4 [r17] = r8,-4
	br.cloop.sptk .L30;;
	extr.u r8 = r8,32,1
	mov ar.lc = r15
	mov ar.pfs = r14
	br.ret.sptk b0
	.endp	mp32sub#


	.if		0

	.align	32
	.global	mp32setmul#
	.proc	mp32setmul#

mp32setmul:
	alloc r14 = ar.pfs,4,0,0,0
	mov r15 = ar.lc
	# load mul
	setf.sig f96 = in3
	# adjust size by -1
	sub r16 = in0,r0,1
	# clear carry
	mov r8 = r0;;
	# adjust addresses
	shladd r17 = r16,2,in1
	shladd r18 = r16,2,in2
	# load loop count
	mov ar.lc = r16;;
.L40:
	ld4 r19 = [r18],-4;;
	setf.sig f98 = r8
	setf.sig f97 = r19;;
	# multiplication can only be done in f registers, but we do have a multiply-add
	xma.l f98 = f96,f97,f98;;
	getf.sig r8 = f98;;
	st4 [r17] = r8,-4
	shr.u r8 = r8,32
	br.cloop.sptk .L40;;
	mov ar.lc = r15
	mov ar.pfs = r14
	br.ret.sptk b0
	.endp	mp32setmul#


	.align	32
	.global	mp32addmul#
	.proc	mp32addmul#

mp32addmul:
	alloc saved_pfs = ar.pfs,4,0,0,0
	mov saved_lc = ar.lc
	# load mul
	setf.sig f96 = in3
	# adjust size by -1
	sub size = in0,r0,1
	# clear carry
	mov r8 = r0;;
	# adjust addresses
	shladd dst = size,2,in1
	shladd src = size,2,in2
	# load loop count
	mov ar.lc = r16;;
.L50:
	ld4 r19 = [dst]
	ld4 r20 = [dst],-4;;
	setf.sig f98 = r8
	setf.sig f97 = r20;;
	# multiplication can only be done in f registers, but we do have a multiply-add
	xma.l f98 = f96,f97,f98;;
	getf.sig r8 = f98;;
	add r8 = r8,r19;;
	st4 [r17] = r8,-4
	shr.u r8 = r8,32
	br.cloop.sptk .L50;;
	mov ar.lc = r15
	mov ar.pfs = r14
	br.ret.sptk b0
	.endp	mp32addmul#

	.endif


	.if 0
	.align	16
	.global	mp32addsqrtrc#
	.proc	mp32addsqrtrc#

mp32addsqrtrc:
	.endp	mp32addsqrtrc#
	.endif
