#
# mp32opt.i386.asm
#
# Assembler optimized multiprecision integer routines for Intel 386
#
# Compile target is MetroWerks CodeWarrior Pro 5 for Windows
#
# Copyright (c) 1998-2000 Virtual Unlimited B.V.
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

	.text

	.align 4
	.globl _mp32addw

_mp32addw:
	push edi

	mov ecx,[esp+8]
	mov edi,[esp+12]
	mov eax,[esp+16]

	lea edi,[edi+ecx*4-4]
	add [edi],eax
	dec ecx
	jz @addw_end
	sub edi,4
	xor edx,edx

@addw_loop:
	adc [edi],edx
	sub edi,4
	dec ecx
	jnz @addw_loop
@addw_end:
	sbb eax,eax
	neg eax

	pop edi
	ret

	.align 4
	.globl _mp32subw

_mp32subw:
	push edi

	mov ecx,[esp+8]
	mov edi,[esp+12]
	mov eax,[esp+16]

	lea edi,[edi+ecx*4-4]
	sub [edi],eax
	dec ecx
	jz @subw_end
	sub edi,4
	xor edx,edx

@subw_loop:
	sbb [edi],edx
	sub edi,4
	dec ecx
	jnz @subw_loop
@subw_end:
	sbb eax,eax
	neg eax

	pop edi
	ret

	.align 4
	.globl _mp32add

_mp32add:
	push edi
	push esi
	
	mov ecx,[esp+12]
	mov edi,[esp+16]
	mov esi,[esp+20]
	
	xor edx,edx
	dec ecx
		
@add_loop:
	mov eax,[esi+ecx*4]
	adc [edi+ecx*4],eax
	dec ecx
	jns @add_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret

	.align 4
	.globl _mp32sub

_mp32sub:
	push edi
	push esi
	
	mov ecx,[esp+12]
	mov edi,[esp+16]
	mov esi,[esp+20]
	
	xor edx,edx
	dec ecx
		
@sub_loop:
	mov eax,[esi+ecx*4]
	sbb [edi+ecx*4],eax
	dec ecx
	jns @sub_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret

	.align 4
	.globl _mp32multwo

_mp32multwo:
	push edi
	
	mov ecx,[esp+8]
	mov edi,[esp+12]

	xor eax,eax
	dec ecx
	
@multwo_loop:
	mov eax,[edi+ecx*4]
	adc [edi+ecx*4],eax
	dec ecx
	jns @multwo_loop
	
	sbb eax,eax
	neg eax

	pop edi
	ret

	.align 4
	.globl _mp32setmul

_mp32setmul:
	push edi
	push esi
	push ebx
	push ebp

	mov ecx,[esp+20]
	mov edi,[esp+24]
	mov esi,[esp+28]
	mov ebp,[esp+32]

	xor ebx,ebx
	dec ecx
	
	.align 4
@setmul_loop:
	mov eax,[esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	mov [edi+ecx*4],eax
	mov ebx,edx
	dec ecx
	jns @setmul_loop

	mov eax,ebx

	pop ebp
	pop ebx
	pop esi
	pop edi
	ret


	.align 4
	.globl _mp32addmul

_mp32addmul:
	push edi
	push esi
	push ebx
	push ebp

	mov ecx,[esp+20]
	mov edi,[esp+24]
	mov esi,[esp+28]
	mov ebp,[esp+32]

	xor ebx,ebx
	dec ecx
	
	.align 4
@addmul_loop:
	mov eax,[esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	add eax,[edi+ecx*4]
	adc edx,0
	mov [edi+ecx*4],eax
	mov ebx,edx
	dec ecx
	jns @addmul_loop

	mov eax,ebx

	pop ebp
	pop ebx
	pop esi
	pop edi
	ret

	.align 4
	.globl _mp32addsqrtrc

_mp32addsqrtrc:
	push edi
	push esi
	push ebx

	mov ecx,[esp+16]
	mov edi,[esp+20]
	mov esi,[esp+24]

	xor ebx,ebx
	dec ecx

	.align 4
@addsqrtrc_loop:
	mov eax,[esi+ecx*4]
	mul eax
	add eax,ebx
	adc edx,0
	add eax,[edi+ecx*8+4]
	adc edx,[edi+ecx*8+0]
	sbb ebx,ebx
	mov [edi+ecx*8+4],eax
	mov [edi+ecx*8+0],edx
	neg ebx
	dec ecx
	jns @addsqrtrc_loop

	mov eax,ebx

	pop ebx
	pop esi
	pop edi
	ret