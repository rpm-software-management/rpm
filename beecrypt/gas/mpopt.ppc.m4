dnl  mpopt.ppc.m4
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
include(ASM_SRCDIR/ppc.m4)


C_FUNCTION_BEGIN(mpaddw)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	li r0,0
	lwzu r6,-4(r4)
	addc r6,r6,r5
	stw r6,0(r4)
	bdz LOCAL(mpaddw_skip)
LOCAL(mpaddw_loop):
	lwzu r6,-4(r4)
	adde r6,r0,r6
	stw r6,0(r4)
	bdnz LOCAL(mpaddw_loop)
LOCAL(mpaddw_skip):
	addze r3,r0
	blr
C_FUNCTION_END(mpaddw)


C_FUNCTION_BEGIN(mpsubw)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	li r0,0
	lwz r6,-4(r4)
	subfc r6,r5,r6
	stwu r6,-4(r4)
	bdz LOCAL(mpsubw_skip)
LOCAL(mpsubw_loop):
	lwz r6,-4(r4)
	subfe r6,r0,r6
	stwu r6, -4(r4)
	bdnz LOCAL(mpsubw_loop)
LOCAL(mpsubw_skip):
	subfe r3,r0,r0
	neg r3,r3
	blr
C_FUNCTION_END(mpsubw)


C_FUNCTION_BEGIN(mpadd)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	li r0,0
	lwz r6,-4(r4)
	lwzu r7,-4(r5)
	addc r6,r7,r6
	stwu r6,-4(r4)
	bdz LOCAL(mpadd_skip)
LOCAL(mpadd_loop):
	lwz r6,-4(r4)
	lwzu r7,-4(r5)
	adde r6,r7,r6
	stwu r6,-4(r4)
	bdnz LOCAL(mpadd_loop)
LOCAL(mpadd_skip):
	addze r3,r0
	blr
C_FUNCTION_END(mpadd)


C_FUNCTION_BEGIN(mpsub)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	li r0,0
	lwz r6,-4(r4)
	lwzu r7,-4(r5)
	subfc r6,r7,r6
	stwu r6,-4(r4)
	bdz LOCAL(mpsub_skip)
LOCAL(mpsub_loop):
	lwz r6,-4(r4)
	lwzu r7,-4(r5)
	subfe r6,r7,r6
	stwu r6,-4(r4)
	bdnz LOCAL(mpsub_loop)
LOCAL(mpsub_skip):
	subfe r3,r0,r0
	neg r3,r3
	blr
C_FUNCTION_END(mpsub)


C_FUNCTION_BEGIN(mpmultwo)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	li r0,0
	lwz r6,-4(r4)
	addc r6,r6,r6
	stwu r6,-4(r4)
	bdz LOCAL(mpmultwo_skip)
LOCAL(mpmultwo_loop):
	lwz r6,-4(r4)
	adde r6,r6,r6
	stwu r6,-4(r4)
	bdnz LOCAL(mpmultwo_loop)
LOCAL(mpmultwo_skip):
	addze r3,r0
	blr
C_FUNCTION_END(mpmultwo)


C_FUNCTION_BEGIN(mpsetmul)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	li r3,0
LOCAL(mpsetmul_loop):
	lwzu r7,-4(r5)
	mullw r8,r7,r6
	addc r8,r8,r3
	mulhwu r9,r7,r6
	addze r3,r9
	stwu r8,-4(r4)
	bdnz LOCAL(mpsetmul_loop)
	blr
C_FUNCTION_END(mpsetmul)


C_FUNCTION_BEGIN(mpaddmul)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	li r3,0
LOCAL(mpaddmul_loop):
	lwzu r8,-4(r5)
	lwzu r7,-4(r4)
	mullw r9,r8,r6
	addc r9,r9,r3
	mulhwu r10,r8,r6
	addze r3,r10
	addc r9,r9,r7
	addze r3,r3
	stw r9,0(r4)
	bdnz LOCAL(mpaddmul_loop)
	blr
C_FUNCTION_END(mpaddmul)


C_FUNCTION_BEGIN(mpaddsqrtrc)
	mtctr r3
	slwi r0,r3,2
	add r4,r4,r0
	add r5,r5,r0
	add r4,r4,r0
	li r3,0
LOCAL(mpaddsqrtrc_loop):
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
	bdnz LOCAL(mpaddsqrtrc_loop)
	blr
C_FUNCTION_END(mpaddsqrtrc)
