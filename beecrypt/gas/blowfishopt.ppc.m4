dnl  blowfishopt.ppc.m4
dnl
dnl  Note: Only tested on big-endian PowerPC!
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

define(`round',`
	lwz r9,$3(r3)
	xor $1,$1,r9
	rlwinm r9,$1,10,22,29
	rlwinm r10,$1,18,22,29
	lwzx r9,r9,r28
	lwzx r10,r10,r29
	rlwinm r11,$1,26,22,29
	add r9,r9,r10
	lwzx r11,r11,r30
	rlwinm r12,$1,2,22,29
	xor r9,r9,r11
	lwzx r12,r12,r31
	add r9,r9,r12
	xor $2,$2,r9
')

define(`eblock',`
	round(r7,r8,0)
	round(r8,r7,4)
	round(r7,r8,8)
	round(r8,r7,12)
	round(r7,r8,16)
	round(r8,r7,20)
	round(r7,r8,24)
	round(r8,r7,28)
	round(r7,r8,32)
	round(r8,r7,36)
	round(r7,r8,40)
	round(r8,r7,44)
	round(r7,r8,48)
	round(r8,r7,52)
	round(r7,r8,56)
	round(r8,r7,60)
	lwz r9,64(r3)
	lwz r10,68(r3)
	xor r7,r7,r9
	xor r8,r8,r10
')

define(`dblock',`
	round(r7,r8,68)
	round(r8,r7,64)
	round(r7,r8,60)
	round(r8,r7,56)
	round(r7,r8,52)
	round(r8,r7,48)
	round(r7,r8,44)
	round(r8,r7,40)
	round(r7,r8,36)
	round(r8,r7,32)
	round(r7,r8,28)
	round(r8,r7,24)
	round(r7,r8,20)
	round(r8,r7,16)
	round(r7,r8,12)
	round(r8,r7,8)
	lwz r9,4(r3)
	lwz r10,0(r3)
	xor r7,r7,r9
	xor r8,r8,r10
')


C_FUNCTION_BEGIN(blowfishEncrypt)
	la r1,-16(r1)
	stmw r28,0(r1)

	la r28,72(r3)
	la r29,1096(r3)
	la r30,2120(r3)
	la r31,3144(r3)

ifelse(ASM_BIGENDIAN,yes,`
	lwz r7,0(r5)
	lwz r8,4(r5)
',`
	li r0,0
	lwbrx r7,r5,r0
	li r0,4
	lwbrx r8,r5,r0
')
	
	eblock

ifelse(ASM_BIGENDIAN,yes,`
	stw r7,4(r4)
	stw r8,0(r4)
',`
	li r0,4
	stwbrx r7,r4,r0
	li r0,0
	stwbrx r8,r4,r0
')

	li r3,0
	lmw r28,0(r1)
	la r1,16(r1)
	blr
C_FUNCTION_END(blowfishEncrypt)


C_FUNCTION_BEGIN(blowfishDecrypt)
	la r1,-16(r1)
	stmw r28,0(r1)

	la r28,72(r3)
	la r29,1096(r3)
	la r30,2120(r3)
	la r31,3144(r3)

ifelse(ASM_BIGENDIAN,yes,`
	lwz r7,0(r5)
	lwz r8,4(r5)
',`
	li r0,0
	lwbrx r7,r5,r0
	li r0,4
	lwbrx r7,r5,r0
')
	
	dblock

ifelse(ASM_BIGENDIAN,yes,`
	stw r7,4(r4)
	stw r8,0(r4)
',`
	li r0,4
	stwbrx r7,r4,r0
	li r0,0
	stwbrx r7,r4,r0
')

	li r3,0
	lmw r28,0(r1)
	la r1,16(r1)
	blr
C_FUNCTION_END(blowfishDecrypt)
