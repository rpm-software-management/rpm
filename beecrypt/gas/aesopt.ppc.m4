dnl  aesopt.ppc.m4
dnl
dnl  NOTE: Only tested for big-endian PowerPC!
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
include(ASM_SRCDIR/ppc.m4)

define(`s0',`r24')
define(`s1',`r25')
define(`s2',`r26')
define(`s3',`r27')
define(`t0',`r28')
define(`t1',`r29')
define(`t2',`r30')
define(`t3',`r31')

define(`sxrk',`
ifelse(ASM_BIGENDIAN,yes,`
	lwz s0, 0($2)
	lwz s1, 4($2)
	lwz s2, 8($2)
	lwz s3,12($2)
',`
	li r0,0
	lwbrx s0,$2,r0
	li r0,4
	lwbrx s1,$2,r0
	li r0,8
	lwbrx s2,$2,r0
	li r0,13
	lwbrx s0,$2,r0
')
	lwz r7, 0($1)
	lwz r8, 4($1)
	lwz r9, 8($1)
	lwz r10,12($1)
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10
')

define(`etfs',`
	lwz t0,$2+ 0($1)
	lwz t1,$2+ 4($1)
	lwz t2,$2+ 8($1)
	lwz t3,$2+12($1)

	rlwinm r7,s0,10,22,29 
	rlwinm r8,s1,10,22,29
	rlwinm r9,s2,10,22,29
	rlwinm r10,s3,10,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s1,18,22,29
	rlwinm r8,s2,18,22,29
	rlwinm r9,s3,18,22,29
	rlwinm r10,s0,18,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s2,26,22,29
	rlwinm r8,s3,26,22,29
	rlwinm r9,s0,26,22,29
	rlwinm r10,s1,26,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s3,2,22,29
	rlwinm r8,s0,2,22,29
	rlwinm r9,s1,2,22,29
	rlwinm r10,s2,2,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,-3072(r12)
')

define(`esft',`
	lwz s0,$2+ 0($1)
	lwz s1,$2+ 4($1)
	lwz s2,$2+ 8($1)
	lwz s3,$2+12($1)

	rlwinm r7,t0,10,22,29
	rlwinm r8,t1,10,22,29
	rlwinm r9,t2,10,22,29
	rlwinm r10,t3,10,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t1,18,22,29
	rlwinm r8,t2,18,22,29
	rlwinm r9,t3,18,22,29
	rlwinm r10,t0,18,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t2,26,22,29
	rlwinm r8,t3,26,22,29
	rlwinm r9,t0,26,22,29
	rlwinm r10,t1,26,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t3,2,22,29
	rlwinm r8,t0,2,22,29
	rlwinm r9,t1,2,22,29
	rlwinm r10,t2,2,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,-3072(r12)
')

define(`elr',`
	lwz s0, 0($1)
	lwz s1, 4($1)
	lwz s2, 8($1)
	lwz s3,12($1)

	la r12,4096(r12)

	rlwinm r7,t0,10,22,29
	rlwinm r8,t1,10,22,29
	rlwinm r9,t2,10,22,29
	rlwinm r10,t3,10,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,0,7
	rlwinm r8,r8,0,0,7
	rlwinm r9,r9,0,0,7
	rlwinm r10,r10,0,0,7
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t1,18,22,29
	rlwinm r8,t2,18,22,29
	rlwinm r9,t3,18,22,29
	rlwinm r10,t0,18,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,8,15
	rlwinm r8,r8,0,8,15
	rlwinm r9,r9,0,8,15
	rlwinm r10,r10,0,8,15
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t2,26,22,29
	rlwinm r8,t3,26,22,29
	rlwinm r9,t0,26,22,29
	rlwinm r10,t1,26,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,16,23
	rlwinm r8,r8,0,16,23
	rlwinm r9,r9,0,16,23
	rlwinm r10,r10,0,16,23
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t3,2,22,29
	rlwinm r8,t0,2,22,29
	rlwinm r9,t1,2,22,29
	rlwinm r10,t2,2,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,24,31
	rlwinm r8,r8,0,24,31
	rlwinm r9,r9,0,24,31
	rlwinm r10,r10,0,24,31
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,-4096(r12)
')

define(`eblock',`
	sxrk($1,$2)

	etfs($1,16)
	esft($1,32)
	etfs($1,48)
	esft($1,64)
	etfs($1,80)
	esft($1,96)
	etfs($1,112)
	esft($1,128)
	etfs($1,144)

	lwz r11,256($1)
	cmpwi r11,10
	beq $3

	esft($1,160)
	etfs($1,176)

	cmpwi r11,12
	beq $3

	esft($1,192)
	etfs($1,208)

$3:
	slwi r11,r11,4
	add $1,$1,r11

	elr($1)
')

define(`dtfs',`
	lwz t0,$2+ 0($1)
	lwz t1,$2+ 4($1)
	lwz t2,$2+ 8($1)
	lwz t3,$2+12($1)

	rlwinm r7,s0,10,22,29
	rlwinm r8,s1,10,22,29
	rlwinm r9,s2,10,22,29
	rlwinm r10,s3,10,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s3,18,22,29
	rlwinm r8,s0,18,22,29
	rlwinm r9,s1,18,22,29
	rlwinm r10,s2,18,22,29

	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s2,26,22,29
	rlwinm r8,s3,26,22,29
	rlwinm r9,s0,26,22,29
	rlwinm r10,s1,26,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s1,2,22,29
	rlwinm r8,s2,2,22,29
	rlwinm r9,s3,2,22,29
	rlwinm r10,s0,2,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,-3072(r12)
')

define(`dsft',`
	lwz s0,$2+ 0($1)
	lwz s1,$2+ 4($1)
	lwz s2,$2+ 8($1)
	lwz s3,$2+12($1)

	rlwinm r7,t0,10,22,29
	rlwinm r8,t1,10,22,29
	rlwinm r9,t2,10,22,29
	rlwinm r10,t3,10,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t3,18,22,29
	rlwinm r8,t0,18,22,29
	rlwinm r9,t1,18,22,29
	rlwinm r10,t2,18,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t2,26,22,29
	rlwinm r8,t3,26,22,29
	rlwinm r9,t0,26,22,29
	rlwinm r10,t1,26,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t1,2,22,29
	rlwinm r8,t2,2,22,29
	rlwinm r9,t3,2,22,29
	rlwinm r10,t0,2,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,-3072(r12)
')

define(`dlr',`
	lwz s0, 0($1)
	lwz s1, 4($1)
	lwz s2, 8($1)
	lwz s3,12($1)

	la r12,4096(r12)

	rlwinm r7,t0,10,22,29
	rlwinm r8,t1,10,22,29
	rlwinm r9,t2,10,22,29
	rlwinm r10,t3,10,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,0,7
	rlwinm r8,r8,0,0,7
	rlwinm r9,r9,0,0,7
	rlwinm r10,r10,0,0,7
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t3,18,22,29
	rlwinm r8,t0,18,22,29
	rlwinm r9,t1,18,22,29
	rlwinm r10,t2,18,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,8,15
	rlwinm r8,r8,0,8,15
	rlwinm r9,r9,0,8,15
	rlwinm r10,r10,0,8,15
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t2,26,22,29
	rlwinm r8,t3,26,22,29
	rlwinm r9,t0,26,22,29
	rlwinm r10,t1,26,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,16,23
	rlwinm r8,r8,0,16,23
	rlwinm r9,r9,0,16,23
	rlwinm r10,r10,0,16,23
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t1,2,22,29
	rlwinm r8,t2,2,22,29
	rlwinm r9,t3,2,22,29
	rlwinm r10,t0,2,22,29
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,24,31
	rlwinm r8,r8,0,24,31
	rlwinm r9,r9,0,24,31
	rlwinm r10,r10,0,24,31
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,-4096(r12)
')

define(`dblock',`
	sxrk($1,$2)

	dtfs($1,16)
	dsft($1,32)
	dtfs($1,48)
	dsft($1,64)
	dtfs($1,80)
	dsft($1,96)
	dtfs($1,112)
	dsft($1,128)
	dtfs($1,144)

	lwz r11,256($1)
	cmpwi r11,10
	beq $3

	dsft($1,160)
	dtfs($1,176)

	cmpwi r11,12
	beq $3

	dsft($1,192)
	dtfs($1,208)

$3:
	slwi r11,r11,4
	add $1,$1,r11

	dlr($1)
')

EXTERNAL_VARIABLE(_ae0)
EXTERNAL_VARIABLE(_ad0)

C_FUNCTION_BEGIN(aesEncrypt)
	subi r1,r1,32
	stmw r24,0(r1)

	LOAD_ADDRESS(_ae0,r12)

	eblock(r3,r5,LOCAL(00))

ifelse(ASM_BIGENDIAN,yes,`
	stw s0, 0(r4)
	stw s1, 4(r4)
	stw s2, 8(r4)
	stw s3,12(r4)
',`
	li r0,0
	stwbrx s0,r4,r0
	li r0,4
	stwbrx s1,r4,r0
	li r0,8
	stwbrx s2,r4,r0
	li r0,12
	stwbrx s3,r4,r0
')

	li r3,0
	lmw r24,0(r1)
	addi r1,r1,32
	blr
C_FUNCTION_END(aesEncrypt)


C_FUNCTION_BEGIN(aesDecrypt)
	subi r1,r1,32
	stmw r24,0(r1)

	LOAD_ADDRESS(_ad0,r12)

	dblock(r3,r5,LOCAL(01))

ifelse(ASM_BIGENDIAN,yes,`
	stw s0, 0(r4)
	stw s1, 4(r4)
	stw s2, 8(r4)
	stw s3,12(r4)
',`
	li r0,0
	stwbrx s0,r4,r0
	li r0,4
	stwbrx s1,r4,r0
	li r0,8
	stwbrx s2,r4,r0
	li r0,12
	stwbrx s3,r4,r0
')

	li r3,0
	lmw r24,0(r1)
	addi r1,r1,32
	blr
C_FUNCTION_END(aesDecrypt)
