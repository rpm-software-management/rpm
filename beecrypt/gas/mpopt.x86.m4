dnl  mpopt.x86.m4
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
 
include(config.m4)
include(ASM_SRCDIR/x86.m4)


C_FUNCTION_BEGIN(mpzero)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi

	xorl %eax,%eax
	repz stosl

	popl %edi
	ret
C_FUNCTION_END(mpzero)


C_FUNCTION_BEGIN(mpfill)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi
	movl 16(%esp),%eax

	repz stosl

	popl %edi
	ret
C_FUNCTION_END(mpfill)


C_FUNCTION_BEGIN(mpeven)
	movl 4(%esp),%ecx
	movl 8(%esp),%eax
	movl -4(%eax,%ecx,4),%eax
	notl %eax
	andl `$'1,%eax
	ret
C_FUNCTION_END(mpeven)


C_FUNCTION_BEGIN(mpodd)
	movl 4(%esp),%ecx
	movl 8(%esp),%eax
	movl -4(%eax,%ecx,4),%eax
	andl `$'1,%eax
	ret
C_FUNCTION_END(mpodd)


C_FUNCTION_BEGIN(mpaddw)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi
	movl 16(%esp),%eax

	xorl %edx,%edx
	leal -4(%edi,%ecx,4),%edi
	addl %eax,(%edi)
	decl %ecx
	jz LOCAL(mpaddw_skip)
	leal -4(%edi),%edi

	.align 4
LOCAL(mpaddw_loop):
	adcl %edx,(%edi)
	leal -4(%edi),%edi
	decl %ecx
	jnz LOCAL(mpaddw_loop)
LOCAL(mpaddw_skip):
	sbbl %eax,%eax
	negl %eax

	popl %edi
	ret
C_FUNCTION_END(mpaddw)


C_FUNCTION_BEGIN(mpsubw)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi
	movl 16(%esp),%eax

	xorl %edx,%edx
	leal -4(%edi,%ecx,4),%edi
	subl %eax,(%edi)
	decl %ecx
	jz LOCAL(mpsubw_skip)
	leal -4(%edi),%edi

	.align 4
LOCAL(mpsubw_loop):
	sbbl %edx,(%edi)
	leal -4(%edi),%edi
	decl %ecx
	jnz LOCAL(mpsubw_loop)
LOCAL(mpsubw_skip):
	sbbl %eax,%eax
	negl %eax
	popl %edi
	ret
C_FUNCTION_END(mpsubw)


C_FUNCTION_BEGIN(mpadd)
	pushl %edi
	pushl %esi

	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi

	xorl %edx,%edx
	decl %ecx

	.align 4
LOCAL(mpadd_loop):
	movl (%esi,%ecx,4),%eax
	movl (%edi,%ecx,4),%edx
	adcl %eax,%edx
	movl %edx,(%edi,%ecx,4)
	decl %ecx
	jns LOCAL(mpadd_loop)

	sbbl %eax,%eax
	negl %eax

	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mpadd)


C_FUNCTION_BEGIN(mpsub)
	pushl %edi
	pushl %esi

	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi

	xorl %edx,%edx
	decl %ecx

	.align 4
LOCAL(mpsub_loop):
	movl (%esi,%ecx,4),%eax
	movl (%edi,%ecx,4),%edx
	sbbl %eax,%edx
	movl %edx,(%edi,%ecx,4)
	decl %ecx
	jns LOCAL(mpsub_loop)

	sbbl %eax,%eax
	negl %eax
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mpsub)


C_FUNCTION_BEGIN(mpdivtwo)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi

	leal (%edi,%ecx,4),%edi
	negl %ecx
	xorl %eax,%eax

	.align 4
LOCAL(mpdivtwo_loop):
	rcrl `$'1,(%edi,%ecx,4)
	inc %ecx
	jnz LOCAL(mpdivtwo_loop)

	popl %edi
	ret
C_FUNCTION_END(mpdivtwo)


C_FUNCTION_BEGIN(mpmultwo)
	pushl %edi

	movl 8(%esp),%ecx
	movl 12(%esp),%edi

	xorl %edx,%edx
	decl %ecx

	.align 4
LOCAL(mpmultwo_loop):
	movl (%edi,%ecx,4),%eax
	adcl %eax,%eax
	movl %eax,(%edi,%ecx,4)
	decl %ecx 
	jns LOCAL(mpmultwo_loop)

	sbbl %eax,%eax
	negl %eax

	popl %edi
	ret
C_FUNCTION_END(mpmultwo)


C_FUNCTION_BEGIN(mpsetmul)
	pushl %edi
	pushl %esi
ifdef(`USE_SSE2',`
	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi
	movd 24(%esp),%mm1

	pxor %mm0,%mm0
	decl %ecx

	.align 4
LOCAL(mpsetmul_loop):
	movd (%esi,%ecx,4),%mm2
	pmuludq %mm1,%mm2
	paddq %mm2,%mm0
	movd %mm0,(%edi,%ecx,4)
	decl %ecx
	psrlq `$'32,%mm0
	jns LOCAL(mpsetmul_loop)

	movd %mm0,%eax
	emms
',`
	pushl %ebx
	pushl %ebp

	movl 20(%esp),%ecx
	movl 24(%esp),%edi
	movl 28(%esp),%esi
	movl 32(%esp),%ebp

	xorl %edx,%edx
	decl %ecx

	.align 4
LOCAL(mpsetmul_loop):
	movl %edx,%ebx
	movl (%esi,%ecx,4),%eax
	mull %ebp
	addl %ebx,%eax
	adcl `$'0,%edx
	movl %eax,(%edi,%ecx,4)
	decl %ecx
	jns LOCAL(mpsetmul_loop)

	movl %edx,%eax

	popl %ebp
	popl %ebx
')
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mpsetmul)


C_FUNCTION_BEGIN(mpaddmul)
	pushl %edi
	pushl %esi
ifdef(`USE_SSE2',`
	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi
	movd 24(%esp),%mm1

	pxor %mm0,%mm0
	decl %ecx

	.align 4
LOCAL(mpaddmul_loop):
	movd (%esi,%ecx,4),%mm2
	movd (%edi,%ecx,4),%mm3
	pmuludq %mm1,%mm2
	paddq %mm2,%mm3
	paddq %mm3,%mm0
	movd %mm0,(%edi,%ecx,4)
	decl %ecx
	psrlq $32,%mm0
	jns LOCAL(mpaddmul_loop)

	movd %mm0,%eax
	emms
',`
	pushl %ebx
	pushl %ebp

	movl 20(%esp),%ecx
	movl 24(%esp),%edi
	movl 28(%esp),%esi
	movl 32(%esp),%ebp

	xorl %edx,%edx
	decl %ecx

	.align 4
LOCAL(mpaddmul_loop):
	movl %edx,%ebx
	movl (%esi,%ecx,4),%eax
	mull %ebp
	addl %ebx,%eax
	adcl $0,%edx
	addl (%edi,%ecx,4),%eax
	adcl $0,%edx
	movl %eax,(%edi,%ecx,4)
	decl %ecx
	jns LOCAL(mpaddmul_loop)

	movl %edx,%eax

	popl %ebp
	popl %ebx
')
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mpaddmul)


C_FUNCTION_BEGIN(mpaddsqrtrc)
	pushl %edi
	pushl %esi
ifdef(`USE_SSE2',`
	movl 12(%esp),%ecx
	movl 16(%esp),%edi
	movl 20(%esp),%esi

	pxor %mm0,%mm0
	decl %ecx

	.align 4
LOCAL(mpaddsqrtrc_loop):
	movd (%esi,%ecx,4),%mm2
	pmuludq %mm2,%mm2
	movd 4(%edi,%ecx,8),%mm3
	paddq %mm2,%mm3
	movd 0(%edi,%ecx,8),%mm4
	paddq %mm3,%mm0
	movd %mm0,4(%edi,%ecx,8)
	psrlq $32,%mm0
	paddq %mm4,%mm0
	movd %mm0,0(%edi,%ecx,8)
	decl %ecx
	psrlq $32,%mm0
	jns LOCAL(mpaddsqrtrc_loop)

	movd %mm0,%eax
	emms
',`
	pushl %ebx

	movl 16(%esp),%ecx
	movl 20(%esp),%edi
	movl 24(%esp),%esi

	xorl %ebx,%ebx
	decl %ecx

	.align 4
LOCAL(mpaddsqrtrc_loop):
	movl (%esi,%ecx,4),%eax
	mull %eax
	addl %ebx,%eax
	adcl $0,%edx
	addl %eax,4(%edi,%ecx,8)
	adcl %edx,(%edi,%ecx,8)
	sbbl %ebx,%ebx
	negl %ebx
	decl %ecx
	jns LOCAL(mpaddsqrtrc_loop)

	movl %ebx,%eax

	popl %ebx
')
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(mpaddsqrtrc)
