dnl  mpopt.x86_64.m4
dnl
dnl  Copyright (c) 2003 Bob Deblier
dnl 
dnl  Author: Bob Deblier <bob.deblier@pandora.be>
dnl 
dnl  This library is free software; you can redistribute it and/or
dnl  modify it under the terms of the GNU Lesser General Public
dnl  License as published by the Free Software Foundation; either
dnl  version 2.1 of the License, or (at your option) any later version.
dnl 
dnl  This library is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl  Lesser General Public License for more details.
dnl 
dnl  You should have received a copy of the GNU Lesser General Public
dnl  License along with this library; if not, write to the Free Software
dnl  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
	.file "mpopt.s"

include(config.m4)
include(ASM_SRCDIR/x86_64.m4)


C_FUNCTION_BEGIN(mpzero)
	movq %rdi,%rcx
	movq %rsi,%rdi
	xorq %rax,%rax
	repz stosq
	ret
C_FUNCTION_END(mpzero)


C_FUNCTION_BEGIN(mpfill)
	movq %rdi,%rcx
	movq %rsi,%rdi
	movq %rdx,%rdi
	repz stosq
	ret
C_FUNCTION_END(mpfill)


C_FUNCTION_BEGIN(mpeven)
	movq -8(%rsi,%rdi,8),%rax
	notq %rax
	andq `$'1,%rax
	ret
C_FUNCTION_END(mpeven)


C_FUNCTION_BEGIN(mpodd)
	movq -8(%rsi,%rdi,8),%rax
	andq `$'1,%rax
	ret
C_FUNCTION_END(mpodd)


C_FUNCTION_BEGIN(mpsetmul)
	movq %rcx,%r8
	movq %rdi,%rcx
	movq %rdx,%rdi

	xorq %rdx,%rdx
	decq %rcx

	.align 4
LOCAL(mpsetmul_loop):
	movq %rdx,%r9
	movq (%rdi,%rcx,8),%rax
	mulq %r8
	addq %r9,%rax
	adcq `$'0,%rdx
	movq %rax,(%rsi,%rcx,8)
	decq %rcx
	jns LOCAL(mpsetmul_loop)

	movq %rdx,%rax

	ret
C_FUNCTION_END(mpsetmul)


C_FUNCTION_BEGIN(mpaddmul)
	movq %rcx,%r8
	movq %rdi,%rcx
	movq %rdx,%rdi

	xorq %rdx,%rdx
	decq %rcx

	.align 4
LOCAL(mpaddmul_loop):
	movq %rdx,%r9
	movq (%rdi,%rcx,8),%rax
	mulq %r8
	addq %r9,%rax
	adcq `$'0,%rdx
	addq (%rsi,%rcx,8),%rax
	adcq `$'0,%rdx
	movq %rax,(%rsi,%rcx,8)
	decq %rcx
	jns LOCAL(mpaddmul_loop)

	movq %rdx,%rax
	ret
C_FUNCTION_END(mpaddmul)
