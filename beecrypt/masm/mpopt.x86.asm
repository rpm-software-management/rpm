;
; mpopt.x86.asm
;
; Assembler optimized multiprecision integer routines for Intel x86 processors
;
; Copyright (c) 2003 Bob Deblier
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
	.xmm
	.code

	align 8
mpzero proc c export
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]

	xor eax,eax
	rep stosd

	pop edi
	ret
mpzero endp


	align 8
mpfill proc c export
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]
	mov eax,dword ptr [esp+16]

	rep stosd

	pop edi
	ret
mpfill endp


	align 8
mpodd proc c export
	mov ecx,dword ptr [esp+4]
	mov eax,dword ptr [esp+8]
	mov eax,dword ptr [eax+ecx*4-4]
	and eax,1
	ret
mpodd endp


	align 8
mpeven proc c export
	mov ecx,dword ptr [esp+4]
	mov eax,dword ptr [esp+8]
	mov eax,dword ptr [eax+ecx*4-4]
	not eax
	and eax,1
	ret
mpeven endp


	align 8
mpaddw proc c export
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]
	mov eax,dword ptr [esp+16]

	lea edi,dword ptr [edi+ecx*4-4]
	add dword ptr [edi],eax
	dec ecx
	jz @mpaddw_end
	sub edi,4
	xor edx,edx

	align 4
@mpaddw_loop:
	adc dword ptr [edi],edx
	sub edi,4
	dec ecx
	jnz @mpaddw_loop
@mpaddw_end:
	sbb eax,eax
	neg eax

	pop edi
	ret
mpaddw endp


	align 8
mpsubw proc c export
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]
	mov eax,dword ptr [esp+16]

	lea edi,dword ptr [edi+ecx*4-4]
	sub dword ptr [edi],eax
	dec ecx
	jz @mpsubw_end
	sub edi,4
	xor edx,edx

	align 4
@mpsubw_loop:
	sbb dword ptr [edi],edx
	sub edi,4
	dec ecx
	jnz @mpsubw_loop
@mpsubw_end:
	sbb eax,eax
	neg eax

	pop edi
	ret
mpsubw endp


	align 8
mpadd proc c export
	push edi
	push esi
	
	mov ecx,dword ptr [esp+12]
	mov edi,dword ptr [esp+16]
	mov esi,dword ptr [esp+20]

	xor edx,edx
	dec ecx

	align 4
@mpadd_loop:
	mov eax,dword ptr [esi+ecx*4]
	mov edx,dword ptr [edi+ecx*4]
	adc edx,eax
	mov dword ptr [edi+ecx*4],edx
	dec ecx
	jns @mpadd_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret
mpadd endp


	align 8
mpsub proc c export
	push edi
	push esi
	
	mov ecx,dword ptr [esp+12]
	mov edi,dword ptr [esp+16]
	mov esi,dword ptr [esp+20]

	xor edx,edx
	dec ecx

	align 4
@mpsub_loop:
	mov eax,dword ptr [esi+ecx*4]
	mov edx,dword ptr [edi+ecx*4]
	sbb edx,eax
	mov dword ptr [edi+ecx*4],edx
	dec ecx
	jns @mpsub_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret
mpsub endp


	align 8
mpdivtwo proc c export
	push edi

	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]

	lea edi,dword ptr [edi+ecx*4]
	neg ecx
	clc

@mpdivtwo_loop:
	rcr dword ptr [edi+ecx*4],1
	inc ecx
	jnz @mpdivtwo_loop
	
	pop edi
	ret
mpdivtwo endp


	align 8
mpmultwo proc c export
	push edi
	
	mov ecx,dword ptr [esp+8]
	mov edi,dword ptr [esp+12]

	dec ecx
	clc

	align 4
@mpmultwo_loop:
	mov eax,dword ptr [edi+ecx*4]
	adc eax,eax
	mov dword ptr [edi+ecx*4],eax
	dec ecx
	jns @mpmultwo_loop
	
	sbb eax,eax
	neg eax

	pop edi
	ret
mpmultwo endp


	align 8
mpsetmul proc c export
	push edi
	push esi

	ifdef USE_SSE2

	mov ecx,dword ptr [esp+12]
	mov edi,dword ptr [esp+16]
	mov esi,dword ptr [esp+20]
	movd mm1,dword ptr [esp+24]

	pxor mm0,mm0
	dec ecx

	align 4
@mpsetmul_loop:
	movd mm2,dword ptr [esi+ecx*4]
	pmuludq mm2,mm1
	paddq mm0,mm2
	movd dword ptr [edi+ecx*4],mm0
	dec ecx
	psrlq mm0,32
	jns @mpsetmul_loop

	movd eax,mm0
	emms

	else

	push ebx
	push ebp

	mov ecx,dword ptr [esp+20]
	mov edi,dword ptr [esp+24]
	mov esi,dword ptr [esp+28]
	mov ebp,dword ptr [esp+32]

	xor edx,edx
	dec ecx
	
	align 4
@mpsetmul_loop:
	mov ebx,edx
	mov eax,dword ptr [esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	mov dword ptr [edi+ecx*4],eax
	dec ecx
	jns @mpsetmul_loop

	mov eax,edx

	pop ebp
	pop ebx

	endif

	pop esi
	pop edi
	ret
mpsetmul endp


	align 8
mpaddmul proc c export
	push edi
	push esi

	ifdef USE_SSE2

	mov ecx,dword ptr [esp+12]
	mov edi,dword ptr [esp+16]
	mov esi,dword ptr [esp+20]
	movd mm1,dword ptr [esp+24]

	pxor mm0,mm0
	dec ecx

@mpaddmul_loop:
	movd mm2,dword ptr [esi+ecx*4]
	pmuludq mm2,mm1
	movd mm3,dword ptr [edi+ecx*4]
	paddq mm3,mm2
	paddq mm0,mm3
	movd dword ptr [edi+ecx*4],mm0
	dec ecx
	psrlq mm0,32
	jns @mpaddmul_loop

	movd eax,mm0
	emms

	else

	push ebx
	push ebp

	mov ecx,dword ptr [esp+20]
	mov edi,dword ptr [esp+24]
	mov esi,dword ptr [esp+28]
	mov ebp,dword ptr [esp+32]

	xor edx,edx
	dec ecx

	align 4
@mpaddmul_loop:
	mov ebx,edx
	mov eax,dword ptr [esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	add dword ptr [edi+ecx*4],eax
	adc edx,0
	dec ecx
	jns @mpaddmul_loop

	mov eax,edx

	pop ebp
	pop ebx

	endif

	pop esi
	pop edi
	ret
mpaddmul endp


	align 8
mpaddsqrtrc proc c export
	push edi
	push esi

	ifdef USE_SSE2
	mov ecx,dword ptr [esp+12]
	mov edi,dword ptr [esp+16]
	mov esi,dword ptr [esp+20]

	pxor mm0,mm0
	dec ecx

	align 4
@mpaddsqrtrc_loop:
	movd mm2,dword ptr [esi+ecx*4]
	pmuludq mm2,mm2
	movd mm3,dword ptr [edi+ecx*8+4]
	paddq mm3,mm2
	movd mm4,dword ptr [edi+ecx*8+0]
	paddq mm0,mm3
	movd dword ptr [edi+ecx*8+4],mm0
	psrlq mm0,32
	paddq mm0,mm4
	movd dword ptr [edi+ecx*8+0],mm0
	psrlq mm0,32
	dec ecx
	jns @mpaddsqrtrc_loop

	movd eax,mm0
	emms

	else
	
	push ebx

	mov ecx,dword ptr [esp+16]
	mov edi,dword ptr [esp+20]
	mov esi,dword ptr [esp+24]

	xor ebx,ebx
	dec ecx

	align 4
@mpaddsqrtrc_loop:
	mov eax,dword ptr [esi+ecx*4]
	mul eax
	add eax,ebx
	adc edx,0
	add dword ptr [edi+ecx*8+4],eax
	adc dword ptr [edi+ecx*8+0],edx
	sbb ebx,ebx
	neg ebx
	dec ecx
	jns @mpaddsqrtrc_loop

	mov eax,ebx

	pop ebx
	
	endif

	pop esi
	pop edi
	ret
mpaddsqrtrc endp

	end
