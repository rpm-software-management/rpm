dnl  mpopt.ppc64.m4
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
include(ASM_SRCDIR/ppc64.m4)


C_FUNCTION_BEGIN(mpaddw)
	mtctr r3
	sldi r0,r3,3
	add r4,r4,r0
	li r0,0
	ldu r6,-8(r4)
	addc r6,r6,r5
	std r6,0(r4)
	bdz LOCAL(mpaddw_skip)
LOCAL(mpaddw_loop):
	ldu r6,-8(r4)
	adde r6,r0,r6
	std r6,0(r4)
	bdnz LOCAL(mpaddw_loop)
LOCAL(mpaddw_skip):
	addze r3,r0
	blr
C_FUNCTION_END(mpaddw)


C_FUNCTION_BEGIN(mpsubw)
	mtctr r3
	sldi r0,r3,3
	add r4,r4,r0
	li r0,0
	ld r6,-8(r4)
	subfc r6,r5,r6
	stdu r6,-8(r4)
	bdz LOCAL(mpsubw_skip)
LOCAL(mpsubw_loop):
	ld r6,-8(r4)
	subfe r6,r0,r6
	stdu r6, -8(r4)
	bdnz LOCAL(mpsubw_loop)
LOCAL(mpsubw_skip):
	subfe r3,r0,r0
	neg r3,r3
	blr
C_FUNCTION_END(mpsubw)


C_FUNCTION_BEGIN(mpadd)
	mtctr r3
	sldi r0,r3,3
	add r4,r4,r0
	add r5,r5,r0
	li r0,0
	ld r6,-8(r4)
	ldu r7,-8(r5)
	addc r6,r7,r6
	stdu r6,-8(r4)
	bdz LOCAL(mpadd_skip)
LOCAL(mpadd_loop):
	ld r6,-8(r4)
	ldu r7,-8(r5)
	adde r6,r7,r6
	stdu r6,-8(r4)
	bdnz LOCAL(mpadd_loop)
LOCAL(mpadd_skip):
	addze r3,r0
	blr
C_FUNCTION_END(mpadd)


C_FUNCTION_BEGIN(mpsub)
	mtctr r3
	sldi r0,r3,3
	add r4,r4,r0
	add r5,r5,r0
	li r0,0
	ld r6,-8(r4)
	ldu r7,-8(r5)
	subfc r6,r7,r6
	stdu r6,-8(r4)
	bdz LOCAL(mpsub_skip)
LOCAL(mpsub_loop):
	ld r6,-8(r4)
	ldu r7,-8(r5)
	subfe r6,r7,r6
	stdu r6,-8(r4)
	bdnz LOCAL(mpsub_loop)
LOCAL(mpsub_skip):
	subfe r3,r0,r0
	neg r3,r3
	blr
C_FUNCTION_END(mpsub)


C_FUNCTION_BEGIN(mpmultwo)
	mtctr r3
	sldi r0,r3,3
	add r4,r4,r0
	li r0,0
	ld r6,-8(r4)
	addc r6,r6,r6
	stdu r6,-8(r4)
	bdz LOCAL(mpmultwo_skip)
LOCAL(mpmultwo_loop):
	ld r6,-8(r4)
	adde r6,r6,r6
	stdu r6,-8(r4)
	bdnz LOCAL(mpmultwo_loop)
LOCAL(mpmultwo_skip):
	addze r3,r0
	blr
C_FUNCTION_END(mpmultwo)


C_FUNCTION_BEGIN(mpsetmul)
	mtctr r3
	sldi r0,r3,3
	add r4,r4,r0
	add r5,r5,r0
	li r3,0
LOCAL(mpsetmul_loop):
	ldu r7,-8(r5)
	mulld r8,r7,r6
	addc r8,r8,r3
	mulhdu r9,r7,r6
	addze r3,r9
	stdu r8,-8(r4)
	bdnz LOCAL(mpsetmul_loop)
	blr
C_FUNCTION_END(mpsetmul)


C_FUNCTION_BEGIN(mpaddmul)
	mtctr r3
	sldi r0,r3,3
	add r4,r4,r0
	add r5,r5,r0
	li r3,0
LOCAL(mpaddmul_loop):
	ldu r8,-8(r5)
	ldu r7,-8(r4)
	mulld r9,r8,r6
	addc r9,r9,r3
	mulhdu r10,r8,r6
	addze r3,r10
	addc r9,r9,r7
	addze r3,r3
	std r9,0(r4)
	bdnz LOCAL(mpaddmul_loop)
	blr
C_FUNCTION_END(mpaddmul)


C_FUNCTION_BEGIN(mpaddsqrtrc)
	mtctr r3
	sldi r0,r3,3
	add r4,r4,r0
	add r5,r5,r0
	add r4,r4,r0
	li r3,0
LOCAL(mpaddsqrtrc_loop):
	ldu r0,-8(r5)
	ld r6,-16(r4)
	ld r7,-8(r4)
	mulld r9,r0,r0
	addc r9,r9,r3
	mulhdu r8,r0,r0
	addze r8,r8
	li r3,0
	addc r7,r7,r9
	adde r6,r6,r8
	addze r3,r3
	std r7,-8(r4)
	stdu r6,-16(r4)
	bdnz LOCAL(mpaddsqrtrc_loop)
	blr
C_FUNCTION_END(mpaddsqrtrc)
