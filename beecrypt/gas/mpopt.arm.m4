dnl  mpopt.arm.m4
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


C_FUNCTION_BEGIN(mpsetmul)
	stmfd sp!, {r4, r5, lr}
	add r1, r1, r0, asl #2
	add r2, r2, r0, asl #2
	mov ip, #0
LOCAL(mpsetmul_loop):
	ldr r4, [r2, #-4]!
	mov r5, #0
	umlal ip, r5, r3, r4
	str ip, [r1, #-4]!
	mov ip, r5
	subs r0, r0, #1
	bne LOCAL(mpsetmul_loop)
	mov r0, ip
	ldmfd sp!, {r4, r5, pc}
C_FUNCTION_END(mpsetmul)

	
C_FUNCTION_BEGIN(mpaddmul)
	stmfd sp!, {r4, r5, r6, lr}
	add r1, r1, r0, asl #2
	add r2, r2, r0, asl #2
	mov ip, #0
LOCAL(mpaddmul_loop):
	ldr r4, [r2, #-4]!
	ldr r5, [r1, #-4]
	mov r6, #0
	umlal ip, r6, r3, r4
	adds r5, r5, ip
	adc ip, r6, #0
	str r5, [r1, #-4]!
	subs r0, r0, #1
	bne LOCAL(mpaddmul_loop)
	mov r0, ip
	ldmfd sp!, {r4, r5, r6, pc}
C_FUNCTION_END(mpaddmul)


C_FUNCTION_BEGIN(mpaddsqrtrc)
	stmfd sp!, {r4, r5, r6, lr}
	add r1, r1, r0, asl #3
	add r2, r2, r0, asl #2
	mov r3, #0
	mov ip, #0
LOCAL(mpaddsqrtrc_loop):
	ldr r4, [r2, #-4]!
	mov r6, #0
	umlal ip, r6, r4, r4
	ldr r5, [r1, #-4]
	ldr r4, [r1, #-8]
	adds r5, r5, ip
	adcs r4, r4, r6
	str r5, [r1, #-4]
	str r4, [r1, #-8]!
	adc ip, r3, #0
	subs r0, r0, #1
	bne LOCAL(mpaddsqrtrc_loop)
	mov r0, ip
	ldmfd sp!, {r4, r5, r6, pc}
C_FUNCTION_END(mpaddsqrtrc)
