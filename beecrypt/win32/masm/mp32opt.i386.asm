;
; mp32opt.i386.asm
;
; Assembler optimized multiprecision integer routines for Intel 386
;
; Compile target is Microsoft Macro Assembler
;
; Copyright (c) 1998-2000 Virtual Unlimited B.V.
;
; Author: Bob Deblier <bob@virtualunlimited.com>
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

	align 8

mp32addw proc
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]
	mov eax,dword ptr [esp+16]

	lea edi,dword ptr [edi+ecx*4-4]
	add dword ptr [edi],eax
	dec ecx
	jz @addw_end
	sub edi,4
	xor edx,edx

	align 4
@addw_loop:
	adc dword ptr [edi],edx
	sub edi,4
	dec ecx
	jnz @addw_loop
@addw_end:
	sbb eax,eax
	neg eax

	pop edi
	ret
mp32addw endp


	align 8

mp32subw proc
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]
	mov eax,dword ptr [esp+16]

	lea edi,dword ptr [edi+ecx*4-4]
	sub dword ptr [edi],eax
	dec ecx
	jz @subw_end
	sub edi,4
	xor edx,edx

	align 4
@subw_loop:
	sbb dword ptr [edi],edx
	sub edi,4
	dec ecx
	jnz @subw_loop
@subw_end:
	sbb eax,eax
	neg eax

	pop edi
	ret
mp32subw endp


	align 8

mp32add proc
	push edi
	push esi
	
	mov ecx,dword ptr [esp+12]
	mov edi,dword ptr [esp+16]
	mov esi,dword ptr [esp+20]
	
	xor edx,edx
	dec ecx
		
@add_loop:
	mov eax,dword ptr [esi+ecx*4]
	adc dword ptr [edi+ecx*4],eax
	dec ecx
	jns @add_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret
mp32add endp

	align 8

mp32sub proc
	push edi
	push esi
	
	mov ecx,dword ptr [esp+12]
	mov edi,dword ptr [esp+16]
	mov esi,dword ptr [esp+20]
	
	xor edx,edx
	dec ecx
		
@sub_loop:
	mov eax,dword ptr [esi+ecx*4]
	sbb dword ptr [edi+ecx*4],eax
	dec ecx
	jns @sub_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret
mp32sub endp


	align 8

mp32multwo proc
	push edi
	
	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]

	xor eax,eax
	dec ecx
	
@multwo_loop:
	mov eax,dword ptr [edi+ecx*4]
	adc dword ptr [edi+ecx*4],eax
	dec ecx
	jns @multwo_loop
	
	sbb eax,eax
	neg eax

	pop edi
	ret
mp32multwo endp


	align 8

mp32setmul proc
	push edi
	push esi
	push ebx
	push ebp

	mov ecx,dword ptr [esp+20]
	mov edi,dword ptr [esp+24]
	mov esi,dword ptr [esp+28]
	mov ebp,dword ptr [esp+32]

	xor ebx,ebx
	dec ecx
	
	align 4
@setmul_loop:
	mov eax,dword ptr [esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	mov dword ptr [edi+ecx*4],eax
	mov ebx,edx
	dec ecx
	jns @setmul_loop

	mov eax,ebx

	pop ebp
	pop ebx
	pop esi
	pop edi
	ret
mp32setmul endp


	align 8

mp32addmul proc
	push edi
	push esi
	push ebx
	push ebp

	mov ecx,dword ptr [esp+20]
	mov edi,dword ptr [esp+24]
	mov esi,dword ptr [esp+28]
	mov ebp,dword ptr [esp+32]

	xor ebx,ebx
	dec ecx
	
	align 4
@addmul_loop:
	mov eax,dword ptr [esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	add eax,dword ptr [edi+ecx*4]
	adc edx,0
	mov dword ptr [edi+ecx*4],eax
	mov ebx,edx
	dec ecx
	jns @addmul_loop

	mov eax,ebx

	pop ebp
	pop ebx
	pop esi
	pop edi
	ret
mp32addmul endp


	align 8

mp32addsqrtrc proc
	push edi
	push esi
	push ebx

	mov ecx,dword ptr [esp+16]
	mov edi,dword ptr [esp+20]
	mov esi,dword ptr [esp+24]

	xor ebx,ebx
	dec ecx

	align 4
@addsqrtrc_loop:
	mov eax,dword ptr [esi+ecx*4]
	mul eax
	add eax,ebx
	adc edx,0
	add eax,dword ptr [edi+ecx*8+4]
	adc edx,dword ptr [edi+ecx*8+0]
	sbb ebx,ebx
	mov dword ptr [edi+ecx*8+4],eax
	mov dword ptr [edi+ecx*8+0],edx
	neg ebx
	dec ecx
	jns @addsqrtrc_loop

	mov eax,ebx

	pop ebx
	pop esi
	pop edi
	ret
mp32addsqrtrc endp

	end
