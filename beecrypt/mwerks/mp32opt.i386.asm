#
# mp32opt.i386.asm
#
# Assembler optimized multiprecision integer routines for Intel 386
#
# Compile target is MetroWerks CodeWarrior Pro 5 for Windows
#
# Copyright (c) 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

	.align 8
	.globl _mp32zero
_mp32zero:
	push edi

	mov ecx,[esp+8]
	mov edi,[esp+12]

	xor eax,eax
	rep stosd

	pop edi
	ret


	.align 8
	.globl _mp32fill
_mp32fill:
	push edi

	mov ecx,[esp+8]
	mov edi,[esp+12]
	mov eax,[esp+16]

	rep stosd

	pop edi
	ret


	.align 8
	.globl _mp32odd
_mp32odd:
	mov ecx,[esp+4]
	mov eax,[esp+8]
	mov eax,[eax+ecx*4-4]
	and eax,1
	ret


	.align 8
	.globl _mp32even
_mp32even:
	mov ecx,[esp+4]
	mov eax,[esp+8]
	mov eax,[eax+ecx*4-4]
	not eax
	and eax,1
	ret


	.align 8
	.globl _mp32addw
_mp32addw:
	push edi

	mov ecx,[esp+8]
	mov edi,[esp+12]
	mov eax,[esp+16]

	lea edi,[edi+ecx*4-4]
	add [edi],eax
	dec ecx
	jz @mp32addw_end
	sub edi,4
	xor edx,edx

	.align 4
@mp32addw_loop:
	adc [edi],edx
	sub edi,4
	dec ecx
	jnz @mp32addw_loop
@mp32addw_end:
	sbb eax,eax
	neg eax

	pop edi
	ret


	.align 8
	.globl _mp32subw
_mp32subw:
	push edi

	mov ecx,[esp+8]
	mov edi,[esp+12]
	mov eax,[esp+16]

	lea edi,[edi+ecx*4-4]
	sub [edi],eax
	dec ecx
	jz @mp32subw_end
	sub edi,4
	xor edx,edx

	.align 4
@mp32subw_loop:
	sbb [edi],edx
	sub edi,4
	dec ecx
	jnz @mp32subw_loop
@mp32subw_end:
	sbb eax,eax
	neg eax

	pop edi
	ret


	.align 8
	.globl _mp32add
_mp32add:
	push edi
	push esi
	
	mov ecx,[esp+12]
	mov edi,[esp+16]
	mov esi,[esp+20]
	
	xor edx,edx
	dec ecx
		
@mp32add_loop:
	mov eax,[esi+ecx*4]
	adc [edi+ecx*4],eax
	dec ecx
	jns @mp32add_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret


	.align 8
	.globl _mp32sub
_mp32sub:
	push edi
	push esi
	
	mov ecx,[esp+12]
	mov edi,[esp+16]
	mov esi,[esp+20]
	
	xor edx,edx
	dec ecx
		
@mp32sub_loop:
	mov eax,[esi+ecx*4]
	sbb [edi+ecx*4],eax
	dec ecx
	jns @mp32sub_loop
	
	sbb eax,eax
	neg eax

	pop esi
	pop edi
	ret


	.align 8
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

	xor edx,edx
	dec ecx
	
	.align 4
@mp32setmul_loop:
	mov ebx,edx
	mov eax,[esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	mov [edi+ecx*4],eax
	dec ecx
	jns @mp32setmul_loop

	mov eax,edx

	pop ebp
	pop ebx
	pop esi
	pop edi
	ret


	.align 8
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

	xor edx,edx
	dec ecx
	
	.align 4
@mp32addmul_loop:
	mov ebx,edx
	mov eax,[esi+ecx*4]
	mul ebp
	add eax,ebx
	adc edx,0
	add [edi+ecx*4],eax
	adc edx,0
	dec ecx
	jns @mp32addmul_loop

	mov eax,edx

	pop ebp
	pop ebx
	pop esi
	pop edi
	ret


	.align 8
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
@mp32addsqrtrc_loop:
	mov eax,[esi+ecx*4]
	mul eax
	add eax,ebx
	adc edx,0
	add [edi+ecx*8+4],eax
	adc [edi+ecx*8+0],edx
	sbb ebx,ebx
	neg ebx
	dec ecx
	jns @mp32addsqrtrc_loop

	mov eax,ebx

	pop ebx
	pop esi
	pop edi
	ret
