/*
 * mp32opt.sparcv9.S
 *
 * Assembler optimized multiprecision integer routines for UltraSparc (64 bits instructions, will run on 32 bit OS)
 *
 * Compile target is GNU Assembler, Sun Solaris Assembler
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

#include "beecrypt.gas.h"

	.file "mp32opt.sparcv9.S"

	.text

C_FUNCTION_BEGIN(mpaddw)
LABEL(mpaddw)

	.register %g2,#scratch

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
C_FUNCTION_END(mpaddw, LOCAL(mpaddw_size))


C_FUNCTION_BEGIN(mpsubw)
LABEL(mpsubw)

	.register %g2,#scratch

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
C_FUNCTION_END(mpsubw, LOCAL(mpsubw_size))


C_FUNCTION_BEGIN(mpadd)
LABEL(mpadd)

	.register %g2,#scratch
	.register %g3,#scratch

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
C_FUNCTION_END(mpadd, LOCAL(mpadd_size))


C_FUNCTION_BEGIN(mpsub)
LABEL(mpsub)

	.register %g2,#scratch
	.register %g3,#scratch

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
C_FUNCTION_END(mpsub, LOCAL(mpsub_size))


C_FUNCTION_BEGIN(mpmultwo)
LABEL(mpmultwo)

	.register %g2,#scratch
	.register %g3,#scratch

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
C_FUNCTION_END(mpmultwo, LOCAL(mpmultwo_size))


C_FUNCTION_BEGIN(mpsetmul)
LABEL(mpsetmul)

	.register %g2,#scratch
	.register %g3,#scratch

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
C_FUNCTION_END(mpsetmul, LOCAL(mpsetmul_size))


C_FUNCTION_BEGIN(mpaddmul)
LABEL(mpaddmul)

	.register %g2,#scratch
	.register %g3,#scratch

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
C_FUNCTION_END(mpaddmul, LOCAL(mpaddmul_size))


C_FUNCTION_BEGIN(mpaddsqrtrc)
LABEL(mpaddsqrtrc)

	.register %g2,#scratch
	.register %g3,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	add %o1,%g1,%o1
	add %o1,%g1,%o1
	clr %o0
LOCAL(mpaddsqrtrc_loop):
	/* load from o1 into g4 as xuint; simulate xuint carry by doing an xuint comparison; carry if result smaller than initial value */
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
C_FUNCTION_END(mpaddsqrtrc, LOCAL(mpaddsqrtrc_size))
