dnl  aesopt.i586.m4
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

define(`sxrk',`
	movl   (%esi),%eax
	movl  4(%esi),%ebx
	movl  8(%esi),%ecx
	movl 12(%esi),%edx
	bswap %eax
	bswap %ebx
	bswap %ecx
	bswap %edx
	xorl   (%ebp),%eax
	xorl  4(%ebp),%ebx
	xorl  8(%ebp),%ecx
	xorl 12(%ebp),%edx
	movl %eax,  (%esp)
	movl %ebx, 4(%esp)
	movl %ecx, 8(%esp)
	movl %edx,12(%esp)
')

define(`etfs',`
	movl $1+0(%ebp),%ecx
	movl $1+4(%ebp),%edx

	movzbl  3(%esp),%eax
	movzbl  7(%esp),%ebx
	xorl SYMNAME(_ae0)(,%eax,4),%ecx
	xorl SYMNAME(_ae0)(,%ebx,4),%edx

	movzbl  6(%esp),%eax
	movzbl 10(%esp),%ebx
	xorl SYMNAME(_ae1)(,%eax,4),%ecx
	xorl SYMNAME(_ae1)(,%ebx,4),%edx

	movzbl  9(%esp),%eax
	movzbl 13(%esp),%ebx
	xorl SYMNAME(_ae2)(,%eax,4),%ecx
	xorl SYMNAME(_ae2)(,%ebx,4),%edx

	movzbl 12(%esp),%eax
	movzbl   (%esp),%ebx
	xorl SYMNAME(_ae3)(,%eax,4),%ecx
	xorl SYMNAME(_ae3)(,%ebx,4),%edx

	movl %ecx,16(%esp)
	movl %edx,20(%esp)

	movl $1+ 8(%ebp),%ecx
	movl $1+12(%ebp),%edx

	movzbl 11(%esp),%eax
	movzbl 15(%esp),%ebx
	xorl SYMNAME(_ae0)(,%eax,4),%ecx
	xorl SYMNAME(_ae0)(,%ebx,4),%edx

	movzbl 14(%esp),%eax
	movzbl  2(%esp),%ebx
	xorl SYMNAME(_ae1)(,%eax,4),%ecx
	xorl SYMNAME(_ae1)(,%ebx,4),%edx

	movzbl  1(%esp),%eax
	movzbl  5(%esp),%ebx
	xorl SYMNAME(_ae2)(,%eax,4),%ecx
	xorl SYMNAME(_ae2)(,%ebx,4),%edx

	movzbl  4(%esp),%eax
	movzbl  8(%esp),%ebx
	xorl SYMNAME(_ae3)(,%eax,4),%ecx
	xorl SYMNAME(_ae3)(,%ebx,4),%edx

	movl %ecx,24(%esp)
	movl %edx,28(%esp)
')

define(`esft',`
	movl $1+0(%ebp),%ecx
	movl $1+4(%ebp),%edx

	movzbl 19(%esp),%eax
	movzbl 23(%esp),%ebx
	xorl SYMNAME(_ae0)(,%eax,4),%ecx
	xorl SYMNAME(_ae0)(,%ebx,4),%edx

	movzbl 22(%esp),%eax
	movzbl 26(%esp),%ebx
	xorl SYMNAME(_ae1)(,%eax,4),%ecx
	xorl SYMNAME(_ae1)(,%ebx,4),%edx

	movzbl 25(%esp),%eax
	movzbl 29(%esp),%ebx
	xorl SYMNAME(_ae2)(,%eax,4),%ecx
	xorl SYMNAME(_ae2)(,%ebx,4),%edx

	movzbl 28(%esp),%eax
	movzbl 16(%esp),%ebx
	xorl SYMNAME(_ae3)(,%eax,4),%ecx
	xorl SYMNAME(_ae3)(,%ebx,4),%edx

	movl %ecx,  (%esp)
	movl %edx, 4(%esp)

	movl $1+ 8(%ebp),%ecx
	movl $1+12(%ebp),%edx

	movzbl 27(%esp),%eax
	movzbl 31(%esp),%ebx
	xorl SYMNAME(_ae0)(,%eax,4),%ecx
	xorl SYMNAME(_ae0)(,%ebx,4),%edx

	movzbl 30(%esp),%eax
	movzbl 18(%esp),%ebx
	xorl SYMNAME(_ae1)(,%eax,4),%ecx
	xorl SYMNAME(_ae1)(,%ebx,4),%edx

	movzbl 17(%esp),%eax
	movzbl 21(%esp),%ebx
	xorl SYMNAME(_ae2)(,%eax,4),%ecx
	xorl SYMNAME(_ae2)(,%ebx,4),%edx

	movzbl 20(%esp),%eax
	movzbl 24(%esp),%ebx
	xorl SYMNAME(_ae3)(,%eax,4),%ecx
	xorl SYMNAME(_ae3)(,%ebx,4),%edx

	movl %ecx, 8(%esp)
	movl %edx,12(%esp)
')

define(`elr',`
	movl 0(%ebp),%ecx
	movl 4(%ebp),%edx

	movzbl 19(%esp),%eax
	movzbl 23(%esp),%ebx
	movl SYMNAME(_ae4)(,%eax,4),%eax
	movl SYMNAME(_ae4)(,%ebx,4),%ebx
	andl `$'0xff000000,%eax
	andl `$'0xff000000,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 22(%esp),%eax
	movzbl 26(%esp),%ebx
	movl SYMNAME(_ae4)(,%eax,4),%eax
	movl SYMNAME(_ae4)(,%ebx,4),%ebx
	andl `$'0xff0000,%eax
	andl `$'0xff0000,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 25(%esp),%eax
	movzbl 29(%esp),%ebx
	movl SYMNAME(_ae4)(,%eax,4),%eax
	movl SYMNAME(_ae4)(,%ebx,4),%ebx
	andl `$'0xff00,%eax
	andl `$'0xff00,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 28(%esp),%eax
	movzbl 16(%esp),%ebx
	movl SYMNAME(_ae4)(,%eax,4),%eax
	movl SYMNAME(_ae4)(,%ebx,4),%ebx
	andl `$'0xff,%eax
	andl `$'0xff,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movl %ecx,  (%esp)
	movl %edx, 4(%esp)

	movl  8(%ebp),%ecx
	movl 12(%ebp),%edx

	movzbl 27(%esp),%eax
	movzbl 31(%esp),%ebx
	movl SYMNAME(_ae4)(,%eax,4),%eax
	movl SYMNAME(_ae4)(,%ebx,4),%ebx
	andl `$'0xff000000,%eax
	andl `$'0xff000000,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 30(%esp),%eax
	movzbl 18(%esp),%ebx
	movl SYMNAME(_ae4)(,%eax,4),%eax
	movl SYMNAME(_ae4)(,%ebx,4),%ebx
	andl `$'0xff0000,%eax
	andl `$'0xff0000,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 17(%esp),%eax
	movzbl 21(%esp),%ebx
	movl SYMNAME(_ae4)(,%eax,4),%eax
	movl SYMNAME(_ae4)(,%ebx,4),%ebx
	andl `$'0xff00,%eax
	andl `$'0xff00,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 20(%esp),%eax
	movzbl 24(%esp),%ebx
	movl SYMNAME(_ae4)(,%eax,4),%eax
	movl SYMNAME(_ae4)(,%ebx,4),%ebx
	andl `$'0xff,%eax
	andl `$'0xff,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movl %ecx, 8(%esp)
	movl %edx,12(%esp)
')

define(`eblock',`
	sxrk

	etfs(16)
	esft(32)
	etfs(48)
	esft(64)
	etfs(80)
	esft(96)
	etfs(112)
	esft(128)
	etfs(144)

	movl 256(%ebp),%eax
	cmp `$'10,%eax
	je $1

	esft(160)
	etfs(176)

	movl 256(%ebp),%eax
	cmp `$'12,%eax
	je $1

	esft(192)
	etfs(208)

	movl 256(%ebp),%eax

	.align 4
$1:
	sall `$'4,%eax
	addl %eax,%ebp

	elr
')

define(`dtfs',`
	movl $1+0(%ebp),%ecx
	movl $1+4(%ebp),%edx

	movzbl  3(%esp),%eax
	movzbl  7(%esp),%ebx
	xorl SYMNAME(_ad0)(,%eax,4),%ecx
	xorl SYMNAME(_ad0)(,%ebx,4),%edx

	movzbl 14(%esp),%eax
	movzbl  2(%esp),%ebx
	xorl SYMNAME(_ad1)(,%eax,4),%ecx
	xorl SYMNAME(_ad1)(,%ebx,4),%edx

	movzbl  9(%esp),%eax
	movzbl 13(%esp),%ebx
	xorl SYMNAME(_ad2)(,%eax,4),%ecx
	xorl SYMNAME(_ad2)(,%ebx,4),%edx

	movzbl  4(%esp),%eax
	movzbl  8(%esp),%ebx
	xorl SYMNAME(_ad3)(,%eax,4),%ecx
	xorl SYMNAME(_ad3)(,%ebx,4),%edx

	movl %ecx,16(%esp)
	movl %edx,20(%esp)

	movl $1+ 8(%ebp),%ecx
	movl $1+12(%ebp),%edx

	movzbl 11(%esp),%eax
	movzbl 15(%esp),%ebx
	xorl SYMNAME(_ad0)(,%eax,4),%ecx
	xorl SYMNAME(_ad0)(,%ebx,4),%edx

	movzbl  6(%esp),%eax
	movzbl 10(%esp),%ebx
	xorl SYMNAME(_ad1)(,%eax,4),%ecx
	xorl SYMNAME(_ad1)(,%ebx,4),%edx

	movzbl  1(%esp),%eax
	movzbl  5(%esp),%ebx
	xorl SYMNAME(_ad2)(,%eax,4),%ecx
	xorl SYMNAME(_ad2)(,%ebx,4),%edx

	movzbl 12(%esp),%eax
	movzbl   (%esp),%ebx
	xorl SYMNAME(_ad3)(,%eax,4),%ecx
	xorl SYMNAME(_ad3)(,%ebx,4),%edx

	movl %ecx,24(%esp)
	movl %edx,28(%esp)
')

define(`dsft',`
	movl $1+0(%ebp),%ecx
	movl $1+4(%ebp),%edx

	movzbl 19(%esp),%eax
	movzbl 23(%esp),%ebx
	xorl SYMNAME(_ad0)(,%eax,4),%ecx
	xorl SYMNAME(_ad0)(,%ebx,4),%edx

	movzbl 30(%esp),%eax
	movzbl 18(%esp),%ebx
	xorl SYMNAME(_ad1)(,%eax,4),%ecx
	xorl SYMNAME(_ad1)(,%ebx,4),%edx

	movzbl 25(%esp),%eax
	movzbl 29(%esp),%ebx
	xorl SYMNAME(_ad2)(,%eax,4),%ecx
	xorl SYMNAME(_ad2)(,%ebx,4),%edx

	movzbl 20(%esp),%eax
	movzbl 24(%esp),%ebx
	xorl SYMNAME(_ad3)(,%eax,4),%ecx
	xorl SYMNAME(_ad3)(,%ebx,4),%edx

	movl %ecx,  (%esp)
	movl %edx, 4(%esp)

	movl $1+ 8(%ebp),%ecx
	movl $1+12(%ebp),%edx

	movzbl 27(%esp),%eax
	movzbl 31(%esp),%ebx
	xorl SYMNAME(_ad0)(,%eax,4),%ecx
	xorl SYMNAME(_ad0)(,%ebx,4),%edx

	movzbl 22(%esp),%eax
	movzbl 26(%esp),%ebx
	xorl SYMNAME(_ad1)(,%eax,4),%ecx
	xorl SYMNAME(_ad1)(,%ebx,4),%edx

	movzbl 17(%esp),%eax
	movzbl 21(%esp),%ebx
	xorl SYMNAME(_ad2)(,%eax,4),%ecx
	xorl SYMNAME(_ad2)(,%ebx,4),%edx

	movzbl 28(%esp),%eax
	movzbl 16(%esp),%ebx
	xorl SYMNAME(_ad3)(,%eax,4),%ecx
	xorl SYMNAME(_ad3)(,%ebx,4),%edx

	movl %ecx, 8(%esp)
	movl %edx,12(%esp)
')

define(`dlr',`
	movl 0(%ebp),%ecx
	movl 4(%ebp),%edx

	movzbl 19(%esp),%eax
	movzbl 23(%esp),%ebx
	movl SYMNAME(_ad4)(,%eax,4),%eax
	movl SYMNAME(_ad4)(,%ebx,4),%ebx
	andl `$'0xff000000,%eax
	andl `$'0xff000000,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 30(%esp),%eax
	movzbl 18(%esp),%ebx
	movl SYMNAME(_ad4)(,%eax,4),%eax
	movl SYMNAME(_ad4)(,%ebx,4),%ebx
	andl `$'0xff0000,%eax
	andl `$'0xff0000,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 25(%esp),%eax
	movzbl 29(%esp),%ebx
	movl SYMNAME(_ad4)(,%eax,4),%eax
	movl SYMNAME(_ad4)(,%ebx,4),%ebx
	andl `$'0xff00,%eax
	andl `$'0xff00,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 20(%esp),%eax
	movzbl 24(%esp),%ebx
	movl SYMNAME(_ad4)(,%eax,4),%eax
	movl SYMNAME(_ad4)(,%ebx,4),%ebx
	andl `$'0xff,%eax
	andl `$'0xff,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movl %ecx,  (%esp)
	movl %edx, 4(%esp)

	movl  8(%ebp),%ecx
	movl 12(%ebp),%edx

	movzbl 27(%esp),%eax
	movzbl 31(%esp),%ebx
	movl SYMNAME(_ad4)(,%eax,4),%eax
	movl SYMNAME(_ad4)(,%ebx,4),%ebx
	andl `$'0xff000000,%eax
	andl `$'0xff000000,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 22(%esp),%eax
	movzbl 26(%esp),%ebx
	movl SYMNAME(_ad4)(,%eax,4),%eax
	movl SYMNAME(_ad4)(,%ebx,4),%ebx
	andl `$'0xff0000,%eax
	andl `$'0xff0000,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 17(%esp),%eax
	movzbl 21(%esp),%ebx
	movl SYMNAME(_ad4)(,%eax,4),%eax
	movl SYMNAME(_ad4)(,%ebx,4),%ebx
	andl `$'0xff00,%eax
	andl `$'0xff00,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movzbl 28(%esp),%eax
	movzbl 16(%esp),%ebx
	movl SYMNAME(_ad4)(,%eax,4),%eax
	movl SYMNAME(_ad4)(,%ebx,4),%ebx
	andl `$'0xff,%eax
	andl `$'0xff,%ebx
	xorl %eax,%ecx
	xorl %ebx,%edx

	movl %ecx, 8(%esp)
	movl %edx,12(%esp)
')

define(`dblock',`
	sxrk

	dtfs(16)
	dsft(32)
	dtfs(48)
	dsft(64)
	dtfs(80)
	dsft(96)
	dtfs(112)
	dsft(128)
	dtfs(144)

	movl 256(%ebp),%eax
	cmp `$'10,%eax
	je $1

	dsft(160)
	dtfs(176)

	movl 256(%ebp),%eax
	cmp `$'12,%eax
	je $1

	dsft(192)
	dtfs(208)

	movl 256(%ebp),%eax

	.align 4
$1:
	sall `$'4,%eax
	addl %eax,%ebp

	dlr
')

C_FUNCTION_BEGIN(aesEncrypt)
	pushl %edi
	pushl %esi
	pushl %ebp
	pushl %ebx

	movl 20(%esp),%ebp
	movl 24(%esp),%edi
	movl 28(%esp),%esi

	subl `$'32,%esp

	eblock(LOCAL(00))

	movl   (%esp),%eax
	movl  4(%esp),%ebx
	movl  8(%esp),%ecx
	movl 12(%esp),%edx
	bswap %eax
	bswap %ebx
	bswap %ecx
	bswap %edx
	movl %eax,  (%edi)
	movl %ebx, 4(%edi)
	movl %ecx, 8(%edi)
	movl %edx,12(%edi)

	addl `$'32,%esp

	xorl %eax,%eax

	popl %ebx
	popl %ebp
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(aesEncrypt)


C_FUNCTION_BEGIN(aesDecrypt)
	pushl %edi
	pushl %esi
	pushl %ebp
	pushl %ebx

	movl 20(%esp),%ebp
	movl 24(%esp),%edi
	movl 28(%esp),%esi

	subl `$'32,%esp

	dblock(LOCAL(01))

	movl   (%esp),%eax
	movl  4(%esp),%ebx
	movl  8(%esp),%ecx
	movl 12(%esp),%edx
	bswap %eax
	bswap %ebx
	bswap %ecx
	bswap %edx
	movl %eax,  (%edi)
	movl %ebx, 4(%edi)
	movl %ecx, 8(%edi)
	movl %edx,12(%edi)

	addl `$'32,%esp

	xorl %eax,%eax

	popl %ebx
	popl %ebp
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(aesDecrypt)
