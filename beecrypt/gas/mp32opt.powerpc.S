/*
 * mp32opt.powerpc.S
 *
 * Assembler optimized multiprecision integer routines for PowerPC
 *
 * Compile target is GNU Assembler
 *
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
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

	.file "mp32opt.powerpc.S"

	.text


C_FUNCTION_BEGIN(mp32addw)
LABEL(mp32addw)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	li r0,0
	lwzu r6,-4(r4)
	addc r6,r6,r5
	stw r6,0(r4)
	bdz LOCAL(mp32addw_skip)
LOCAL(mp32addw_loop):
	lwzu r6,-4(r4)
	adde r6,r0,r6
	stw r6,0(r4)
	bdnz LOCAL(mp32addw_loop)
LOCAL(mp32addw_skip):
	/* return the carry */
	addze r3,r0
	blr
C_FUNCTION_END(mp32addw, LOCAL(mp32addw_size))


C_FUNCTION_BEGIN(mp32subw)
LABEL(mp32subw)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	li r0,0
	lwz r6,-4(r4)
	subfc r6,r5,r6
	stwu r6,-4(r4)
	bdz LOCAL(mp32subw_skip)
LOCAL(mp32subw_loop):
	lwz r6,-4(r4)
	subfe r6,r0,r6
	stwu r6, -4(r4)
	bdnz LOCAL(mp32subw_loop)
LOCAL(mp32subw_skip):
	/* return the carry */
	subfe r3,r0,r0
	neg r3,r3
	blr
C_FUNCTION_END(mp32subw, LOCAL(mp32subw_size))


C_FUNCTION_BEGIN(mp32add)
LABEL(mp32add)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	li r0,0
	lwz r6,-4(r4)
	lwzu r7,-4(r5)
	addc r6,r7,r6
	stwu r6,-4(r4)
	bdz LOCAL(mp32add_skip)
LOCAL(mp32add_loop):
	lwz r6,-4(r4)
	lwzu r7,-4(r5)
	adde r6,r7,r6
	stwu r6,-4(r4)
	bdnz LOCAL(mp32add_loop)
LOCAL(mp32add_skip):
	/* return the carry */
	addze r3,r0
	blr
C_FUNCTION_END(mp32add, LOCAL(mp32add_size))


C_FUNCTION_BEGIN(mp32sub)
LABEL(mp32sub)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	li r0,0
	lwz r6,-4(r4)
	lwzu r7,-4(r5)
	subfc r6,r7,r6
	stwu r6,-4(r4)
	bdz LOCAL(mp32sub_skip)
LOCAL(mp32sub_loop):
	lwz r6,-4(r4)
	lwzu r7,-4(r5)
	subfe r6,r7,r6
	stwu r6,-4(r4)
	bdnz LOCAL(mp32sub_loop)
LOCAL(mp32sub_skip):
	/* return the carry */
	subfe r3,r0,r0
	neg r3,r3
	blr
C_FUNCTION_END(mp32sub, LOCAL(mp32sub_size))


C_FUNCTION_BEGIN(mp32multwo)
LABEL(mp32multwo)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	li r0,0
	lwz r6,-4(r4)
	addc r6,r6,r6
	stwu r6,-4(r4)
	bdz LOCAL(mp32multwo_skip)
LOCAL(mp32multwo_loop):
	lwz r6,-4(r4)
	adde r6,r6,r6
	stwu r6,-4(r4)
	bdnz LOCAL(mp32multwo_loop)
LOCAL(mp32multwo_skip):
	/* return the carry */
	addze r3,r0
	blr
C_FUNCTION_END(mp32multwo, LOCAL(mp32multwo_size))


C_FUNCTION_BEGIN(mp32setmul)
LABEL(mp32setmul)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	li r3,0
LOCAL(mp32setmul_loop):
	lwzu r7,-4(r5)
	mullw r8,r7,r6
	addc r8,r8,r3
	mulhwu r9,r7,r6
	addze r3,r9
	stwu r8,-4(r4)
	bdnz LOCAL(mp32setmul_loop)
	blr
C_FUNCTION_END(mp32setmul, LOCAL(mp32setmul_size))


C_FUNCTION_BEGIN(mp32addmul)
LABEL(mp32addmul)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	li r3,0
LOCAL(mp32addmul_loop):
	lwzu r8,-4(r5)
	lwzu r7,-4(r4)
	mullw r9,r8,r6
	addc r9,r9,r3
	mulhwu r10,r8,r6
	addze r3,r10
	addc r9,r9,r7
	addze r3,r3
	stw r9,0(r4)
	bdnz LOCAL(mp32addmul_loop)
	blr
C_FUNCTION_END(mp32addmul, LOCAL(mp32addmul_size))


C_FUNCTION_BEGIN(mp32addsqrtrc)
LABEL(mp32addsqrtrc)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	add r4,r4,r0
	li r3,0
LOCAL(mp32addsqrtrc_loop):
	lwzu r0,-4(r5)
	lwz r6,-8(r4)
	lwz r7,-4(r4)
	mullw r9,r0,r0
	addc r9,r9,r3
	mulhwu r8,r0,r0
	addze r8,r8
	li r3,0
	addc r7,r7,r9
	adde r6,r6,r8
	addze r3,r3
	stw r7,-4(r4)
	stwu r6,-8(r4)
	bdnz LOCAL(mp32addsqrtrc_loop)
	blr
C_FUNCTION_END(mp32addsqrtrc, LOCAL(mp32addsqrtrc_size))
