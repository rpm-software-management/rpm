dnl  ppc.m4
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
	.long	.$1[PR], TOC[tc0], 0
	.toc
	.globl	.$1[PR]
	.csect	.$1[PR]
')
undefine(`C_FUNCTION_END')
define(C_FUNCTION_END,`
	.tbtag 0x0,0xc,0x0,0x0,0x0,0x0,0x0,0x0
')
define(LOAD_ADDRESS,`
	lwz $2,L$1(r2)
')
define(EXTERNAL_VARIABLE,`
	.toc
L$1:
	.tc $1[TC],$1[RW]
')
	.machine	"ppc"
')

ifelse(substr(ASM_OS,0,6),darwin,`
define(LOAD_ADDRESS,`
	lis $2,hi16($1)
	la $2,lo16($1)($2)
')
define(EXTERNAL_VARIABLE)
')

ifelse(substr(ASM_OS,0,5),linux,`
define(USE_NUMERIC_REGISTERS)
define(LOAD_ADDRESS,`
	lis $2,$1@ha
	la $2,$1@l($2)
')
define(EXTERNAL_VARIABLE)
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
