/*
 * fips180opt.powerpc.S
 *
 * Assembler optimized SHA-1 routines for PowerPC processors
 *
 * Warning: this code is incomplete and only contains a rough prototype!
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

#include "config.gas.h"

	.file "fips180opt.powerpc.S"

	.text

#if DARWIN
# define reg0	r0
# define reg3	r3
# define reg4	r4
# define reg5	r5
# define reg6	r6
# define reg7	r7
# define reg8	r8
# define reg9	r9
# define reg26	r26
# define reg27	r27
# define reg28	r28
# define reg29	r29
# define reg30	r30
# define reg31	r31
#else
# define reg0	%r0
# define reg3	%r3
# define reg4	%r4
# define reg5	%r5
# define reg6	%r6
# define reg7	%r7
# define reg8	%r8
# define reg9	%r9
# define reg26	%r26
# define reg27	%r27
# define reg28	%r28
# define reg29	%r29
# define reg30	%r30
# define reg31	%r31
#endif

	.equ	K00,	0x5a827999
	.equ	K20,	0x6ed9eba1
	.equ	K40,	0x8f1bbcdc
	.equ	K60,	0xca62c1d6

	.equ	PARAM_H,		0
	.equ	PARAM_DATA,		20

/* sha1Param: param in reg3 */

	.macro	subround1 a b c d e w
	lwzu reg7,4(\w)
	rotlwi	reg5,\a,5
	dbct r0,\w
	xor reg6,\c,\d
	add \e,\e,K00
	and reg6,reg6,\b
	add \e,\e,reg7
	xor reg6,reg6,\d
	add \e,\e,reg5
	rotrwi \b,\b,2
	add \e,\e,reg6
	.endm

	.macro	subround2 a b c d e w
	lwzu reg7,4(\w)
	rotlwi reg5,\a,5
	dbct r0,\w
	add \e,\e,K20
	xor reg6,\b,\c
	add \e,\e,reg5
	xor reg6,reg6,\d
	add \e,\e,reg7
	rotrwi \b,\b,2
	add \e,\e,reg6
	.endm

	.macro	subround3 a b c d e w
	lwzu reg7,4(\w)
	rotlwi reg5,\a,5
	dbct r0,\w
	xor reg6,\b,\c
	add \e,\e,reg5
	and reg6,reg6,\d
	add \e,\e,K40
	and reg5,\b,\c
	add \e,\e,reg7
	or reg6,reg6,reg5
	rotrwi \b,\b,2
	add \e,\e,reg6
	.endm

	.macro	subround4 a b c d e w
	lwzu reg7,4(\w)
	rotlwi reg5,\a,5
	dbct r0,\w
	add \e,\e,K60
	xor reg6,\b,\c
	add \e,\e,reg5
	xor reg6,reg6,\d
	add \e,\e,reg7
	rotrwi \b,\b,2
	add \e,\e,reg6
	.endm

C_FUNCTION_BEGIN(sha1Process)
/* zero reg0 for general use */
	li reg0,0
/* for a,b,c,d,e use r26,r27,r28,r29,r30, for w use r31 */
	
/* we need to save registers before loading them */
	stmw reg26,-24(reg1)
/* load the frame pointer with parameter data, and hint cache */
	addi reg31,reg3,PARAM_DATA
	dbct reg31

#if !WORDS_BIGENDIAN /* have to provide for PowerPC little-endian mode
	/* loop of 16 entries */
	li reg5,60
	mtctr reg6
.L00:
	lwbrx reg6,reg31,reg5
	stwx reg6,reg31,reg5
	subi. reg5,reg5,4
	bcge cr0,.L00
	addi reg31,reg3,PARAM_DATA
#endif

/* do the initial mixing */
	li reg8,64
	addi reg26,reg3,PARAM_DATA+64-4
	addi reg27,reg3,PARAM_DATA+64-3*4-4
	addi reg28,reg3,PARAM_DATA+64-8*4-4
	addi reg29,reg3,PARAM_DATA+64-14*4-4
	addi reg30,reg3,PARAM_DATA+64-16*4-4
	mtctr reg8

.L10:
	lwzu reg5,4(reg27)
	lwzu reg6,4(reg28)
	lwzu reg7,4(reg29)
	lwzu reg8,4(reg30)
	xor reg5,reg5,reg6
	xor reg7,reg7,reg8
	xor reg5,reg5,reg7
	stwu reg5,4(reg26)
	bdnz .L10

	lwz reg26,PARAM_H   (reg3)
	lwz reg27,PARAM_H+4 (reg3)
	lwz reg28,PARAM_H+8 (reg3)
	lwz reg29,PARAM_H+12(reg3)
	lwz reg30,PARAM_H+16(reg3)

	subround1 reg26,reg27,reg28,reg29,reg30,reg31
	subround1 reg30,reg26,reg27,reg28,reg29,reg31
	subround1 reg29,reg30,reg26,reg27,reg28,reg31
	subround1 reg28,reg29,reg30,reg26,reg27,reg31
	subround1 reg27,reg28,reg29,reg30,reg26,reg31
	subround1 reg26,reg27,reg28,reg29,reg30,reg31
	subround1 reg30,reg26,reg27,reg28,reg29,reg31
	subround1 reg29,reg30,reg26,reg27,reg28,reg31
	subround1 reg28,reg29,reg30,reg26,reg27,reg31
	subround1 reg27,reg28,reg29,reg30,reg26,reg31
	subround1 reg26,reg27,reg28,reg29,reg30,reg31
	subround1 reg30,reg26,reg27,reg28,reg29,reg31
	subround1 reg29,reg30,reg26,reg27,reg28,reg31
	subround1 reg28,reg29,reg30,reg26,reg27,reg31
	subround1 reg27,reg28,reg29,reg30,reg26,reg31
	subround1 reg26,reg27,reg28,reg29,reg30,reg31
	subround1 reg30,reg26,reg27,reg28,reg29,reg31
	subround1 reg29,reg30,reg26,reg27,reg28,reg31
	subround1 reg28,reg29,reg30,reg26,reg27,reg31
	subround1 reg27,reg28,reg29,reg30,reg26,reg31

	subround2 reg26,reg27,reg28,reg29,reg30,reg31
	subround2 reg30,reg26,reg27,reg28,reg29,reg31
	subround2 reg29,reg30,reg26,reg27,reg28,reg31
	subround2 reg28,reg29,reg30,reg26,reg27,reg31
	subround2 reg27,reg28,reg29,reg30,reg26,reg31
	subround2 reg26,reg27,reg28,reg29,reg30,reg31
	subround2 reg30,reg26,reg27,reg28,reg29,reg31
	subround2 reg29,reg30,reg26,reg27,reg28,reg31
	subround2 reg28,reg29,reg30,reg26,reg27,reg31
	subround2 reg27,reg28,reg29,reg30,reg26,reg31
	subround2 reg26,reg27,reg28,reg29,reg30,reg31
	subround2 reg30,reg26,reg27,reg28,reg29,reg31
	subround2 reg29,reg30,reg26,reg27,reg28,reg31
	subround2 reg28,reg29,reg30,reg26,reg27,reg31
	subround2 reg27,reg28,reg29,reg30,reg26,reg31
	subround2 reg26,reg27,reg28,reg29,reg30,reg31
	subround2 reg30,reg26,reg27,reg28,reg29,reg31
	subround2 reg29,reg30,reg26,reg27,reg28,reg31
	subround2 reg28,reg29,reg30,reg26,reg27,reg31
	subround2 reg27,reg28,reg29,reg30,reg26,reg31

	subround3 reg26,reg27,reg28,reg29,reg30,reg31
	subround3 reg30,reg26,reg27,reg28,reg29,reg31
	subround3 reg29,reg30,reg26,reg27,reg28,reg31
	subround3 reg28,reg29,reg30,reg26,reg27,reg31
	subround3 reg27,reg28,reg29,reg30,reg26,reg31
	subround3 reg26,reg27,reg28,reg29,reg30,reg31
	subround3 reg30,reg26,reg27,reg28,reg29,reg31
	subround3 reg29,reg30,reg26,reg27,reg28,reg31
	subround3 reg28,reg29,reg30,reg26,reg27,reg31
	subround3 reg27,reg28,reg29,reg30,reg26,reg31
	subround3 reg26,reg27,reg28,reg29,reg30,reg31
	subround3 reg30,reg26,reg27,reg28,reg29,reg31
	subround3 reg29,reg30,reg26,reg27,reg28,reg31
	subround3 reg28,reg29,reg30,reg26,reg27,reg31
	subround3 reg27,reg28,reg29,reg30,reg26,reg31
	subround3 reg26,reg27,reg28,reg29,reg30,reg31
	subround3 reg30,reg26,reg27,reg28,reg29,reg31
	subround3 reg29,reg30,reg26,reg27,reg28,reg31
	subround3 reg28,reg29,reg30,reg26,reg27,reg31
	subround3 reg27,reg28,reg29,reg30,reg26,reg31

	subround4 reg26,reg27,reg28,reg29,reg30,reg31
	subround4 reg30,reg26,reg27,reg28,reg29,reg31
	subround4 reg29,reg30,reg26,reg27,reg28,reg31
	subround4 reg28,reg29,reg30,reg26,reg27,reg31
	subround4 reg27,reg28,reg29,reg30,reg26,reg31
	subround4 reg26,reg27,reg28,reg29,reg30,reg31
	subround4 reg30,reg26,reg27,reg28,reg29,reg31
	subround4 reg29,reg30,reg26,reg27,reg28,reg31
	subround4 reg28,reg29,reg30,reg26,reg27,reg31
	subround4 reg27,reg28,reg29,reg30,reg26,reg31
	subround4 reg26,reg27,reg28,reg29,reg30,reg31
	subround4 reg30,reg26,reg27,reg28,reg29,reg31
	subround4 reg29,reg30,reg26,reg27,reg28,reg31
	subround4 reg28,reg29,reg30,reg26,reg27,reg31
	subround4 reg27,reg28,reg29,reg30,reg26,reg31
	subround4 reg26,reg27,reg28,reg29,reg30,reg31
	subround4 reg30,reg26,reg27,reg28,reg29,reg31
	subround4 reg29,reg30,reg26,reg27,reg28,reg31
	subround4 reg28,reg29,reg30,reg26,reg27,reg31
	subround4 reg27,reg28,reg29,reg30,reg26,reg31

/* then store the five values into registers */
	lwz reg5,PARAM_H   (reg3)
	lwz reg6,PARAM_H+4 (reg3)
	lwz reg7,PARAM_H+8 (reg3)
	lwz reg8,PARAM_H+12(reg3)
	lwz reg9,PARAM_H+16(reg3)
	add reg26,reg5,reg26
	add reg27,reg5,reg27
	add reg28,reg5,reg28
	add reg29,reg5,reg29
	add reg30,reg5,reg30
	stw reg26,PARAM_H   (reg3)
	stw reg27,PARAM_H+4 (reg3)
	stw reg28,PARAM_H+8 (reg3)
	stw reg29,PARAM_H+12(reg3)
	stw reg30,PARAM_H+16(reg3)

/* finally, restore registers */
	lmw reg26,-24(reg1)
/* and return */
	blr
C_FUNCION_END(sha1Process, .Lsha1Process_size)
