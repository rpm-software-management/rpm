/*
 * mp32opt.sparcv8.S
 *
 * Assembler optimized multiprecision integer routines for Sparc v8
 *
 * Compile target is GNU Assembler, Sun Solaris Assembler
 *
 * Copyright (c) 2001 Virtual Unlimited B.V.
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

	.file "mp32opt.sparcv8.S"

	.text

C_FUNCTION_BEGIN(mpsetmul)
LABEL(mpsetmul)

	.register %g2,#scratch

	sll %o0,2,%g1
	dec 4,%o2
	clr %o0
LOCAL(mpsetmul_loop):
	ld [%o2+%g1],%g2
	umul %o3,%g2,%g2
	rd %y,%g3
	addcc %o0,%g2,%g2
	addx %g0,%g3,%o0
	deccc 4,%g1
	bnz LOCAL(mpsetmul_loop)
	st %g2,[%o1+%g1]
	retl
	nop
C_FUNCTION_END(mpsetmul, LOCAL(mpsetmul_size))


C_FUNCTION_BEGIN(mpaddmul)
LABEL(mpaddmul)

	.register %g2,#scratch

	sll %o0,2,%g1
	mov %o1,%o4
	dec 4,%o1
	dec 4,%o2
	clr %o0
LOCAL(mpaddmul_loop):
	ld [%o2+%g1],%g2
	ld [%o1+%g1],%g3
	umul %o3,%g2,%g2
	rd %y,%g4
	addcc %o0,%g2,%g2
	addx %g0,%g4,%g4
	addcc %g2,%g3,%g2
	addx %g0,%g4,%o0
	deccc 4,%g1
	bnz LOCAL(mpaddmul_loop)
	st %g2,[%o4+%g1]
	retl
	nop
C_FUNCTION_END(mpaddmul, LOCAL(mpaddmul_size))


C_FUNCTION_BEGIN(mpaddsqrtrc)
LABEL(mpaddsqrtrc)

        .register %g2,#scratch
        .register %g3,#scratch

        sll %o0,2,%g1
        add %o1,%g1,%o1
		dec 4,%o2
        add %o1,%g1,%o1
		dec 8,%o1
        clr %o0
LOCAL(mpaddsqrtrc_loop):
        ld [%o2+%g1],%g2
        ldd [%o1],%o4
        umul %g2,%g2,%g3
        rd %y,%g2
		/* first addition */
        addcc %o5,%g3,%o5
        addxcc %o4,%g2,%o4
        addx %g0,%g0,%o3
		/* second addition */
        addcc %o5,%o0,%o5
        addxcc %o4,%g0,%o4
        addx %o3,%g0,%o0
        std %o4,[%o1]
        deccc 4,%g1
        bnz LOCAL(mpaddsqrtrc_loop)
        sub %o1,8,%o1
        retl
        nop
C_FUNCTION_END(mpaddsqrtrc, LOCAL(mpaddsqrtrc_size))
