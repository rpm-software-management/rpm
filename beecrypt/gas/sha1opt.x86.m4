dnl  sha1opt.i586.m4
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
include(ASM_SRCDIR/x86.m4)

	.equ	K00,	0x5a827999
	.equ	K20,	0x6ed9eba1
	.equ	K40,	0x8f1bbcdc
	.equ	K60,	0xca62c1d6

	.equ	PARAM_H,		0
	.equ	PARAM_DATA,		20

define(`subround1',`
	movl $2,%ecx
	movl $1,%ebx
	movl $3,%edx
	roll `$'5,%eax
	xorl %edx,%ecx
	addl $4,%eax
	andl %ebx,%ecx
	addl `$'K00,%eax
	rorl `$'2,%ebx
	addl $5(%esi,%edi),%eax
	xorl %edx,%ecx
	movl %ebx,$1
	addl %ecx,%eax
	movl %eax,$4
')

define(`subround2',`
	movl $2,%ecx
	movl $1,%ebx
	roll `$'5,%eax
	xorl %ebx,%ecx
	addl $4,%eax
	xorl $3,%ecx
	addl `$'K20,%eax
	rorl `$'2,%ebx
	addl $5(%esi,%edi),%eax
	movl %ebx,$1
	addl %ecx,%eax
	movl %eax,$4
')

define(`subround3',`
	movl $2,%ecx
	roll `$'5,%eax
	movl $1,%ebx
	movl %ecx,%edx
	addl $4,%eax
	orl %ebx,%ecx
	andl %ebx,%edx
	andl $3,%ecx
	addl `$'K40,%eax
	orl %edx,%ecx
	addl $5(%esi,%edi),%eax
	rorl `$'2,%ebx
	addl %ecx,%eax
	movl %ebx,$1
	movl %eax,$4
')

define(`subround4',`
	movl $2,%ecx
	movl $1,%ebx
	roll `$'5,%eax
	xorl %ebx,%ecx
	addl $4,%eax
	xorl $3,%ecx
	addl `$'K60,%eax
	rorl `$'2,%ebx
	addl $5(%esi,%edi),%eax
	movl %ebx,$1
	addl %ecx,%eax
	movl %eax,$4
')

C_FUNCTION_BEGIN(sha1Process)
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %ebp

	movl 20(%esp),%esi
	subl `$'20,%esp
	leal PARAM_DATA(%esi),%edi
	movl %esp,%ebp

	movl `$'4,%ecx
LOCAL(0):
	movl (%esi,%ecx,4),%edx
	movl %edx,(%ebp,%ecx,4)
	decl %ecx
	jns LOCAL(0)

	movl `$'15,%ecx

	.align 4
LOCAL(1):
	movl (%edi,%ecx,4),%edx
ifdef(`USE_BSWAP',`
	bswap %edx
',`
	movl %edx,%eax
	andl `$'0xff00ff,%edx
	rol `$'8,%eax
	andl `$'0xff00ff,%eax
	ror `$'8,%edx
	or %eax,%edx
')
	mov %edx,(%edi,%ecx,4)
	decl %ecx
	jns LOCAL(1)

	leal PARAM_DATA(%esi),%edi
	movl `$'16,%ecx
	xorl %eax,%eax

	.align 4
LOCAL(2):
	movl 52(%edi),%eax
	movl 56(%edi),%ebx
	xorl 32(%edi),%eax
	xorl 36(%edi),%ebx
	xorl 8(%edi),%eax
	xorl 12(%edi),%ebx
	xorl (%edi),%eax
	xorl 4(%edi),%ebx
	roll `$'1,%eax
	roll `$'1,%ebx
	movl %eax,64(%edi)
	movl %ebx,68(%edi)
	movl 60(%edi),%eax
	movl 64(%edi),%ebx
	xorl 40(%edi),%eax
	xorl 44(%edi),%ebx
	xorl 16(%edi),%eax
	xorl 20(%edi),%ebx
	xorl 8(%edi),%eax
	xorl 12(%edi),%ebx
	roll `$'1,%eax
	roll `$'1,%ebx
	movl %eax,72(%edi)
	movl %ebx,76(%edi)
	addl `$'16,%edi
	decl %ecx
	jnz LOCAL(2)

	movl `$'PARAM_DATA,%edi

	movl (%ebp),%eax
LOCAL(01_20):
	subround1( 4(%ebp), 8(%ebp), 12(%ebp), 16(%ebp), 0)
	subround1(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround1(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround1(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround1( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround1( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround1(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround1(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround1(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround1( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround1( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround1(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround1(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround1(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround1( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround1( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround1(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround1(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround1(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround1( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi

LOCAL(21_40):
	subround2( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround2(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround2(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround2(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround2( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround2( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround2(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround2(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround2(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround2( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround2( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround2(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround2(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround2(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround2( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround2( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround2(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround2(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround2(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround2( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi

LOCAL(41_60):
	subround3( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround3(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround3(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround3(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround3( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround3( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround3(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround3(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround3(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround3( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround3( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround3(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround3(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround3(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround3( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround3( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround3(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround3(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround3(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround3( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi

LOCAL(61_80):
	subround4( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround4(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround4(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround4(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround4( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround4( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround4(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround4(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround4(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround4( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround4( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround4(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround4(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround4(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround4( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)
	addl `$'20,%edi
	subround4( 4(%ebp),   %ebx , 12(%ebp), 16(%ebp), 0)
	subround4(  (%ebp),   %ebx ,  8(%ebp), 12(%ebp), 4)
	subround4(16(%ebp),   %ebx ,  4(%ebp),  8(%ebp), 8)
	subround4(12(%ebp),   %ebx ,   (%ebp),  4(%ebp), 12)
	subround4( 8(%ebp),   %ebx , 16(%ebp),   (%ebp), 16)

	movl `$'4,%ecx

	.align 4
LOCAL(3):
	movl (%ebp,%ecx,4),%eax
	addl %eax,(%esi,%ecx,4)
	decl %ecx
	jns LOCAL(3)

	addl `$'20,%esp
	popl %ebp
	popl %ebx
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(sha1Process)
