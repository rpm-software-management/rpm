#
# mp32opt.gas.powerpc.s
#
# Assembler optimized multiprecision integer routines for PowerPC
#
# Compile target is GNU AS
#
# Copyright (c) 2000 Virtual Unlimited B.V.
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

	.file "mp32opt.gas.powerpc.s"

	.align	4
	.globl	mp32addw
	.type	mp32addw,@function

mp32addw:
	mtctr %r3
	slwi %r0,%r3,2
	add %r4,%r4,%r0
	li %r0,0
	lwzu %r6,-4(%r4)
	addc %r6,%r6,%r5
	stw %r6,0(%r4)
	bdz .L01
.L00:
	lwzu %r6,-4(%r4)
	adde %r6,%r0,%r6
	stw %r6,0(%r4)
	bdnz .L00
.L01:	# return the carry
	addze %r3,%r0
	blr

	.if 0	# doesn't work yet
	.align	4
	.globl	mp32subw
	.type	mp32subw,@function

mp32subw:
	mtctr %r3
	slwi %r0,%r3,2
	add %r4,%r4,%r0
	li %r0,0
	lwz %r6,-4(%r4)
	subfc %r6,%r5,%r6
	stwu %r6,-4(%r4)
	bdz .L11
.L10:
	lwz %r6,-4(%r4)
	subfe %r6,%r0,%r6
	stwu %r6, -4(%r4)
	bdnz .L10
.L11:	# return the carry
	addze %r3,%r0
	blr
	.endif

	.align	4
	.globl	mp32add
	.type	mp32add,@function

mp32add:
	mtctr %r3
	slwi %r0,%r3,2
	add %r4,%r4,%r0
	add %r5,%r5,%r0
	li %r0,0
	lwz %r6,-4(%r4)
	lwzu %r7,-4(%r5)
	addc %r6,%r7,%r6
	stwu %r6,-4(%r4)
	bdz .L21
.L20:
	lwz %r6,-4(%r4)
	lwzu %r7,-4(%r5)
	adde %r6,%r7,%r6
	stwu %r6,-4(%r4)
	bdnz .L20
.L21:	# return the carry
	addze %r3,%r0
	blr

	.if 0	# doesn't work yet
	.align	4
	.globl	mp32sub
	.type	mp32sub,@function

mp32sub:
	mtctr %r3
	slwi %r0,%r3,2
	add %r4,%r4,%r0
	add %r5,%r5,%r0
	li %r0,0
	lwz %r6,-4(%r4)
	lwzu %r7,-4(%r5)
	subfc %r6,%r7,%r6
	stwu %r6,-4(%r4)
	bdz .L31
.L30:
	lwz %r6,-4(%r4)
	lwzu %r7,-4(%r5)
	subfe %r6,%r7,%r6
	stwu %r6,-4(%r4)
	bdnz .L30
.L31:	# return the carry
	addze %r3,%r0
	blr
	.endif

	.align	4
	.globl	mp32setmul
	.type	mp32setmul,@function

# size %r3
# dst  %r4
# src  %r5
# mulw %r6

mp32setmul:
	mtctr %r3
	slwi %r0,%r3,2
	add %r4,%r4,%r0
	add %r5,%r5,%r0
	li %r3,0
.L40:
	lwzu %r7,-4(%r5)
	mullw %r8,%r7,%r6
	addc %r8,%r8,%r3
	mulhwu %r3,%r7,%r6
	addze %r3,%r3
	stwu %r8,-4(%r4)
	bdnz .L40
	blr

	.align	4
	.globl	mp32addmul
	.type	mp32addmul,@function

# size %r3
# dst  %r4
# src  %r5
# mulw %r6

mp32addmul:
	mtctr %r3
	slwi %r0,%r3,2
	add %r4,%r4,%r0
	add %r5,%r5,%r0
	li %r3,0
.L50:
	lwzu %r7,-4(%r4)
	lwzu %r8,-4(%r5)
	mullw %r9,%r8,%r6
	addc %r9,%r9,%r3
	mulhwu %r3,%r8,%r6
	addze %r3,%r3
	addc %r9,%r9,%r7
	addze %r3,%r3
	stw %r9,0(%r4)
	bdnz .L50
	blr

	.align	4
	.globl	mp32addsqrtrc
	.type	mp32addsqrtrc,@function

# size %r3
# dst  %r4
# src  %r5

mp32addsqrtrc:
	mtctr %r3
	slwi %r0,%r3,2
	add %r4,%r4,%r0
	add %r5,%r5,%r0
	add %r4,%r4,%r0
	li %r3,0
.L60:
	lwzu %r0,-4(%r5)
	lwz %r6,-8(%r4)
	lwz %r7,-4(%r4)
	mullw %r9,%r0,%r0
	mulhwu %r8,%r0,%r0
	addc %r9,%r9,%r3
	addze %r8,%r8
	addc %r7,%r7,%r9
	adde %r6,%r6,%r8
	li %r3,0
	addze %r3,%r3
	stw %r7,-4(%r4)
	stwu %r6,-8(%r4)
	bdnz .L60
	blr
