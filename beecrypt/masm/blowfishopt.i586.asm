;
; blowfishopt.i586.asm
;
; Assembler optimized blowfish routines for Intel Pentium processors
;
; Compile target is Microsoft Macro Assembler
;
; Copyright (c) 2000 Virtual Unlimited B.V.
;
; Author: Bob Deblier <bob.deblier@pandora.be>
;
; This library is free software; you can redistribute it and/or
; modify it under the terms of the GNU Lesser General Public
; License as published by the Free Software Foundation; either
; version 2.1 of the License, or (at your option) any later version.
;
; This library is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; Lesser General Public License for more details.
;
; You should have received a copy of the GNU Lesser General Public
; License along with this library; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;

	.586
	.model flat,C

	.code

etworounds macro offset
	xor ecx,dword ptr [esi+offset]
	rol ecx,16
	mov al,ch
	mov bl,cl
	rol ecx,16
	mov edi,dword ptr [esi+eax*4+72+0000h]
	add edi,dword ptr [esi+ebx*4+72+0400h]
	mov al,ch
	mov bl,cl
	xor edi,dword ptr [esi+eax*4+72+0800h]
	add edi,dword ptr [esi+ebx*4+72+0C00h]
	xor edx,edi
	xor edx,dword ptr [esi+offset+4]
	rol edx,16
	mov al,dh
	mov bl,dl
	rol edx,16
	mov edi,dword ptr [esi+eax*4+72+0000h]
	add edi,dword ptr [esi+ebx*4+72+0400h]
	mov al,dh
	mov bl,dl
	xor edi,dword ptr [esi+eax*4+72+0800h]
	add edi,dword ptr [esi+ebx*4+72+0C00h]
	xor ecx,edi
	endm
	
dtworounds macro offset
	xor ecx,dword ptr [esi+offset+4]
	rol ecx,16
	mov al,ch
	mov bl,cl
	rol ecx,16
	mov edi,dword ptr [esi+eax*4+72+0000h]
	add edi,dword ptr [esi+ebx*4+72+0400h]
	mov al,ch
	mov bl,cl
	xor edi,dword ptr [esi+eax*4+72+0800h]
	add edi,dword ptr [esi+ebx*4+72+0C00h]
	xor edx,edi
	xor edx,dword ptr [esi+offset]
	rol edx,16
	mov al,dh
	mov bl,dl
	rol edx,16
	mov edi,dword ptr [esi+eax*4+72+0000h]
	add edi,dword ptr [esi+ebx*4+72+0400h]
	mov al,dh
	mov bl,dl
	xor edi,dword ptr [esi+eax*4+72+0800h]
	add edi,dword ptr [esi+ebx*4+72+0C00h]
	xor ecx,edi
	endm


	align 8
	
blowfishEncrypt proc c export
	push edi
	push esi
	push ebx

	mov esi,dword ptr [esp+16]
	mov edi,dword ptr [esp+24]
	
	xor eax,eax
	xor ebx,ebx
	
	mov ecx,dword ptr [edi]
	mov edx,dword ptr [edi+4]
	
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

	mov edi,dword ptr [esp+20]
	xor ecx,dword ptr [esi+64]
	xor edx,dword ptr [esi+68]
	
	bswap ecx
	bswap edx
	
	mov dword ptr [edi+4],ecx
	mov dword ptr [edi],edx
	
	xor eax,eax
	
	pop ebx
	pop esi
	pop edi
	ret
blowfishEncrypt endp


	align 8

blowfishDecrypt proc c export
	push edi
	push esi
	push ebx

	mov esi,dword ptr [esp+16]
	mov edi,dword ptr [esp+24]
	
	xor eax,eax
	xor ebx,ebx
	
	mov ecx,dword ptr [edi]
	mov edx,dword ptr [edi+4]
	
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

	mov edi,dword ptr [esp+20]
	xor ecx,dword ptr [esi+4]
	xor edx,dword ptr [esi]
	
	bswap ecx
	bswap edx
	
	mov dword ptr [edi+4],ecx
	mov dword ptr [edi],edx
	
	xor eax,eax
	
	pop ebx
	pop esi
	pop edi
	ret
blowfishDecrypt endp

	end
