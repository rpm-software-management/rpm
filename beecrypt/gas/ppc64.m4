dnl  ppc64.m4
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

ifelse(substr(ASM_OS,0,3),aix,`
define(USE_NUMERIC_REGISTERS)
undefine(`C_FUNCTION_BEGIN')
define(C_FUNCTION_BEGIN,`
	.toc
	.globl	$1[DS]
	.csect	$1[DS]
	.llong	.$1[PR], TOC[tc0], 0
	.toc
	.globl	.$1[PR]
	.csect	.$1[PR]
')
undefine(`C_FUNCTION_END')
define(C_FUNCTION_END,`
	.tbtag 0x0,0xc,0x0,0x0,0x0,0x0,0x0,0x0
')

	.machine	"ppc64"
')

ifelse(substr(ASM_OS,0,5),linux,`
define(USE_NUMERIC_REGISTERS)
dnl trampoline definitions from glibc-2.3.2/sysdeps/powerpc/powerpc64/dl-machine.h
undefine(`C_FUNCTION_BEGIN')
define(C_FUNCTION_BEGIN,`
	.section .text
	.align	2
	.globl .$1
	.type .$1,@function
	.section ".opd","aw"
	.align 3
	.globl $1
	.size $1,24
$1:
	.quad .$1,.TOC.@tocbase,0
	.previous
.$1:
')
undefine(`C_FUNCTION_END')
define(C_FUNCTION_END,`
.LT_$1:
	.long 0
	.byte 0x00,0x0c,0x24,0x40,0x00,0x00,0x00,0x00
	.long .LT_$1 - .$1
	.short .LT_$1_name_end-.LT_$1_name_start
.LT_$1_name_start:
	.ascii "$1"
.LT_$1_name_end:
	.align 2
	.size .$1,. - .$1
	.previous
')
')

ifdef(`USE_NUMERIC_REGISTERS',`
define(r0,0)
define(r1,1)
define(r2,2)
define(r3,3)
define(r4,4)
define(r5,5)
define(r6,6)
define(r7,7)
define(r8,8)
define(r9,9)
define(r10,10)
define(r11,11)
define(r12,12)
define(r13,13)
define(r14,14)
define(r15,15)
define(r16,16)
define(r17,17)
define(r18,18)
define(r19,19)
define(r20,20)
define(r21,21)
define(r22,22)
define(r23,23)
define(r24,24)
define(r25,25)
define(r26,26)
define(r27,27)
define(r28,28)
define(r29,29)
define(r30,30)
define(r31,31)
')
