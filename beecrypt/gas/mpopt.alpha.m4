dnl  mpopt.alpha.m4
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
include(ASM_SRCDIR/alpha.m4)


C_FUNCTION_BEGIN(mpadd)
	subq `$'16,1,`$'16
	s8addq `$'16,0,`$'1
	addq `$'17,`$'1,`$'17
	addq `$'18,`$'1,`$'18
	mov `$31',`$'0

	.align 4
LOCAL(mpadd_loop):
	ldq `$'1,0(`$'17)
	ldq `$'2,0(`$'18)
	addq `$'1,`$'0,`$'3
	cmpult `$'3,`$'1,`$'0
	addq `$3',`$'2,`$'1
	cmpult `$'1,`$'3,`$'2
	stq `$'1,0(`$'17)
	or `$'2,`$'0,`$'0
	subq `$'16,1,`$'16
	subq `$'17,8,`$'17
	subq `$'18,8,`$'18
	bge `$'16,LOCAL(mpadd_loop)
	ret `$'31,(`$'26),1
C_FUNCTION_END(mpadd)


C_FUNCTION_BEGIN(mpsub)
	subq `$'16,1,`$'16
	s8addq `$'16,0,`$'1
	addq `$'17,`$'1,`$'17
	addq `$'18,`$'1,`$'18
	mov `$31',`$'0

	.align 4
LOCAL(mpsub_loop):
	ldq `$'1,0(`$'17)
	ldq `$'2,0(`$'18)
	subq `$'1,`$'0,`$'3
	cmpult `$'1,`$'3,`$'0
	subq `$'3,`$'2,`$'1
	cmpult `$'3,`$'1,`$'2
	stq `$'1,0(`$'17)
	or `$'2,`$'0,`$'0
	subq `$'16,1,`$'16
	subq `$'17,8,`$'17
	subq `$'18,8,`$'18
	bge `$'16,LOCAL(mpsub_loop)
	ret `$'31,(`$'26),1
C_FUNCTION_END(mpsub)


C_FUNCTION_BEGIN(mpsetmul)
	subq `$'16,1,`$'16
	s8addq `$'16,0,`$'1
	addq `$'17,`$'1,`$'17
	addq `$'18,`$'1,`$'18
	mov `$31',`$'0

	.align 4
LOCAL(mpsetmul_loop):
	ldq `$1',0(`$'18)
	mulq `$'19,`$'1,`$'2
	umulh `$'19,`$'1,`$'3
	addq `$'2,`$'0,`$'2
	cmpult `$'2,`$'0,`$'0
	stq `$'2,0(`$'17)
	addq `$'3,`$'0,`$'0
	subq `$'16,1,`$'16
	subq `$'17,8,`$'17
	subq `$'18,8,`$'18
	bge `$'16,LOCAL(mpsetmul_loop)
	ret `$'31,(`$'26),1
C_FUNCTION_END(mpsetmul)


C_FUNCTION_BEGIN(mpaddmul)
	subq `$'16,1,`$'16
	s8addq `$'16,0,`$'1
	addq `$'17,`$'1,`$'17
	addq `$'18,`$'1,`$'18
	mov `$31',`$'0

	.align 4
LOCAL(mpaddmul_loop):
	ldq `$'1,0(`$'17)
	ldq `$'2,0(`$'18)
	mulq `$'19,`$'2,`$'3
	umulh `$'19,`$'2,`$'4
	addq `$'3,`$'0,`$'3
	cmpult `$'3,`$'0,`$'0
	addq `$'4,`$'0,`$'4
	addq `$'3,`$'1,`$'3
	cmpult `$'3,`$'1,`$'0
	addq `$'4,`$'0,`$'0
	stq `$'3,0(`$'17)
	subq `$'16,1,`$'16
	subq `$'17,8,`$'17
	subq `$'18,8,`$'18
	bge `$'16,LOCAL(mpaddmul_loop)
	ret `$'31,(`$'26),1
C_FUNCTION_END(mpaddmul)


C_FUNCTION_BEGIN(mpaddsqrtrc)
	subq `$'16,1,`$'16
	s8addq `$'16,0,`$'1
	addq `$'17,`$'1,`$'17
	addq `$'17,`$'1,`$'17
	addq `$'18,`$'1,`$'18
	mov `$31',`$'0

	.align 4
LOCAL(mpaddsqrtrc_loop):
	ldq `$'1,0(`$'18)
	mulq `$1',`$1',`$'2
	umulh `$1',`$1',`$'1
	addq `$'2,`$'0,`$'3
	cmpult `$3',`$'2,`$'0
	ldq `$'2,8(`$'17)
	addq `$'1,`$'0,`$'1
	addq `$'3,`$'2,`$'4
	cmpult `$'4,`$'3,`$'0
	ldq `$'3,0(`$'17)
	addq `$'1,`$'0,`$'2
	cmpult `$2',`$'1,`$'0
	stq `$'4,8(`$'17)
	addq `$'2,`$'3,`$'1
	cmpult `$'1,`$'2,`$2'
	stq `$'1,0(`$'17)
	addq `$'2,`$'0,`$'0
	subq `$'16,1,`$'16
	subq `$'17,16,`$'17
	subq `$'18,8,`$'18
	bge `$'16,LOCAL(mpaddmul_loop)
	ret `$'31,(`$'26),1
C_FUNCTION_END(mpaddsqrtrc)
