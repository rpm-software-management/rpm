/*
 * mp32opt.gas.ia64.S
 *
 * Assembler optimized multiprecision integer routines for ia64 (Intel Itanium)
 *
 * Compile target is GNU AS
 *
 * Copyright (c) 2000 Virtual Unlimited B.V.
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

#define saved_pfs	r14
#define saved_lc	r15

#define size		r16
#define dst			r17
#define src			r18

	.file	"mp32opt.gas.ia64.s"

	.text

	.explicit

	.align  32
	.global mp32zero
	.type   mp32zero,@function

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


	.align  32
	.global mp32copy
	.type   mp32copy,@function

mp32copy:
	alloc r14 = ar.pfs,3,5,0,8
	mov r15 = ar.lc
	sub r16 = in0,r0,1
	mov r17 = in1
	mov r18 = in2;;
	mov ar.lc = r16
	mov ar.ec = 3
	mov pr.rot = (1 << 16);;
.L01:
	(p16) ld4 r32 = [r18],4
	(p18) st4 [r17] = r34,4
	br.ctop.sptk .L01;;
	mov ar.lc = r15
	mov ar.pfs = r14
	br.ret.sptk b0


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
	.global	mp32add
	.type	mp32add,@function

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


	.align	32
	.global	mp32sub
	.type	mp32sub,@function

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


	.align	32
	.global	mp32setmul
	.type	mp32setmul,@function

mp32setmul:
	alloc r14 = ar.pfs,4,0,0,0
	mov r15 = ar.lc
	# adjust size by -1
	sub r16 = in0,r0,1
	# clear carry
	mov r8 = r0;;
	# adjust addresses
	shladd r17 = r16,2,in1
	shladd r18 = r16,2,in2
	# load loop count
	mov ar.lc = r16
	# load mul
	setf.sig f96 = in3;;
.L40:
	ld4 r19 = [r18],-4;;
	setf.sig f98 = r8
	setf.sig f97 = r19;;
	# multiplication can only be done in f registers, but we do have a multiply-add
	xma.l f98 = f96, f97, f98;;
	getf.sig r8 = f98;;
	st4 [r17] = r8,-4
	shr.u r8 = r8,32
	br.cloop.sptk .L40;;
	mov ar.lc = r15
	mov ar.pfs = r14
	br.ret.sptk b0


	.align	32
	.global	mp32addmul
	.type	mp32addmul,@function

mp32addmul:
	alloc r14 = ar.pfs,4,0,0,0
	mov r15 = ar.lc
	# adjust size by -1
	sub r16 = in0,r0,1
	# load mul
	# setf.sig f6 = r0
	setf.sig f7 = in3
	# clear carry
	mov r8 = r0;;
	# adjust addresses
	shladd r17 = r16,2,in1
	shladd r18 = r16,2,in2
	# load loop count
	mov ar.lc = r16;;
.L50:
	ld4 r19 = [r17]
	ld4 r20 = [r18],-4;;
	setf.sig f8 = r8
	setf.sig f32 = r20;;
	# multiplication can only be done in f registers, but we do have a multiply-add
	xma.l f8 = f7,f32,f8;;
	getf.sig r8 = f8;;
	add r8 = r8,r19;;
	st4 [r17] = r8,-4
	shr.u r8 = r8,32
	br.cloop.sptk .L50;;
	mov ar.lc = r15
	mov ar.pfs = r14
	br.ret.sptk b0


	.if 0
	.align	16
	.global	mp32addsqrtrc#
	.proc	mp32addsqrtrc#

mp32addsqrtrc:
	.endp	mp32addsqrtrc#
	.endif

