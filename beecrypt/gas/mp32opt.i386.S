/*
 * mp32opt.i386.S
 *
 * Assembler optimized multiprecision integer routines for Intel 386 and higher
 *
 * Compile target is GNU Assembler
 *
 * Copyright (c) 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

	.file "mp32opt.i386.S"

	.text


C_FUNCTION_BEGIN(mp32zero)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi

	xorl %eax,%eax
	repz stosl

	popl %edi
	ret
C_FUNCTION_END(mp32zero, .Lmp32zero_size)


C_FUNCTION_BEGIN(mp32fill)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi
	movl 16(%esp),%eax

	repz stosl

	popl %edi
	ret
C_FUNCTION_END(mp32fill, .Lmp32fill_size)


#if 0	/* BROKEN! */
C_FUNCTION_BEGIN(mp32even)
	mov 4(%esp),%ecx
	mov 8(%esp),%eax
	mov 4(%esp,%ecx,4),%eax
	not %eax
	and %eax,1
	ret
C_FUNCTION_END(mp32even, .Lmp32even_size)


C_FUNCTION_BEGIN(mp32odd)
	mov 4(%esp),%ecx
	mov 8(%esp),%eax
	mov 4(%esp,%ecx,4),%eax
	and %eax,1
	ret
C_FUNCTION_END(mp32odd, .Lmp32odd_size)
#endif


C_FUNCTION_BEGIN(mp32addw)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi
	movl 16(%esp),%eax

	xorl %edx,%edx
	leal -4(%edi,%ecx,4),%edi
	addl %eax,(%edi)
	decl %ecx
	jz .Lmp32addw_skip
	leal -4(%edi),%edi

	.align 4
.Lmp32addw_loop:
	adcl %edx,(%edi)
	leal -4(%edi),%edi
	decl %ecx
	jnz .Lmp32addw_loop
.Lmp32addw_skip:
	sbbl %eax,%eax
	negl %eax

	popl %edi
	ret
C_FUNCTION_END(mp32addw, .Lmp32addw_size)


C_FUNCTION_BEGIN(mp32subw)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi
	movl 16(%esp),%eax

	xorl %edx,%edx
	leal -4(%edi,%ecx,4),%edi
	subl %eax,(%edi)
	decl %ecx
	jz .Lmp32subw_skip
	leal -4(%edi),%edi

	.align 4
.Lmp32subw_loop:
	sbbl %edx,(%edi)
	leal -4(%edi),%edi
	decl %ecx
	jnz .Lmp32subw_loop
.Lmp32subw_skip:
	sbbl %eax,%eax
	negl %eax
	popl %edi
	ret
C_FUNCTION_END(mp32subw, .Lmp32subw_size)


C_FUNCTION_BEGIN(mp32add)
	pushl %edi
	pushl %esi

	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi

	xorl %edx,%edx
	decl %ecx

	.align 4
.Lmp32add_loop:
	movl (%esi,%ecx,4),%eax
	adcl %eax,(%edi,%ecx,4)
	decl %ecx
	jns .Lmp32add_loop

	sbbl %eax,%eax
	negl %eax

	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mp32add, .Lmp32add_size)


C_FUNCTION_BEGIN(mp32sub)
	pushl %edi
	pushl %esi

	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi

	xorl %edx,%edx
	decl %ecx

	.align 4
.Lmp32sub_loop:
	movl (%esi,%ecx,4),%eax
	sbbl %eax,(%edi,%ecx,4)
	decl %ecx
	jns .Lmp32sub_loop

	sbbl %eax,%eax
	negl %eax
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mp32sub, .Lmp32sub_size)


C_FUNCTION_BEGIN(mp32divtwo)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi

	leal (%edi,%ecx,4),%edi
	negl %ecx
	clc

	.align 4
.Lmp32divtwo_loop:
	rcrl $1,(%edi,%ecx,4)
	inc %ecx
	jnz .Lmp32divtwo_loop

	popl %edi
	ret
C_FUNCTION_END(mp32divtwo, .Lmp32divtwo_size)


C_FUNCTION_BEGIN(mp32multwo)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi

	clc
	decl %ecx

	.align 4
.Lmp32multwo_loop:
	rcll $1,(%edi,%ecx,4)
	decl %ecx 
	jns .Lmp32multwo_loop

	sbbl %eax,%eax
	negl %eax

	popl %edi
	ret
C_FUNCTION_END(mp32multwo, .Lmp32multwo_size)


C_FUNCTION_BEGIN(mp32setmul)
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %ebp

	movl 20(%esp),%ecx
	movl 24(%esp),%edi
	movl 28(%esp),%esi
	movl 32(%esp),%ebp

	xorl %edx,%edx
	decl %ecx

	.align 4
.Lmp32setmul_loop:
	movl %edx,%ebx
	movl (%esi,%ecx,4),%eax
	mull %ebp
	addl %ebx,%eax
	adcl $0,%edx
	movl %eax,(%edi,%ecx,4)
	decl %ecx
	jns .Lmp32setmul_loop

	movl %edx,%eax

	popl %ebp
	popl %ebx
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mp32setmul, .Lmp32setmul_size)


C_FUNCTION_BEGIN(mp32addmul)
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %ebp

	movl 20(%esp),%ecx
	movl 24(%esp),%edi
	movl 28(%esp),%esi
	movl 32(%esp),%ebp

	xorl %edx,%edx
	decl %ecx

	.align 4
.Lmp32addmul_loop:
	movl %edx,%ebx
	movl (%esi,%ecx,4),%eax
	mull %ebp
	addl %ebx,%eax
	adcl $0,%edx
	addl (%edi,%ecx,4),%eax
	adcl $0,%edx
	movl %eax,(%edi,%ecx,4)
	decl %ecx
	jns .Lmp32addmul_loop

	movl %edx,%eax

	popl %ebp
	popl %ebx
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mp32addmul, .Lmp32addmul_size)


C_FUNCTION_BEGIN(mp32addsqrtrc)
	pushl %edi
	pushl %esi
	pushl %ebx

	movl 16(%esp),%ecx
	movl 20(%esp),%edi
	movl 24(%esp),%esi

	xorl %ebx,%ebx
	decl %ecx

	.align 4
.Lmp32addsqrtrc_loop:
	movl (%esi,%ecx,4),%eax
	mull %eax
	addl %ebx,%eax
	adcl $0,%edx
	addl %eax,4(%edi,%ecx,8)
	adcl %edx,(%edi,%ecx,8)
	sbbl %ebx,%ebx
	negl %ebx
	decl %ecx
	jns .Lmp32addsqrtrc_loop

	movl %ebx,%eax

	popl %ebx
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mp32addsqrtrc, .Lmp32addsqrtrc_size)
