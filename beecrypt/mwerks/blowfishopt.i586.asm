#
# blowfishopt.i586.asm
#
# Assembler optimized blowfish routines for Intel Pentium processors
#
# Compile target is Metrowerks CodeWarrior Pro 5 for Windows
#
# Copyright (c) 2000 Virtual Unlimited B.V.
#
# Author: Bob Deblier <bob@virtualunlimited.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

etworounds:	.macro p
	xor ecx,[esi+p]
	rol ecx,16
	mov al,ch
	mov bl,cl
	rol ecx,16
	mov edi,[esi+eax*4+72+0x000]
	add edi,[esi+ebx*4+72+0x400]
	mov al,ch
	mov bl,cl
	xor edi,[esi+eax*4+72+0x800]
	add edi,[esi+ebx*4+72+0xC00]
	xor edx,edi
	xor edx,[esi+p+4]
	rol edx,16
	mov al,dh
	mov bl,dl
	rol edx,16
	mov edi,[esi+eax*4+72+0x000]
	add edi,[esi+ebx*4+72+0x400]
	mov al,dh
	mov bl,dl
	xor edi,[esi+eax*4+72+0x800]
	add edi,[esi+ebx*4+72+0xC00]
	xor ecx,edi
	.endm
	
dtworounds:	.macro p
	xor ecx,[esi+p+4]
	rol ecx,16
	mov al,ch
	mov bl,cl
	rol ecx,16
	mov edi,[esi+eax*4+72+0x000]
	add edi,[esi+ebx*4+72+0x400]
	mov al,ch
	mov bl,cl
	xor edi,[esi+eax*4+72+0x800]
	add edi,[esi+ebx*4+72+0xC00]
	xor edx,edi
	xor edx,[esi+p]
	rol edx,16
	mov al,dh
	mov bl,dl
	rol edx,16
	mov edi,[esi+eax*4+72+0x000]
	add edi,[esi+ebx*4+72+0x400]
	mov al,dh
	mov bl,dl
	xor edi,[esi+eax*4+72+0x800]
	add edi,[esi+ebx*4+72+0xC00]
	xor ecx,edi
	.endm

	.text

	.align 8
	.globl _blowfishEncrypt
	
_blowfishEncrypt:
	push edi
	push esi
	push ebx

	mov esi,[esp+16]
	mov edi,[esp+24]
	
	xor eax,eax
	xor ebx,ebx
	
	mov ecx,[edi]
	mov edx,[edi+4]
	
	bswap ecx
	bswap edx
	
	etworounds 0
	etworounds 8
	etworounds 16
	etworounds 24
	etworounds 32
	etworounds 40
	etworounds 48
	etworounds 56

	mov edi,[esp+20]
	xor ecx,[esi+64]
	xor edx,[esi+68]
	
	bswap ecx
	bswap edx
	
	mov [edi+4],ecx
	mov [edi],edx
	
	xor eax,eax
	
	pop ebx
	pop esi
	pop edi
	ret

	.align 8
	.globl _blowfishDecrypt

_blowfishDecrypt:
	push edi
	push esi
	push ebx

	mov esi,[esp+16]
	mov edi,[esp+24]
	
	xor eax,eax
	xor ebx,ebx
	
	mov ecx,[edi]
	mov edx,[edi+4]
	
	bswap ecx
	bswap edx
	
	dtworounds 64
	dtworounds 56
	dtworounds 48
	dtworounds 40
	dtworounds 32
	dtworounds 24
	dtworounds 16
	dtworounds 8

	mov edi,[esp+20]
	xor ecx,[esi+4]
	xor edx,[esi]
	
	bswap ecx
	bswap edx
	
	mov [edi+4],ecx
	mov [edi],edx
	
	xor eax,eax
	
	pop ebx
	pop esi
	pop edi
	ret
