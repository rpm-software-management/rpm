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

#include "config.gas.h"

	.file "mp32opt.sparcv8.S"

	.text

C_FUNCTION_BEGIN(mp32setmul)
	sll %o0,2,%g1
	dec 4,%o2
	clr %o0
.Lmp32setmul_loop:
	ld [%o2+%g1],%g2
	umul %o3,%g2,%g2
	rd %y,%g3
	addcc %o0,%g2,%g2
	addx %g0,%g3,%o0
	deccc 4,%g1
	bnz .Lmp32setmul_loop
	st %g2,[%o1+%g1]
	retl
	nop
C_FUNCTION_END(mp32setmul, .Lmp32setmul_size)


C_FUNCTION_BEGIN(mp32addmul)
	sll %o0,2,%g1
	mov %o1,%o4
	dec 4,%o1
	dec 4,%o2
	clr %o0
.Lmp32addmul_loop:
	ld [%o2+%g1],%g2
	ld [%o1+%g1],%g3
	umul %o3,%g2,%g2
	rd %y,%g4
	addcc %o0,%g2,%g2
	addx %g0,%g4,%g4
	addcc %g2,%g3,%g2
	addx %g0,%g4,%o0
	deccc 4,%g1
	bnz .Lmp32addmul_loop
	st %g2,[%o4+%g1]
	retl
	nop
C_FUNCTION_END(mp32addmul, .Lmp32addmul_size)


#if 0
C_FUNCTION_BEGIN(mp32addsqrtrc)
	sll %o0,2,%g1
	dec 4,%o1
	dec 4,%o2
	sub %o3,%o2,4
	clr %o0
.Lmp32addsqrtrc_loop:
	ld [%o2+%g1],%g2
	ld [%o1+%g1],%g3
	ld [%o3+%g1],%g4
	umul %g2,%g2,%g2
	rd %y,%o4
	addcc %g3,%g2,%g3
	addx %g4,%o4,%g4
	/* carry from first addition */
	addx %g0,%g0,%o4
C_FUNCTION_END(mp32addsqrtrc, .Lmp32addsqrtrc_size)
#endif
