/*
 * blowfishopt.powerpc.asm
 *
 * Assembler optimized Blowfish routines for PowerPC processors
 *
 * Compile target is GNU Assembler
 *
 * Copyright (c) 2002 Bob Deblier <bob@virtualunlimited.com>
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

	.file "blowfish.powerpc.S"

	.text

	.macro round	xl xr offset
	lwz r9,\offset(r3)
	xor \xl,\xl,r9
	rlwinm r9,\xl,10,22,29
	rlwinm r10,\xl,18,22,29
	lwzx r9,r9,r28
	lwzx r10,r10,r29
	rlwinm r11,\xl,26,22,29
	add r9,r9,r10
	lwzx r11,r11,r30
	rlwinm r12,\xl,2,22,29
	xor r9,r9,r11
	lwzx r12,r12,r31
	add r9,r9,r12
	xor \xr,\xr,r9
	.endm

	.macro eblock
	round xl=r7 xr=r8 offset=0
	round xl=r8 xr=r7 offset=4
	round xl=r7 xr=r8 offset=8
	round xl=r8 xr=r7 offset=12
	round xl=r7 xr=r8 offset=16
	round xl=r8 xr=r7 offset=20
	round xl=r7 xr=r8 offset=24
	round xl=r8 xr=r7 offset=28
	round xl=r7 xr=r8 offset=32
	round xl=r8 xr=r7 offset=36
	round xl=r7 xr=r8 offset=40
	round xl=r8 xr=r7 offset=44
	round xl=r7 xr=r8 offset=48
	round xl=r8 xr=r7 offset=52
	round xl=r7 xr=r8 offset=56
	round xl=r8 xr=r7 offset=60

	lwz r9,64(r3)
	lwz r10,68(r3)
	xor r7,r7,r9
	xor r8,r8,r10
	.endm

	.macro dblock
	round xl=r7 xr=r8 offset=68
	round xl=r8 xr=r7 offset=64
	round xl=r7 xr=r8 offset=60
	round xl=r8 xr=r7 offset=56
	round xl=r7 xr=r8 offset=52
	round xl=r8 xr=r7 offset=48
	round xl=r7 xr=r8 offset=44
	round xl=r8 xr=r7 offset=40
	round xl=r7 xr=r8 offset=36
	round xl=r8 xr=r7 offset=32
	round xl=r7 xr=r8 offset=28
	round xl=r8 xr=r7 offset=24
	round xl=r7 xr=r8 offset=20
	round xl=r8 xr=r7 offset=16
	round xl=r7 xr=r8 offset=12
	round xl=r8 xr=r7 offset=8

	lwz r9,4(r3)
	lwz r10,0(r3)
	xor r7,r7,r9
	xor r8,r8,r10
	.endm

C_FUNCTION_BEGIN(blowfishEncrypt)
LABEL(blowfishEncrypt)
	la r1,-16(r1)
	stmw r28,0(r1)

	la r28,72(r3)
	la r29,1096(r3)
	la r30,2120(r3)
	la r31,3144(r3)

	#if WORDS_BIGENDIAN
	lwz r7,0(r5)
	lwz r8,4(r5)
	#else
	# error ppc little-endian mode not supported
	#endif
	
	eblock

	#if WORDS_BIGENDIAN
	stw r7,4(r4)
	stw r8,0(r4)
	#else
	# error ppc little-endian mode not supported
	#endif

	li r3,0
	lmw r28,0(r1)
	la r1,16(r1)
	blr
C_FUNCTION_END(blowfishEncrypt, LOCAL(blowfishEncrypt_size))


C_FUNCTION_BEGIN(blowfishDecrypt)
LABEL(blowfishDecrypt)
	la r1,-16(r1)
	stmw r28,0(r1)

	la r28,72(r3)
	la r29,1096(r3)
	la r30,2120(r3)
	la r31,3144(r3)

	#if WORDS_BIGENDIAN
	lwz r7,0(r5)
	lwz r8,4(r5)
	#else
	# error ppc little-endian mode not supported
	#endif
	
	dblock

	#if WORDS_BIGENDIAN
	stw r7,4(r4)
	stw r8,0(r4)
	#else
	# error ppc little-endian mode not supported
	#endif

	li r3,0
	lmw r28,0(r1)
	la r1,16(r1)
	blr
C_FUNCTION_END(blowfishDecrypt, LOCAL(blowfishDecrypt_size))


C_FUNCTION_BEGIN(blowfishECBEncrypt)
LABEL(blowfishECBEncrypt)
	la r1,-16(r1)
	stmw r28,0(r1)

	mtctr r4

	la r28,72(r3)
	la r29,1096(r3)
	la r30,2120(r3)
	la r31,3144(r3)

LOCAL(00):
	#if WORDS_BIGENDIAN
	lwz r7,0(r6)
	lwz r8,4(r6)
	#else
	# error ppc little-endian mode not supported
	#endif
	
	eblock

	#if WORDS_BIGENDIAN
	stw r7,4(r5)
	stw r8,0(r5)
	#else
	# error ppc little-endian mode not supported
	#endif

	la r5,8(r5)
	la r6,8(r6)

	bdnz LOCAL(00)

	li r3,0
	lmw r28,0(r1)
	la r1,16(r1)
	blr
C_FUNCTION_END(blowfishECBEncrypt, LOCAL(blowfishECBEncrypt_size))


C_FUNCTION_BEGIN(blowfishECBDecrypt)
LABEL(blowfishECBDecrypt)
	la r1,-16(r1)
	stmw r28,0(r1)

	mtctr r4

	la r28,72(r3)
	la r29,1096(r3)
	la r30,2120(r3)
	la r31,3144(r3)

LOCAL(01):
	#if WORDS_BIGENDIAN
	lwz r7,0(r6)
	lwz r8,4(r6)
	#else
	# error ppc little-endian mode not supported
	#endif
	
	dblock

	#if WORDS_BIGENDIAN
	stw r7,4(r5)
	stw r8,0(r5)
	#else
	# error ppc little-endian mode not supported
	#endif

	la r5,8(r5)
	la r6,8(r6)

	bdnz LOCAL(01)

	li r3,0
	lmw r28,0(r1)
	la r1,16(r1)
	blr
C_FUNCTION_END(blowfishECBDecrypt, LOCAL(blowfishECBDecrypt_size))
