#
# fips180opt.gas.i586.asm
#
# Assembler optimized SHA-1 routines for Intel Pentium processors
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

.if 1
	.file "fips180opt.gas.i586.s"

	.text

	.equ	K00,	0x5a827999
	.equ	K20,	0x6ed9eba1
	.equ	K40,	0x8f1bbcdc
	.equ	K60,	0xca62c1d6

	.equ	PARAM_H,		0
	.equ	PARAM_DATA,		20

	.macro	subround1 b c d e w
	movl \c,%ecx
	movl \b,%ebx
	movl \d,%edx
	roll $5,%eax
	xorl %edx,%ecx
	addl \e,%eax
	andl %ebx,%ecx
	addl $K00,%eax
	rorl $2,%ebx
	addl \w(%esi,%edi),%eax
	xorl %ecx,%edx
	movl %ebx,\b
	addl %ebx,%eax
	movl %eax,\e
	.endm

	.macro subround2 b c d e w
	movl \c,%ecx
	movl \b,%ebx
	roll $5,%eax
	xorl %ebx,%ecx
	addl \e,%eax
	xorl \d,%ecx
	addl $K20,%eax
	rorl $2,%ebx
	addl \w(%esi,%edi),%eax
	movl %ebx,\b
	addl %ecx,%eax
	movl %eax,\e
	.endm

	.macro subround3 b c d e w
	movl \c,%ecx
	roll $5,%eax
	movl \b,%ebx
	movl %ecx,%edx
	addl \e,%eax
	orl %ebx,%ecx
	andl %ebx,%edx
	andl \d,%ecx
	addl $K40,%eax
	orl %edx,%ecx
	addl \w(%esi,%edi),%eax
	rorl $4,%ebx
	addl %ecx,%eax
	movl %ebx,\b
	movl %eax,\e
	.endm

	.macro subround4 b c d e w
	movl \c,%ecx
	movl \b,%ebx
	roll $5,%eax
	xorl %ebx,%ecx
	addl \e,%eax
	xorl \d,%ecx
	addl $K60,%eax
	rorl $2,%ebx
	addl \w(%esi,%edi),%eax
	movl %ebx,\b
	addl %ecx,%eax
	movl %eax,\e
	.endm

	.align	4
	.globl	sha1Process
	.type	sha1Process,@function

sha1Process:
	pushl %edi
	pushl %esi
	pushl %ebx

	pushl %ebp
	leal -20(%esp),%ebp

	movl 20(%esp),%esi
	leal PARAM_DATA(%esi),%edi

	movl $4,%ecx
.L0:
	movl (%esi,%ecx,4),%edx
	movl %edx,(%ebp,%ecx,4)
	decl %ecx
	jns .L0

	movl $15,%ecx
	xorl %eax,%eax

	.p2align 2
.L1:
	movl (%edi,%ecx,4),%edx
	bswap %edx
	mov %edx,(%edi,%ecx,4)
	decl %ecx
	jns .L1

	leal PARAM_DATA(%esi),%edi
	movl $16,%ecx

	.p2align 2
.L2:
	movl 52(%edi),%eax
	movl 56(%edi),%ebx
	xorl 32(%edi),%eax
	xorl 36(%edi),%ebx
	xorl 8(%edi),%eax
	xorl 12(%edi),%eax
	xorl (%edi),%ebx
	xorl 4(%edi),%ebx
	roll $1,%eax
	roll $1,%ebx
	movl %eax,64(%edi)
	movl %ebx,68(%edi)
	movl 60(%edi),%eax
	movl 64(%edi),%ebx
	xorl 36(%edi),%eax
	xorl 40(%edi),%ebx
	xorl 16(%edi),%eax
	xorl 20(%edi),%eax
	xorl 8(%edi),%ebx
	xorl 12(%edi),%ebx
	roll $1,%eax
	roll $1,%ebx
	movl %eax,72(%edi)
	movl %ebx,76(%edi)
	addl $16,%edi
	decl %ecx
	jnz .L2

	movl $PARAM_DATA,%edi

	movl (%ebp),%eax
.L01_20:
	subround1  4(%ebp), 8(%ebp), 12(%ebp), 16(%ebp), 0
	subround1   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround1 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround1 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround1  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround1  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround1   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround1 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround1 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround1  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround1  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround1   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround1 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround1 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround1  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround1  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround1   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround1 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround1 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround1  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi

.L21_40:
	subround2  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround2   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround2 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround2 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround2  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround2  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround2   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround2 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround2 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround2  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround2  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround2   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround2 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround2 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround2  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround2  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround2   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround2 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround2 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround2  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi

.L41_60:
	subround3  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround3   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround3 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround3 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround3  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround3  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround3   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround3 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround3 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround3  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround3  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround3   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround3 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround3 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround3  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround3  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround3   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround3 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround3 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround3  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi

.L61_80:
	subround4  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround4   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround4 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround4 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround4  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround4  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround4   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround4 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround4 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround4  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround4  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround4   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround4 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround4 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround4  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	addl $20,%edi
	subround4  4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0
	subround4   (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4
	subround4 16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8
	subround4 12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12
	subround4  8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16
	# addl $20,%edi

	movl $4,%ecx

	.p2align 2
.L3:
	movl (%ebp,%ecx,4),%eax
	addl %eax,(%esi,%ecx,4)
	decl %ecx
	jns .L3

	popl %ebp
	popl %ebx
	popl %esi
	popl %edi
	ret
.endif
