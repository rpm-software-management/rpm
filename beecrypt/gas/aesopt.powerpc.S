/*
 * aesopt.powerpc.asm
 *
 * Assembler optimized AES routines for PowerPC processors
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

	.file "aesopt.powerpc.S"

	.text

/*
 * ae0 in r11
 * r7 thru r10 scratch
 * four registers for s: r24 r25 r26 r27
 * four registers for t: r28 r29 r30 r31
 */

#define s0 r24
#define s1 r25
#define s2 r26
#define s3 r27
#define t0 r28
#define t1 r29
#define t2 r30
#define t3 r31

	.macro	sxrk rk src
	#if WORDS_BIGENDIAN
	lwz s0, 0(\src)
	lwz s1, 4(\src)
	lwz s2, 8(\src)
	lwz s3,12(\src)
	lwz r7, 0(\rk)
	lwz r8, 4(\rk)
	lwz r9, 8(\rk)
	lwz r10,12(\rk)
	#else
	# error ppc little-endian not implemented
	#endif
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10
	.endm

	.macro	etfs	rk offset
	lwz t0,\offset+ 0(\rk)
	lwz t1,\offset+ 4(\rk)
	lwz t2,\offset+ 8(\rk)
	lwz t3,\offset+12(\rk)

	rlwinm r7,s0,10,22,29 /* ((s0 >> 24) & 0xff) * 4 */
	rlwinm r8,s1,10,22,29 /* ((s1 >> 24) & 0xff) * 4 */
	rlwinm r9,s2,10,22,29 /* ((s2 >> 24) & 0xff) * 4 */
	rlwinm r10,s3,10,22,29 /* ((s3 >> 24) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s1,18,22,29 /* ((s1 >> 16) & 0xff) * 4 */
	rlwinm r8,s2,18,22,29 /* ((s2 >> 16) & 0xff) * 4 */
	rlwinm r9,s3,18,22,29 /* ((s3 >> 16) & 0xff) * 4 */
	rlwinm r10,s0,18,22,29 /* ((s0 >> 16) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s2,26,22,29 /* ((s2 >> 8) & 0xff) * 4 */
	rlwinm r8,s3,26,22,29 /* ((s3 >> 8) & 0xff) * 4 */
	rlwinm r9,s0,26,22,29 /* ((s0 >> 8) & 0xff) * 4 */
	rlwinm r10,s1,26,22,29 /* ((s1 >> 8) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s3,2,22,29 /* ((s3 >> 0) & 0xff) * 4 */
	rlwinm r8,s0,2,22,29 /* ((s0 >> 0) & 0xff) * 4 */
	rlwinm r9,s1,2,22,29 /* ((s1 >> 0) & 0xff) * 4 */
	rlwinm r10,s2,2,22,29 /* ((s2 >> 0) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,-3072(r12)
	.endm

	.macro	esft	rk offset
	lwz s0,\offset+ 0(\rk)
	lwz s1,\offset+ 4(\rk)
	lwz s2,\offset+ 8(\rk)
	lwz s3,\offset+12(\rk)

	rlwinm r7,t0,10,22,29 /* ((t0 >> 24) & 0xff) * 4 */
	rlwinm r8,t1,10,22,29 /* ((t1 >> 24) & 0xff) * 4 */
	rlwinm r9,t2,10,22,29 /* ((t2 >> 24) & 0xff) * 4 */
	rlwinm r10,t3,10,22,29 /* ((s3 >> 24) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t1,18,22,29 /* ((t1 >> 16) & 0xff) * 4 */
	rlwinm r8,t2,18,22,29 /* ((t2 >> 16) & 0xff) * 4 */
	rlwinm r9,t3,18,22,29 /* ((t3 >> 16) & 0xff) * 4 */
	rlwinm r10,t0,18,22,29 /* ((t0 >> 16) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t2,26,22,29 /* ((t2 >> 8) & 0xff) * 4 */
	rlwinm r8,t3,26,22,29 /* ((t3 >> 8) & 0xff) * 4 */
	rlwinm r9,t0,26,22,29 /* ((t0 >> 8) & 0xff) * 4 */
	rlwinm r10,t1,26,22,29 /* ((t1 >> 8) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t3,2,22,29 /* ((t3 >> 0) & 0xff) * 4 */
	rlwinm r8,t0,2,22,29 /* ((t0 >> 0) & 0xff) * 4 */
	rlwinm r9,t1,2,22,29 /* ((t1 >> 0) & 0xff) * 4 */
	rlwinm r10,t2,2,22,29 /* ((t2 >> 0) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,-3072(r12)
	.endm

	.macro	elr rk
	lwz s0, 0(\rk)
	lwz s1, 4(\rk)
	lwz s2, 8(\rk)
	lwz s3,12(\rk)

	la r12,4096(r12)

	rlwinm r7,t0,10,22,29 /* ((t0 >> 24) & 0xff) * 4 */
	rlwinm r8,t1,10,22,29 /* ((t1 >> 24) & 0xff) * 4 */
	rlwinm r9,t2,10,22,29 /* ((t2 >> 24) & 0xff) * 4 */
	rlwinm r10,t3,10,22,29 /* ((t3 >> 24) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,0,7 /* & 0xff000000 */
	rlwinm r8,r8,0,0,7 /* & 0xff000000 */
	rlwinm r9,r9,0,0,7 /* & 0xff000000 */
	rlwinm r10,r10,0,0,7 /* & 0xff000000 */
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t1,18,22,29 /* ((t1 >> 16) & 0xff) * 4 */
	rlwinm r8,t2,18,22,29 /* ((t2 >> 16) & 0xff) * 4 */
	rlwinm r9,t3,18,22,29 /* ((t3 >> 16) & 0xff) * 4 */
	rlwinm r10,t0,18,22,29 /* ((t0 >> 16) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,8,15 /* & 0xff0000 */
	rlwinm r8,r8,0,8,15 /* & 0xff0000 */
	rlwinm r9,r9,0,8,15 /* & 0xff0000 */
	rlwinm r10,r10,0,8,15 /* & 0xff0000 */
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t2,26,22,29 /* ((t2 >> 8) & 0xff) * 4 */
	rlwinm r8,t3,26,22,29 /* ((t3 >> 8) & 0xff) * 4 */
	rlwinm r9,t0,26,22,29 /* ((t0 >> 8) & 0xff) * 4 */
	rlwinm r10,t1,26,22,29 /* ((t1 >> 8) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,16,23 /* & 0xff00 */
	rlwinm r8,r8,0,16,23 /* & 0xff00 */
	rlwinm r9,r9,0,16,23 /* & 0xff00 */
	rlwinm r10,r10,0,16,23 /* & 0xff00 */
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t3,2,22,29 /* ((t3 >> 0) & 0xff) * 4 */
	rlwinm r8,t0,2,22,29 /* ((t0 >> 0) & 0xff) * 4 */
	rlwinm r9,t1,2,22,29 /* ((t1 >> 0) & 0xff) * 4 */
	rlwinm r10,t2,2,22,29 /* ((t2 >> 0) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,24,31 /* & 0xff */
	rlwinm r8,r8,0,24,31 /* & 0xff */
	rlwinm r9,r9,0,24,31 /* & 0xff */
	rlwinm r10,r10,0,24,31 /* & 0xff */
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,-4096(r12)
	.endm

	.macro	eblock	rk src label
	sxrk \rk \src

	etfs \rk 16
	esft \rk 32
	etfs \rk 48
	esft \rk 64
	etfs \rk 80
	esft \rk 96
	etfs \rk 112
	esft \rk 128
	etfs \rk 144

	lwz r11,256(\rk)
	cmpwi r11,10
	beq \label

	esft \rk 160
	etfs \rk 176

	cmpwi r11,12
	beq \label

	esft \rk 192
	etfs \rk 208

\label:
	slwi r11,r11,4
	add \rk,\rk,r11

	elr \rk
	.endm

	.macro	dtfs	rk offset
	lwz t0,\offset+ 0(\rk)
	lwz t1,\offset+ 4(\rk)
	lwz t2,\offset+ 8(\rk)
	lwz t3,\offset+12(\rk)

	rlwinm r7,s0,10,22,29 /* ((s0 >> 24) & 0xff) * 4 */
	rlwinm r8,s1,10,22,29 /* ((s1 >> 24) & 0xff) * 4 */
	rlwinm r9,s2,10,22,29 /* ((s2 >> 24) & 0xff) * 4 */
	rlwinm r10,s3,10,22,29 /* ((s3 >> 24) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s3,18,22,29 /* ((s3 >> 16) & 0xff) * 4 */
	rlwinm r8,s0,18,22,29 /* ((s0 >> 16) & 0xff) * 4 */
	rlwinm r9,s1,18,22,29 /* ((s1 >> 16) & 0xff) * 4 */
	rlwinm r10,s2,18,22,29 /* ((s2 >> 16) & 0xff) * 4 */

/* start here */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s2,26,22,29 /* ((s2 >> 8) & 0xff) * 4 */
	rlwinm r8,s3,26,22,29 /* ((s3 >> 8) & 0xff) * 4 */
	rlwinm r9,s0,26,22,29 /* ((s0 >> 8) & 0xff) * 4 */
	rlwinm r10,s1,26,22,29 /* ((s1 >> 8) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,1024(r12)

	rlwinm r7,s1,2,22,29 /* ((s1 >> 0) & 0xff) * 4 */
	rlwinm r8,s2,2,22,29 /* ((s2 >> 0) & 0xff) * 4 */
	rlwinm r9,s3,2,22,29 /* ((s3 >> 0) & 0xff) * 4 */
	rlwinm r10,s0,2,22,29 /* ((s0 >> 0) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor t0,t0,r7
	xor t1,t1,r8
	xor t2,t2,r9
	xor t3,t3,r10

	la r12,-3072(r12)
	.endm

	.macro	dsft	rk offset
	lwz s0,\offset+ 0(\rk)
	lwz s1,\offset+ 4(\rk)
	lwz s2,\offset+ 8(\rk)
	lwz s3,\offset+12(\rk)

	rlwinm r7,t0,10,22,29 /* ((t0 >> 24) & 0xff) * 4 */
	rlwinm r8,t1,10,22,29 /* ((t1 >> 24) & 0xff) * 4 */
	rlwinm r9,t2,10,22,29 /* ((t2 >> 24) & 0xff) * 4 */
	rlwinm r10,t3,10,22,29 /* ((s3 >> 24) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t3,18,22,29 /* ((t3 >> 16) & 0xff) * 4 */
	rlwinm r8,t0,18,22,29 /* ((t0 >> 16) & 0xff) * 4 */
	rlwinm r9,t1,18,22,29 /* ((t1 >> 16) & 0xff) * 4 */
	rlwinm r10,t2,18,22,29 /* ((t2 >> 16) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t2,26,22,29 /* ((t2 >> 8) & 0xff) * 4 */
	rlwinm r8,t3,26,22,29 /* ((t3 >> 8) & 0xff) * 4 */
	rlwinm r9,t0,26,22,29 /* ((t0 >> 8) & 0xff) * 4 */
	rlwinm r10,t1,26,22,29 /* ((t1 >> 8) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,1024(r12)

	rlwinm r7,t1,2,22,29 /* ((t1 >> 0) & 0xff) * 4 */
	rlwinm r8,t2,2,22,29 /* ((t2 >> 0) & 0xff) * 4 */
	rlwinm r9,t3,2,22,29 /* ((t3 >> 0) & 0xff) * 4 */
	rlwinm r10,t0,2,22,29 /* ((t0 >> 0) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,-3072(r12)
	.endm

	.macro	dlr rk
	lwz s0, 0(\rk)
	lwz s1, 4(\rk)
	lwz s2, 8(\rk)
	lwz s3,12(\rk)

	la r12,4096(r12)

	rlwinm r7,t0,10,22,29 /* ((t0 >> 24) & 0xff) * 4 */
	rlwinm r8,t1,10,22,29 /* ((t1 >> 24) & 0xff) * 4 */
	rlwinm r9,t2,10,22,29 /* ((t2 >> 24) & 0xff) * 4 */
	rlwinm r10,t3,10,22,29 /* ((t3 >> 24) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,0,7 /* & 0xff000000 */
	rlwinm r8,r8,0,0,7 /* & 0xff000000 */
	rlwinm r9,r9,0,0,7 /* & 0xff000000 */
	rlwinm r10,r10,0,0,7 /* & 0xff000000 */
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t3,18,22,29 /* ((t3 >> 16) & 0xff) * 4 */
	rlwinm r8,t0,18,22,29 /* ((t0 >> 16) & 0xff) * 4 */
	rlwinm r9,t1,18,22,29 /* ((t1 >> 16) & 0xff) * 4 */
	rlwinm r10,t2,18,22,29 /* ((t2 >> 16) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,8,15 /* & 0xff0000 */
	rlwinm r8,r8,0,8,15 /* & 0xff0000 */
	rlwinm r9,r9,0,8,15 /* & 0xff0000 */
	rlwinm r10,r10,0,8,15 /* & 0xff0000 */
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t2,26,22,29 /* ((t2 >> 8) & 0xff) * 4 */
	rlwinm r8,t3,26,22,29 /* ((t3 >> 8) & 0xff) * 4 */
	rlwinm r9,t0,26,22,29 /* ((t0 >> 8) & 0xff) * 4 */
	rlwinm r10,t1,26,22,29 /* ((t1 >> 8) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,16,23 /* & 0xff00 */
	rlwinm r8,r8,0,16,23 /* & 0xff00 */
	rlwinm r9,r9,0,16,23 /* & 0xff00 */
	rlwinm r10,r10,0,16,23 /* & 0xff00 */
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	rlwinm r7,t1,2,22,29 /* ((t1 >> 0) & 0xff) * 4 */
	rlwinm r8,t2,2,22,29 /* ((t2 >> 0) & 0xff) * 4 */
	rlwinm r9,t3,2,22,29 /* ((t3 >> 0) & 0xff) * 4 */
	rlwinm r10,t0,2,22,29 /* ((t0 >> 0) & 0xff) * 4 */
	lwzx r7,r7,r12
	lwzx r8,r8,r12
	lwzx r9,r9,r12
	lwzx r10,r10,r12
	rlwinm r7,r7,0,24,31 /* & 0xff */
	rlwinm r8,r8,0,24,31 /* & 0xff */
	rlwinm r9,r9,0,24,31 /* & 0xff */
	rlwinm r10,r10,0,24,31 /* & 0xff */
	xor s0,s0,r7
	xor s1,s1,r8
	xor s2,s2,r9
	xor s3,s3,r10

	la r12,-4096(r12)
	.endm

	.macro	dblock	rk src label
	sxrk \rk \src

	dtfs \rk 16
	dsft \rk 32
	dtfs \rk 48
	dsft \rk 64
	dtfs \rk 80
	dsft \rk 96
	dtfs \rk 112
	dsft \rk 128
	dtfs \rk 144

	lwz r11,256(\rk)
	cmpwi r11,10
	beq \label

	dsft \rk 160
	dtfs \rk 176

	cmpwi r11,12
	beq \label

	dsft \rk 192
	dtfs \rk 208

\label:
	slwi r11,r11,4
	add \rk,\rk,r11

	dlr \rk
	.endm

C_FUNCTION_BEGIN(aesEncrypt)
LABEL(aesEncrypt)
	subi r1,r1,32
	stmw r24,0(r1)

	lis r12,_ae0@ha
	la r12,_ae0@l(r12)

	eblock rk=r3 src=r5 label=LOCAL(00)

	#if WORDS_BIGENDIAN
	stw s0, 0(r4)
	stw s1, 4(r4)
	stw s2, 8(r4)
	stw s3,12(r4)
	#else
	# error ppc little-endian mode not supported
	#endif

	li r3,0
	lmw r24,0(r1)
	addi r1,r1,32
	blr
C_FUNCTION_END(aesEncrypt, LOCAL(aesEncrypt_size))


C_FUNCTION_BEGIN(aesDecrypt)
LABEL(aesDecrypt)
	subi r1,r1,32
	stmw r24,0(r1)

	lis r12,_ad0@ha
	la r12,_ad0@l(r12)

	dblock rk=r3 src=r5 label=LOCAL(01)

	#if WORDS_BIGENDIAN
	stw s0, 0(r4)
	stw s1, 4(r4)
	stw s2, 8(r4)
	stw s3,12(r4)
	#else
	# error ppc little-endian mode not supported
	#endif

	li r3,0
	lmw r24,0(r1)
	addi r1,r1,32
	blr
C_FUNCTION_END(aesDecrypt, LOCAL(aesDecrypt_size))

C_FUNCTION_BEGIN(aesECBEncrypt)
LABEL(aesECBEncrypt)
	subi r1,r1,32
	stmw r24,0(r1)

	mtctr r4

	lis r12,_ae0@ha
	la r12,_ae0@l(r12)

LOCAL(02):
	/* copy r3 into r4 */
	mr r4,r3

	eblock rk=r4 src=r6 label=LOCAL(03)

	#if WORDS_BIGENDIAN
	stw s0, 0(r5)
	stw s1, 4(r5)
	stw s2, 8(r5)
	stw s3,12(r5)
	#else
	# error ppc little-endian mode not supported
	#endif

	addi r5,r5,16
	addi r6,r6,16

	bdnz LOCAL(02)

	li r3,0
	lmw r24,0(r1)
	addi r1,r1,32
	blr
C_FUNCTION_END(aesECBEncrypt, LOCAL(aesECBEncrypt_size))


C_FUNCTION_BEGIN(aesECBDecrypt)
LABEL(aesECBDecrypt)
	subi r1,r1,32
	stmw r24,0(r1)

	mtctr r4

	lis r12,_ad0@ha
	la r12,_ad0@l(r12)

LOCAL(04):
	/* copy r3 into r4 */
	mr r4,r3

	dblock rk=r4 src=r6 label=LOCAL(05)

	#if WORDS_BIGENDIAN
	stw s0, 0(r5)
	stw s1, 4(r5)
	stw s2, 8(r5)
	stw s3,12(r5)
	#else
	# error ppc little-endian mode not supported
	#endif

	addi r5,r5,16
	addi r6,r6,16

	bdnz LOCAL(04)

	li r3,0
	lmw r24,0(r1)
	addi r1,r1,32
	blr
C_FUNCTION_END(aesECBDecrypt, LOCAL(aesECBDecrypt_size))
