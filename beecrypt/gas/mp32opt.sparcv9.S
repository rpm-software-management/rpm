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

#include "config.gas.h"

	.file "mp32opt.sparcv9.S"

	.text

C_FUNCTION_BEGIN(mp32addw)

	.register %g2,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	clr %o0
	lduw [%o1+%g1],%g2
	addcc %g2,%o2,%g2
	stw %g2,[%o1+%g1]
	brz,pn %g1,.Lmp32addw_skip
	dec 4,%g1
.Lmp32addw_loop:
	lduw [%o1+%g1],%g2
	addccc %g2,%g0,%g2
	stw %g2,[%o1+%g1]
	brnz,pt %g1,.Lmp32addw_loop
	dec 4,%g1
.Lmp32addw_skip:
	retl
	movcs %icc,1,%o0
.Lend_addw:
C_FUNCTION_END(mp32addw, .Lmp32addw_size)


C_FUNCTION_BEGIN(mp32subw)

	.register %g2,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	clr %o0
	lduw [%o1+%g1],%g2
	subcc %g2,%o2,%g2
	stw %g2,[%o1+%g1]
	brz,pn %g1,.Lmp32subw_skip
	dec 4,%g1
.Lmp32subw_loop:
	lduw [%o1+%g1],%g2
	subccc %g2,%g0,%g2
	stw %g2,[%o1+%g1]
	brnz,pt %g1,.Lmp32subw_loop
	dec 4,%g1
.Lmp32subw_skip:
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mp32subw, .Lmp32subw_size)


C_FUNCTION_BEGIN(mp32add)

	.register %g2,#scratch
	.register %g3,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	addcc %g0,%g0,%o0
.Lmp32add_loop:
	lduw [%o1+%g1],%g2
	lduw [%o2+%g1],%g3
	addccc %g2,%g3,%g4
	stw %g4,[%o1+%g1]
	brnz,pt %g1,.Lmp32add_loop
	dec 4,%g1
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mp32add, .Lmp32add_size)


C_FUNCTION_BEGIN(mp32sub)

	.register %g2,#scratch
	.register %g3,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	addcc %g0,%g0,%o0
.Lmp32sub_loop:
	lduw [%o1+%g1],%g2
	lduw [%o2+%g1],%g3
	subccc %g2,%g3,%g4
	stw %g4,[%o1+%g1]
	brnz,pt %g1,.Lmp32sub_loop
	dec 4,%g1
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mp32sub, .Lmp32sub_size)


C_FUNCTION_BEGIN(mp32multwo)

	.register %g2,#scratch
	.register %g3,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	addcc %g0,%g0,%o0
.Lmp32multwo_loop:
	lduw [%o1+%g1],%g2
	addccc %g2,%g2,%g3
	stw %g3,[%o1+%g1]
	brnz,pt %g1,.Lmp32multwo_loop
	dec 4,%g1
	retl
	movcs %icc,1,%o0
C_FUNCTION_END(mp32multwo, .Lmp32multwo_size)


C_FUNCTION_BEGIN(mp32setmul)

	.register %g2,#scratch
	.register %g3,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	clr %o0
.Lmp32setmul_loop:
	lduw [%o2+%g1],%g2
	srlx %o0,32,%o0
	mulx %o3,%g2,%g3
	add %o0,%g3,%o0
	stw %o0,[%o1+%g1]
	brnz,pt %g1,.Lmp32setmul_loop
	dec 4,%g1
	retl
	srlx %o0,32,%o0
C_FUNCTION_END(mp32setmul, .Lmp32setmul_size)


C_FUNCTION_BEGIN(mp32addmul)

	.register %g2,#scratch
	.register %g3,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	clr %o0
.Lmp32addmul_loop:
	lduw [%o2+%g1],%g2
	lduw [%o1+%g1],%g4
	srlx %o0,32,%o0
	mulx %o3,%g2,%g3
	add %o0,%g3,%o0
	add %o0,%g4,%o0
	stw %o0,[%o1+%g1]
	brnz,pt %g1,.Lmp32addmul_loop
	dec 4,%g1
	retl
	srlx %o0,32,%o0
C_FUNCTION_END(mp32addmul, .Lmp32addmul_size)


C_FUNCTION_BEGIN(mp32addsqrtrc)

	.register %g2,#scratch
	.register %g3,#scratch

	sll %o0,2,%g1
	dec 4,%g1
	add %o1,%g1,%o1
	add %o1,%g1,%o1
	clr %o0
.Lmp32addsqrtrc_loop:
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
	brnz,pt %g1,.Lmp32addsqrtrc_loop
	dec 4,%g1
	retl
	nop
C_FUNCTION_END(mp32addsqrtrc, .Lmp32addsqrtrc_size)
