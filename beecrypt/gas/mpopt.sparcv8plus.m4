dnl  mpopt.sparcv8plus.m4
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
include(ASM_SRCDIR/asmdefs.m4)
include(ASM_SRCDIR/sparc.m4)


C_FUNCTION_BEGIN(mpaddw)
	sll %o0,2,%g1
	dec 4,%g1
	clr %o0
	lduw [%o1+%g1],%g2
	addcc %g2,%o2,%g2
	stw %g2,[%o1+%g1]
	brz,pn %g1,LOCAL(mpaddw_skip)
	dec 4,%g1
LOCAL(mpaddw_loop):
	lduw [%o1+%g1],%g2
	addccc %g2,%g0,%g2
	stw %g2,[%o1+%g1]
	brnz,pt %g1,LOCAL(mpaddw_loop)
	dec 4,%g1
LOCAL(mpaddw_skip):
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mpaddw)


C_FUNCTION_BEGIN(mpsubw)
	sll %o0,2,%g1
	dec 4,%g1
	clr %o0
	lduw [%o1+%g1],%g2
	subcc %g2,%o2,%g2
	stw %g2,[%o1+%g1]
	brz,pn %g1,LOCAL(mpsubw_skip)
	dec 4,%g1
LOCAL(mpsubw_loop):
	lduw [%o1+%g1],%g2
	subccc %g2,%g0,%g2
	stw %g2,[%o1+%g1]
	brnz,pt %g1,LOCAL(mpsubw_loop)
	dec 4,%g1
LOCAL(mpsubw_skip):
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mpsubw)


C_FUNCTION_BEGIN(mpadd)
	sll %o0,2,%g1
	dec 4,%g1
	addcc %g0,%g0,%o0
LOCAL(mpadd_loop):
	lduw [%o1+%g1],%g2
	lduw [%o2+%g1],%g3
	addccc %g2,%g3,%g4
	stw %g4,[%o1+%g1]
	brnz,pt %g1,LOCAL(mpadd_loop)
	dec 4,%g1
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mpadd)


C_FUNCTION_BEGIN(mpsub)
	sll %o0,2,%g1
	dec 4,%g1
	addcc %g0,%g0,%o0
LOCAL(mpsub_loop):
	lduw [%o1+%g1],%g2
	lduw [%o2+%g1],%g3
	subccc %g2,%g3,%g4
	stw %g4,[%o1+%g1]
	brnz,pt %g1,LOCAL(mpsub_loop)
	dec 4,%g1
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mpsub)


C_FUNCTION_BEGIN(mpmultwo)
	sll %o0,2,%g1
	dec 4,%g1
	addcc %g0,%g0,%o0
LOCAL(mpmultwo_loop):
	lduw [%o1+%g1],%g2
	addccc %g2,%g2,%g3
	stw %g3,[%o1+%g1]
	brnz,pt %g1,LOCAL(mpmultwo_loop)
	dec 4,%g1
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mpmultwo)


C_FUNCTION_BEGIN(mpsetmul)
	sll %o0,2,%g1
	dec 4,%g1
	clr %o0
LOCAL(mpsetmul_loop):
	lduw [%o2+%g1],%g2
	srlx %o0,32,%o0
	mulx %o3,%g2,%g3
	add %o0,%g3,%o0
	stw %o0,[%o1+%g1]
	brnz,pt %g1,LOCAL(mpsetmul_loop)
	dec 4,%g1
	retl
	srlx %o0,32,%o0
C_FUNCTION_END(mpsetmul)


C_FUNCTION_BEGIN(mpaddmul)
	sll %o0,2,%g1
	dec 4,%g1
	clr %o0
LOCAL(mpaddmul_loop):
	lduw [%o2+%g1],%g2
	lduw [%o1+%g1],%g4
	srlx %o0,32,%o0
	mulx %o3,%g2,%g3
	add %o0,%g3,%o0
	add %o0,%g4,%o0
	stw %o0,[%o1+%g1]
	brnz,pt %g1,LOCAL(mpaddmul_loop)
	dec 4,%g1
	retl
	srlx %o0,32,%o0
C_FUNCTION_END(mpaddmul)


C_FUNCTION_BEGIN(mpaddsqrtrc)
	sll %o0,2,%g1
	dec 4,%g1
	add %o1,%g1,%o1
	add %o1,%g1,%o1
	clr %o0
LOCAL(mpaddsqrtrc_loop):
	lduw [%o2+%g1],%g2
	ldx [%o1],%g4
	mulx %g2,%g2,%g2
	add %o0,%g4,%g3
	clr %o0
	add %g3,%g2,%g3
	cmp %g4,%g3
	movgu %xcc,1,%o0
	stx %g3,[%o1]
	sub %o1,8,%o1
	brnz,pt %g1,LOCAL(mpaddsqrtrc_loop)
	dec 4,%g1
	retl
	nop
C_FUNCTION_END(mpaddsqrtrc)
