#
# mp32opt.gas.sparcv9.s
#
# Assembler optimized multiprecision integer routines for UltraSparc (64 bits instructions, will run on 32 bit OS)
#
# Compile target is GNU AS
#
# Copyright (c) 1998-2000 Virtual Unlimited B.V.
#
# Author: Bob Deblier <bob@virtualunlimited.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

	.file "mp32opt.gas.sparcv9.s"

	.section	".text"

	.align	4
	.globl	mp32addw
	.type	mp32addw,@function

# INPUT PARAMETERS
# size	%o0
# data	%o1
# addw	%o2

mp32addw:
	sll %o0,2,%g1
	sub %g1,4,%g1
	mov %g0,%o0
	lduw [%o1+%g1],%g2
	addcc %g2,%o2,%g2
	stw %g2,[%o1+%g1]
	brz,pn %g1,.L01
	sub %g1,4,%g1
.L00:
	lduw [%o1+%g1],%g2
	addccc %g2,%g0,%g2
	stw %g2,[%o1+%g1]
	brnz,pt %g1,.L00
	sub %g1,4,%g1
.L01:
	retl
	movcs %icc,1,%o0

	.align	4
	.globl	mp32subw
	.type	mp32subw,@function

mp32subw:
	sll %o0,2,%g1
	sub %g1,4,%g1
	mov %g0,%o0
	lduw [%o1+%g1],%g2
	subcc %g2,%o2,%g2
	stw %g2,[%o1+%g1]
	brz,pn %g1,.L11
	sub %g1,4,%g1
.L10:
	lduw [%o1+%g1],%g2
	subccc %g2,%g0,%g2
	stw %g2,[%o1+%g1]
	brnz,pt %g1,.L10
	sub %g1,4,%g1
.L11:
	retl
	movcs %icc,1,%o0

	.align	4
	.globl	mp32add
	.type	mp32add,@function

mp32add:
	sll %o0,2,%g1
	sub %g1,4,%g1
	addcc %g0,%g0,%o0
.L20:
	lduw [%o1+%g1],%g2
	lduw [%o2+%g1],%g3
	addccc %g2,%g3,%g4
	stw %g4,[%o1+%g1]
	brnz,pt %g1,.L20
	sub %g1,4,%g1
	retl
	movcs %icc,1,%o0

	.align	4
	.globl	mp32sub
	.type	mp32sub,@function

mp32sub:
	sll %o0,2,%g1
	sub %g1,4,%g1
	addcc %g0,%g0,%o0
.L30:
	lduw [%o1+%g1],%g2
	lduw [%o2+%g1],%g3
	subccc %g2,%g3,%g4
	stw %g4,[%o1+%g1]
	brnz,pt %g1,.L30
	sub %g1,4,%g1
	retl
	movcs %icc,1,%o0

	.align	4
	.globl	mp32setmul
	.type	mp32setmul,@function

mp32setmul:
	sll %o0,2,%g1
	sub %g1,4,%g1
	mov %g0,%o0
.L40:
	lduw [%o2+%g1],%g2
	mulx %o3,%g2,%g3
	add %o0,%g3,%o0
	stw %o0,[%o1+%g1]
	srlx %o0,32,%o0
	brnz,pt %g1,.L40
	sub %g1,4,%g1
	retl
	nop

	.align	4
	.globl	mp32addmul
	.type	mp32addmul,@function

mp32addmul:
	sll %o0,2,%g1
	sub %g1,4,%g1
	mov %g0,%o0
.L50:
	lduw [%o2+%g1],%g2
	lduw [%o1+%g1],%g4
	mulx %o3,%g2,%g3
	add %o0,%g3,%o0
	add %o0,%g4,%o0
	stw %o0,[%o1+%g1]
	srlx %o0,32,%o0
	brnz,pt %g1,.L50
	sub %g1,4,%g1
	retl
	nop

.if 0
	# not finished !
	.align	4
	.globl	mp32addsqrtrc
	.type	mp32addsqrtrc,@function

mp32addsqrtrc:
	sll %o0,2,%g1
	add %o1,%g1,%o1
	sub %g1,4,%g1
	add %o1,%g1,%o1
	mov %g0,%o0

.L60:
	lduw [%o2+%g1],%g2
	lduw [%o1],%g4
	mulx %g2,%g2,%g3
	add %o0,%g3,%o0
	add %o0,%g4,%o0
	stw %o0,[%o1]
	sub %o1,4,%o1
	srlx %o0,32,%o0
	lduw [%o1],%g4
	add %o0,%g4,%g0
	stw %o0,[%o1]
	sub %o1,4,%o1
	srlx %o0,32,%o0
	brnz,pt %g1,.L60
	sub %g1,4,%g1
	retl
	nop
.endif
