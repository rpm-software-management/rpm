dnl  blowfishopt.i586.m4
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

dnl during this macro we assume:
dnl	bp in %esi, xl and xr in %ecx and %edx, %eax and %ebx clear

define(`etworounds',`
	xorl $1+0(%esi),%ecx
	roll `$'16,%ecx
	movzx %ch,%eax
	movzx %cl,%ebx
	roll `$'16,%ecx
	movl 0x000+72(%esi,%eax,4),%edi
	addl 0x400+72(%esi,%ebx,4),%edi
	movzx %ch,%eax
	movzx %cl,%ebx
	xorl 0x800+72(%esi,%eax,4),%edi
	addl 0xC00+72(%esi,%ebx,4),%edi
	xorl %edi,%edx
	xorl $1+4(%esi),%edx
	roll `$'16,%edx
	movzx %dh,%eax
	movzx %dl,%ebx
	roll `$'16,%edx
	movl 0x000+72(%esi,%eax,4),%edi
	addl 0x400+72(%esi,%ebx,4),%edi
	movzx %dh,%eax
	movzx %dl,%ebx
	xorl 0x800+72(%esi,%eax,4),%edi
	addl 0xC00+72(%esi,%ebx,4),%edi
	xorl %edi,%ecx
')

dnl bp in %esi, xl and xr in %ecx and %edx, %eax and %ebx clear
define(`dtworounds',`
	xorl $1+4(%esi),%ecx
	roll `$'16,%ecx
	movzx %ch,%eax
	movzx %cl,%ebx
	roll `$'16,%ecx
	movl 0x000+72(%esi,%eax,4),%edi
	addl 0x400+72(%esi,%ebx,4),%edi
	movzx %ch,%eax
	movzx %cl,%ebx
	xorl 0x800+72(%esi,%eax,4),%edi
	addl 0xC00+72(%esi,%ebx,4),%edi
	xorl %edi,%edx
	xorl $1+0(%esi),%edx
	roll `$'16,%edx
	movzx %dh,%eax
	movzx %dl,%ebx
	roll `$'16,%edx
	movl 0x000+72(%esi,%eax,4),%edi
	addl 0x400+72(%esi,%ebx,4),%edi
	movzx %dh,%eax
	movzx %dl,%ebx
	xorl 0x800+72(%esi,%eax,4),%edi
	addl 0xC00+72(%esi,%ebx,4),%edi
	xorl %edi,%ecx
')

C_FUNCTION_BEGIN(blowfishEncrypt)
	pushl %edi
	pushl %esi
	pushl %ebx

	movl 16(%esp),%esi
	movl 24(%esp),%edi

	movl 0(%edi),%ecx
	movl 4(%edi),%edx

	bswap %ecx
	bswap %edx

	etworounds(0)
	etworounds(8)
	etworounds(16)
	etworounds(24)
	etworounds(32)
	etworounds(40)
	etworounds(48)
	etworounds(56)

	movl 20(%esp),%edi
	xorl 64(%esi),%ecx
	xorl 68(%esi),%edx

	bswap %ecx
	bswap %edx

	movl %ecx,4(%edi)
	movl %edx,0(%edi)

	xorl %eax,%eax
	popl %ebx
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(blowfishEncrypt)


C_FUNCTION_BEGIN(blowfishDecrypt)
	pushl %edi
	pushl %esi
	pushl %ebx

	movl 16(%esp),%esi
	movl 24(%esp),%edi

	movl 0(%edi),%ecx
	movl 4(%edi),%edx

	bswap %ecx
	bswap %edx
	
	dtworounds(64)
	dtworounds(56)
	dtworounds(48)
	dtworounds(40)
	dtworounds(32)
	dtworounds(24)
	dtworounds(16)
	dtworounds(8)

	movl 20(%esp),%edi
	xorl 4(%esi),%ecx
	xorl 0(%esi),%edx
	
	bswap %ecx
	bswap %edx
	
	movl %ecx,4(%edi)
	movl %edx,0(%edi)

	xorl %eax,%eax

	popl %ebx
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(blowfishDecrypt)
