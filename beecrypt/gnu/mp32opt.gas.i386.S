/*
 * mp32opt.gas.i386.S
 *
 * Assembler optimized multiprecision integer routines for Intel 386 and higher
 *
 * Compile target is GNU AS
 *
 * Copyright (c) 1998-2000 Virtual Unlimited B.V.
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

	.file "mp32opt.gas.i386.S"

	.text

	.align	4
#if defined(CYGWIN)
	.globl	_mp32addw
	.def	_mp32addw
	.scl	2
	.type	32
	.endef

_mp32addw:
#else
	.global	mp32addw
	.type	mp32addw,@function

mp32addw:
#endif
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi
	movl 16(%esp),%eax

	xorl %edx,%edx
	leal -4(%edi,%ecx,4),%edi
	addl %eax,(%edi)
	decl %ecx
	jz .L1
	leal -4(%edi),%edi

	.p2align 4,,7
.L0:
	adcl %edx,(%edi)
	leal -4(%edi),%edi
	decl %ecx
	jnz .L0
.L1:
	sbbl %eax,%eax
	negl %eax

	popl %edi
	ret

#if defined(CYGWIN)
	.globl	_mp32subw
	.def	_mp32subw
	.scl	2
	.type	32
	.endef

_mp32subw:
#else
	.global	mp32subw
	.type	mp32subw,@function

mp32subw:
#endif
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi
	movl 16(%esp),%eax

	xorl %edx,%edx
	leal -4(%edi,%ecx,4),%edi
	subl %eax,(%edi)
	decl %ecx
	jz .L3
	leal -4(%edi),%edi

	.p2align 4,,7
.L2:
	sbbl %edx,(%edi)
	leal -4(%edi),%edi
	decl %ecx
	jnz .L2
.L3:
	sbbl %eax,%eax
	negl %eax
	popl %edi
	ret

#if defined(CYGWIN)
	.globl	_mp32add
	.def	_mp32add
	.scl	2
	.type	32
	.endef

_mp32add:
#else
	.global	mp32add
	.type	mp32add,@function

mp32add:
#endif
	pushl %edi
	pushl %esi

	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi

	xorl %edx,%edx
	decl %ecx

	.p2align 4,,7
.L4:
	movl (%esi,%ecx,4),%eax
	adcl %eax,(%edi,%ecx,4)
	decl %ecx
	jns .L4

	sbbl %eax,%eax
	negl %eax

	popl %esi
	popl %edi
	ret

#if defined(CYGWIN)
	.globl	_mp32sub
	.def	_mp32sub
	.scl	2
	.type	32
	.endef

_mp32sub:
#else
	.global	mp32sub
	.type	mp32sub,@function

mp32sub:
#endif
	pushl %edi
	pushl %esi

	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi

	xorl %edx,%edx
	decl %ecx

	.p2align 4,,7
.L5:
	movl (%esi,%ecx,4),%eax
	sbbl %eax,(%edi,%ecx,4)
	decl %ecx
	jns .L5

	sbbl %eax,%eax
	negl %eax
	popl %esi
	popl %edi
	ret

#if defined(CYGWIN)
	.globl	_mp32multwo
	.def	_mp32multwo
	.scl	2
	.type	32
	.endef

_mp32multwo:
#else
	.global	mp32multwo
	.type	mp32multwo,@function

mp32multwo:
#endif
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi

	xorl %eax,%eax
	decl %ecx

	.p2align 4,,7
.L6:
	movl (%edi,%ecx,4),%eax
	adcl %eax,(%edi,%ecx,4)
	decl %ecx 
	jns .L6

	sbbl %eax,%eax
	negl %eax

	popl %edi
	ret

#if defined(CYGWIN)
	.globl	_mp32setmul
	.def	_mp32setmul
	.scl	2
	.type	32
	.endef

_mp32setmul:
#else
	.global	mp32setmul
	.type	mp32setmul,@function

mp32setmul:
#endif
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %ebp

	movl 20(%esp),%ecx
	movl 24(%esp),%edi
	movl 28(%esp),%esi
	movl 32(%esp),%ebp

	xorl %ebx,%ebx
	decl %ecx

	.p2align 4,,7
.L7:
	movl (%esi,%ecx,4),%eax
	mull %ebp
	addl %ebx,%eax
	adcl $0,%edx
	movl %eax,(%edi,%ecx,4)
	movl %edx,%ebx
	decl %ecx
	jns .L7

	movl %ebx,%eax

	popl %ebp
	popl %ebx
	popl %esi
	popl %edi
	ret

#if defined(CYGWIN)
	.globl	_mp32addmul
	.def	_mp32addmul
	.scl	2
	.type	32
	.endef

_mp32addmul:
#else
	.global	mp32addmul
	.type	mp32addmul,@function

mp32addmul:
#endif
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %ebp

	movl 20(%esp),%ecx
	movl 24(%esp),%edi
	movl 28(%esp),%esi
	movl 32(%esp),%ebp

	xorl %ebx,%ebx
	decl %ecx

	.p2align 4,,7
.L8:
	movl (%esi,%ecx,4),%eax
	mull %ebp
	addl %ebx,%eax
	adcl $0,%edx
	addl (%edi,%ecx,4),%eax
	adcl $0,%edx
	movl %eax,(%edi,%ecx,4)
	movl %edx,%ebx
	decl %ecx
	jns .L8

	movl %ebx,%eax

	popl %ebp
	popl %ebx
	popl %esi
	popl %edi
	ret

#if defined(CYGWIN)
	.globl	_mp32addsqrtrc
	.def	_mp32addsqrtrc
	.scl	2
	.type	32
	.endef

_mp32addsqrtrc:
#else
	.global	mp32addsqrtrc
	.type	mp32addsqrtrc,@function

mp32addsqrtrc:
#endif
	pushl %edi
	pushl %esi
	pushl %ebx

	movl 16(%esp),%ecx
	movl 20(%esp),%edi
	movl 24(%esp),%esi

	xorl %ebx,%ebx
	decl %ecx

	.p2align 4,,7
.L9:
	movl (%esi,%ecx,4),%eax
	mull %eax
	addl %ebx,%eax
	adcl $0,%edx
	addl 4(%edi,%ecx,8),%eax
	adcl (%edi,%ecx,8),%edx
	sbbl %ebx,%ebx
	movl %eax,4(%edi,%ecx,8)
	movl %edx,(%edi,%ecx,8)
	negl %ebx
	decl %ecx
	jns .L9

	movl %ebx,%eax

	popl %ebx
	popl %esi
	popl %edi
	ret
