/*
 * mp32opt.gas.powerpc.S
 *
 * Assembler optimized multiprecision integer routines for PowerPC
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

	.file "mp32opt.gas.powerpc.S"

	.text

#if defined(DARWIN)
# define reg0	r0
# define reg3	r3
# define reg4	r4
# define reg5	r5
# define reg6	r6
# define reg7	r7
# define reg8	r8
# define reg9	r9
#else
# define reg0	%r0
# define reg3	%r3
# define reg4	%r4
# define reg5	%r5
# define reg6	%r6
# define reg7	%r7
# define reg8	%r8
# define reg9	%r9
#endif

	.align	4
#if defined(DARWIN)
	.globl	_mp32addw

_mp32addw:
#else
	.global	mp32addw
	.type	mp32addw,@function

mp32addw:
#endif
	mtctr reg3
	slwi reg0,reg3,2
	add reg4,reg4,reg0
	li reg0,0
	lwzu reg6,-4(reg4)
	addc reg6,reg6,reg5
	stw reg6,0(reg4)
	bdz .L01
.L00:
	lwzu reg6,-4(reg4)
	adde reg6,reg0,reg6
	stw reg6,0(reg4)
	bdnz .L00
.L01:	# return the carry
	addze reg3,reg0
	blr

	.align	4
#if defined(DARWIN)
	.globl	_mp32subw

_mp32subw:
#else
	.global	mp32subw
	.type	mp32subw,@function

mp32subw:
#endif
	mtctr reg3
	slwi reg0,reg3,2
	add reg4,reg4,reg0
	li reg0,0
	lwz reg6,-4(reg4)
	subfc reg6,reg5,reg6
	stwu reg6,-4(reg4)
	bdz .L11
.L10:
	lwz reg6,-4(reg4)
	subfe reg6,reg0,reg6
	stwu reg6, -4(reg4)
	bdnz .L10
.L11:	# return the carry
	subfe reg3,reg0,reg0
	neg reg3,reg3
	blr

	.align	4
#if defined(DARWIN)
	.globl	_mp32add

_mp32add:
#else
	.global	mp32add
	.type	mp32add,@function

mp32add:
#endif
	mtctr reg3
	slwi reg0,reg3,2
	add reg4,reg4,reg0
	add reg5,reg5,reg0
	li reg0,0
	lwz reg6,-4(reg4)
	lwzu reg7,-4(reg5)
	addc reg6,reg7,reg6
	stwu reg6,-4(reg4)
	bdz .L21
.L20:
	lwz reg6,-4(reg4)
	lwzu reg7,-4(reg5)
	adde reg6,reg7,reg6
	stwu reg6,-4(reg4)
	bdnz .L20
.L21:	# return the carry
	addze reg3,reg0
	blr

	.align	4
#if defined(DARWIN)
	.globl	_mp32sub

_mp32sub:
#else
	.global	mp32sub
	.type	mp32sub,@function

mp32sub:
#endif
	mtctr reg3
	slwi reg0,reg3,2
	add reg4,reg4,reg0
	add reg5,reg5,reg0
	li reg0,0
	lwz reg6,-4(reg4)
	lwzu reg7,-4(reg5)
	subfc reg6,reg7,reg6
	stwu reg6,-4(reg4)
	bdz .L31
.L30:
	lwz reg6,-4(reg4)
	lwzu reg7,-4(reg5)
	subfe reg6,reg7,reg6
	stwu reg6,-4(reg4)
	bdnz .L30
.L31:	# return the carry
	subfe reg3,reg0,reg0
	neg reg3,reg3
	blr

	.align	4
#if defined(DARWIN)
	.globl	_mp32setmul

_mp32setmul:
#else
	.globl	mp32setmul
	.type	mp32setmul,@function

mp32setmul:
#endif
	mtctr reg3
	slwi reg0,reg3,2
	add reg4,reg4,reg0
	add reg5,reg5,reg0
	li reg3,0
.L40:
	lwzu reg7,-4(reg5)
	mullw reg8,reg7,reg6
	addc reg8,reg8,reg3
	mulhwu reg3,reg7,reg6
	addze reg3,reg3
	stwu reg8,-4(reg4)
	bdnz .L40
	blr

	.align	4
#if defined(DARWIN)
	.globl	_mp32addmul

_mp32addmul:
#else
	.globl	mp32addmul
	.type	mp32addmul,@function

mp32addmul:
#endif
	mtctr reg3
	slwi reg0,reg3,2
	add reg4,reg4,reg0
	add reg5,reg5,reg0
	li reg3,0
.L50:
	lwzu reg7,-4(reg4)
	lwzu reg8,-4(reg5)
	mullw reg9,reg8,reg6
	addc reg9,reg9,reg3
	mulhwu reg3,reg8,reg6
	addze reg3,reg3
	addc reg9,reg9,reg7
	addze reg3,reg3
	stw reg9,0(reg4)
	bdnz .L50
	blr

	.align	4
#if defined(DARWIN)
	.globl	_mp32addsqrtrc

_mp32addsqrtrc:
#else
	.globl	mp32addsqrtrc
	.type	mp32addsqrtrc,@function

mp32addsqrtrc:
#endif
	mtctr reg3
	slwi reg0,reg3,2
	add reg4,reg4,reg0
	add reg5,reg5,reg0
	add reg4,reg4,reg0
	li reg3,0
.L60:
	lwzu reg0,-4(reg5)
	lwz reg6,-8(reg4)
	lwz reg7,-4(reg4)
	mullw reg9,reg0,reg0
	mulhwu reg8,reg0,reg0
	addc reg9,reg9,reg3
	addze reg8,reg8
	addc reg7,reg7,reg9
	adde reg6,reg6,reg8
	li reg3,0
	addze reg3,reg3
	stw reg7,-4(reg4)
	stwu reg6,-8(reg4)
	bdnz .L60
	blr
