/*
 * blowfishopt.i586.S
 *
 * Assembler optimized blowfish routines for Intel Pentium processors
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

#include "beecrypt.gas.h"

	.file "blowfishopt.i586.S"

	.text

	.macro  etworounds	p # bp in %esi, xl and xr in %ecx and %edx, %eax and %ebx clear
	xorl 0+\p(%esi),%ecx
	roll $16,%ecx
	movb %ch,%al
	movb %cl,%bl
	roll $16,%ecx
	movl 0x000+72(%esi,%eax,4),%edi
	addl 0x400+72(%esi,%ebx,4),%edi
	movb %ch,%al
	movb %cl,%bl
	xorl 0x800+72(%esi,%eax,4),%edi
	addl 0xC00+72(%esi,%ebx,4),%edi
	xorl %edi,%edx
	xorl 4+\p(%esi),%edx
	roll $16,%edx
	movb %dh,%al
	movb %dl,%bl
	roll $16,%edx
	movl 0x000+72(%esi,%eax,4),%edi
	addl 0x400+72(%esi,%ebx,4),%edi
	movb %dh,%al
	movb %dl,%bl
	xorl 0x800+72(%esi,%eax,4),%edi
	addl 0xC00+72(%esi,%ebx,4),%edi
	xorl %edi,%ecx
	.endm

	.macro  dtworounds	p # bp in %esi, xl and xr in %ecx and %edx, %eax and %ebx clear
	xorl 4+\p(%esi),%ecx
	roll $16,%ecx
	movb %ch,%al
	movb %cl,%bl
	roll $16,%ecx
	movl 0x000+72(%esi,%eax,4),%edi
	addl 0x400+72(%esi,%ebx,4),%edi
	movb %ch,%al
	movb %cl,%bl
	xorl 0x800+72(%esi,%eax,4),%edi
	addl 0xC00+72(%esi,%ebx,4),%edi
	xorl %edi,%edx
	xorl 0+\p(%esi),%edx
	roll $16,%edx
	movb %dh,%al
	movb %dl,%bl
	roll $16,%edx
	movl 0x000+72(%esi,%eax,4),%edi
	addl 0x400+72(%esi,%ebx,4),%edi
	movb %dh,%al
	movb %dl,%bl
	xorl 0x800+72(%esi,%eax,4),%edi
	addl 0xC00+72(%esi,%ebx,4),%edi
	xorl %edi,%ecx
	.endm

C_FUNCTION_BEGIN(blowfishEncrypt)
LABEL(blowfishEncrypt)
	/* parameter one is the blowfish parameters; need to extract bp and set it up in esi */
	pushl %edi
	pushl %esi
	pushl %ebx

	movl 16(%esp),%esi # esi now contains bp
	movl 24(%esp),%edi # edi now contains src

	xorl %eax,%eax
	xorl %ebx,%ebx

	movl 0(%edi),%ecx
	movl 4(%edi),%edx

	bswap %ecx
	bswap %edx

	etworounds p= 0
	etworounds p= 8
	etworounds p=16
	etworounds p=24
	etworounds p=32
	etworounds p=40
	etworounds p=48
	etworounds p=56

	movl 20(%esp),%edi	# edi now contains dst
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
C_FUNCTION_END(blowfishEncrypt, LOCAL(blowfishEncrypt_size))


C_FUNCTION_BEGIN(blowfishDecrypt)
LABEL(blowfishDecrypt)
	/* parameter one is the blowfish parameters; need to extract bp and set it up in ebp */
	pushl %edi
	pushl %esi
	pushl %ebx

	movl 16(%esp),%esi # esi now contains bp
	movl 24(%esp),%edi # edi now contains dst

	xorl %eax,%eax
	xorl %ebx,%ebx

	movl 0(%edi),%ecx
	movl 4(%edi),%edx

	bswap %ecx
	bswap %edx
	
	dtworounds p=64
	dtworounds p=56
	dtworounds p=48
	dtworounds p=40
	dtworounds p=32
	dtworounds p=24
	dtworounds p=16
	dtworounds p= 8

	movl 20(%esp),%edi	# edi now contains dst
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
C_FUNCTION_END(blowfishDecrypt, LOCAL(blowfishDecrypt_size))
