;
; mp32opt.i386.asm
;
; Assembler optimized multiprecision integer routines for Intel 386
;
; Compile target is Microsoft Macro Assembler
;
; Copyright (c) 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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
mp32zero proc
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]

	xor eax,eax
	rep stosd

	pop edi
	ret
mp32zero endp


	align 8
mp32fill proc
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]
	mov eax,dword ptr [esp+16]

	rep stosd

	pop edi
	ret
mp32fill endp


	align 8
mp32odd proc
	mov ecx,dword ptr [esp+4]
	mov eax,dword ptr [esp+8]
	mov eax,dword ptr [eax+ecx*4-4]
	and eax,1
	ret
mp32odd endp


	align 8
mp32even proc
	mov ecx,dword ptr [esp+4]
	mov eax,dword ptr [esp+8]
	mov eax,dword ptr [eax+ecx*4-4]
	not eax
	and eax,1
	ret
mp32even endp


	align 8
mp32addw proc
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]
	mov eax,dword ptr [esp+16]

	lea edi,dword ptr [edi+ecx*4-4]
	add dword ptr [edi],eax
	dec ecx
	jz @mp32addw_end
	sub edi,4
	xor edx,edx

	align 4
@mp32addw_loop:
	adc dword ptr [edi],edx
	sub edi,4
	dec ecx
	jnz @mp32addw_loop
@mp32addw_end:
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
	jz @mp32subw_end
	sub edi,4
	xor edx,edx

	align 4
@mp32subw_loop:
	sbb dword ptr [edi],edx
	sub edi,4
	dec ecx
	jnz @mp32subw_loop
@mp32subw_end:
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

	align 4
@mp32add_loop:
	mov eax,dword ptr [esi+ecx*4]
	adc dword ptr [edi+ecx*4],eax
	dec ecx
	jns @mp32add_loop
	
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

	align 4
@mp32sub_loop:
	mov eax,dword ptr [esi+ecx*4]
	sbb dword ptr [edi+ecx*4],eax
	dec ecx
	jns @mp32sub_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret
mp32sub endp

	
	align 8
mp32divtwo proc
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]

	lea edi,dword ptr [edi+ecx*4]
	neg ecx
	clc
@mp32divtwo_loop:
	rcr dword ptr [edi+ecx*4],1
	inc ecx
	jnz @mp32divtwo_loop
	
	pop edi
	ret
mp32divtwo endp


	align 8
mp32multwo proc
	push edi
	
	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]

	clc
	dec ecx

	align 4
@mp32multwo_loop:
	rcl dword ptr [edi+ecx*4],1
	dec ecx
	jns @mp32multwo_loop
	
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

	xor edx,edx
	dec ecx
	
	align 4
@mp32setmul_loop:
	mov ebx,edx
	mov eax,dword ptr [esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	mov dword ptr [edi+ecx*4],eax
	dec ecx
	jns @mp32setmul_loop

	mov eax,edx

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

	xor edx,edx
	dec ecx

	align 4
@mp32addmul_loop:
	mov ebx,edx
	mov eax,dword ptr [esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	add dword ptr [edi+ecx*4],eax
	adc edx,0
	dec ecx
	jns @mp32addmul_loop

	mov eax,edx

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
@mp32addsqrtrc_loop:
	mov eax,dword ptr [esi+ecx*4]
	mul eax
	add eax,ebx
	adc edx,0
	add dword ptr [edi+ecx*8+4],eax
	adc dword ptr [edi+ecx*8+0],edx
	sbb ebx,ebx
	neg ebx
	dec ecx
	jns @mp32addsqrtrc_loop

	mov eax,ebx

	pop ebx
	pop esi
	pop edi
	ret
mp32addsqrtrc endp

	end
