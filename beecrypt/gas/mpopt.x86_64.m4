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
include(ASM_SRCDIR/asmdefs.m4)
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
	movq %rdx,%rax
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


C_FUNCTION_BEGIN(mpaddw)
	movq %rdx,%rax
	xorq %rdx,%rdx
	leaq -8(%rsi,%rdi,8),%rsi
	addq %rax,(%rsi)
	decq %rdi
	jz LOCAL(mpaddw_skip)
	leaq -8(%rsi),%rsi

	.align 4
LOCAL(mpaddw_loop):
	adcq %rdx,(%rsi)
	leaq -8(%rsi),%rsi
	decq %rdi
	jnz LOCAL(mpaddw_loop)
LOCAL(mpaddw_skip):
	sbbq %rax,%rax
	negq %rax
	ret
C_FUNCTION_END(mpaddw)


C_FUNCTION_BEGIN(mpsubw)
	movq %rdx,%rax
	xorq %rdx,%rdx
	leaq -8(%rsi,%rdi,8),%rsi
	subq %rax,(%rsi)
	decq %rdi
	jz LOCAL(mpsubw_skip)
	leaq -8(%rsi),%rsi

	.align 4
LOCAL(mpsubw_loop):
	sbbq %rdx,(%rsi)
	leaq -8(%rsi),%rsi
	decq %rdi
	jnz LOCAL(mpsubw_loop)
LOCAL(mpsubw_skip):
	sbbq %rax,%rax
	negq %rax
	ret
C_FUNCTION_END(mpsubw)


C_FUNCTION_BEGIN(mpadd)
	xorq %r8,%r8
	decq %rdi

	.align 4
LOCAL(mpadd_loop):
	movq (%rdx,%rdi,8),%rax
	movq (%rsi,%rdi,8),%r8
	adcq %rax,%r8
	movq %r8,(%rsi,%rdi,8)
	decq %rdi
	jns LOCAL(mpadd_loop)

	sbbq %rax,%rax
	negq %rax
	ret
C_FUNCTION_END(mpadd)


C_FUNCTION_BEGIN(mpsub)
	xorq %r8,%r8
	decq %rdi

	.align 4
LOCAL(mpsub_loop):
	movq (%rdx,%rdi,8),%rax
	movq (%rsi,%rdi,8),%r8
	sbbq %rax,%r8
	movq %r8,(%rsi,%rdi,8)
	decq %rdi
	jns LOCAL(mpsub_loop)

	sbbq %rax,%rax
	negq %rax
	ret
C_FUNCTION_END(mpsub)


C_FUNCTION_BEGIN(mpdivtwo)
	leaq (%rsi,%rdi,8),%rsi
	negq %rdi
	xorq %rax,%rax

	.align 4
LOCAL(mpdivtwo_loop):
	rcrq `$'1,(%rsi,%rdi,8)
	inc %rdi
	jnz LOCAL(mpdivtwo_loop)

	ret
C_FUNCTION_END(mpdivtwo)


C_FUNCTION_BEGIN(mpmultwo)
	xorq %rdx,%rdx
	decq %rdi

	.align 4
LOCAL(mpmultwo_loop):
	movq (%rsi,%rdi,8),%rax
	adcq %rax,%rax
	movq %rax,(%rsi,%rdi,8)
	decq %rdi
	jns LOCAL(mpmultwo_loop)

	sbbq %rax,%rax
	negq %rax
	ret
C_FUNCTION_END(mpmultwo)


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


C_FUNCTION_BEGIN(mpaddsqrtrc)
	movq %rdi,%rcx
	movq %rsi,%rdi
	movq %rdx,%rsi

	xorq %r8,%r8
	decq %rcx

	leaq (%rdi,%rcx,8),%rdi 
	leaq (%rdi,%rcx,8),%rdi

	.align 4
LOCAL(mpaddsqrtrc_loop):
	movq (%rsi,%rcx,8),%rax
	mulq %rax
	addq %r8,%rax
	adcq `$'0,%rdx
	addq %rax,8(%rdi)
	adcq %rdx,0(%rdi)
	sbbq %r8,%r8
	negq %r8
	subq `$'16,%rdi
	decq %rcx
	jns LOCAL(mpaddsqrtrc_loop)

	movq %r8,%rax
	ret
C_FUNCTION_END(mpaddsqrtrc)
