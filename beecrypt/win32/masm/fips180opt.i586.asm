;
; fips180opt.i586.asm
;
; Assembler optimized SHA-1 routines for Intel Pentium processors
;
; Compile target is Microsoft Macro Assembler
;
; Copyright (c) 2000 Virtual Unlimited B.V.
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

K00	equ	5a827999h
K20	equ	6ed9eba1h
K40	equ	8f1bbcdch
K60	equ	0ca62c1d6h

PARAM_H			equ	0
PARAM_DATA		equ	20
PARAM_OFFSET	equ	352

	.code

subround1 macro b,c,d,e,w
	mov ecx,c
	mov ebx,b
	mov edx,d
	rol eax,5
	xor ecx,edx
	add eax,e
	and ecx,ebx
	add eax,K00
	ror ebx,2
	add eax,w
	xor ecx,edx
	mov b,ebx
	add eax,ecx
	mov e,eax
	endm

subround2 macro b,c,d,e,w
	mov ecx,c
	mov ebx,b
	rol eax,5
	xor ecx,ebx
	add eax,e
	xor ecx,d
	add eax,K20
	ror ebx,2
	add eax,w
	mov b,ebx
	add eax,ecx
	mov e,eax
	endm

subround3 macro b,c,d,e,w
	mov ecx,c
	rol eax,5
	mov ebx,b
	mov edx,ecx
	add eax,e
	or ecx,ebx
	and edx,ebx
	and ecx,d
	add eax,K40
	or ecx,edx
	add eax,w
	ror ebx,2
	add eax,ecx
	mov b,ebx
	mov e,eax
	endm

subround4 macro b,c,d,e,w
	mov ecx,c
	mov ebx,b
	rol eax,5
	xor ecx,ebx
	add eax,e
	xor ecx,d
	add eax,K60
	ror ebx,2
	add eax,w
	mov b,ebx
	add eax,ecx
	mov e,eax
	endm


	align 8

sha1Process proc
	push edi
	push esi
	push ebx
	push ebp
	; allocate local variables
	mov esi,dword ptr [esp+20]  ; esi now points to param
	sub esp,20					; esp now points below the local variables
	lea edi,dword ptr [esi+PARAM_DATA]
	mov ebp,esp                 ; ebp now points to the local variables

	mov ecx,4
@loads:
	mov edx,dword ptr [esi+ecx*4]
	mov dword ptr [ebp+ecx*4],edx
	dec ecx
	jns @loads

	mov ecx,15
	xor eax,eax

	align 4
@swaps:
	mov edx,dword ptr [edi+ecx*4]
	bswap edx
	mov	dword ptr [edi+ecx*4],edx
	dec ecx
	jns @swaps

	lea edi,dword ptr [esi+PARAM_DATA]
	mov byte ptr [esi+PARAM_OFFSET],al
	mov ecx,16

	align 4
@xors:
	mov eax,dword ptr [edi+52]
	mov ebx,dword ptr [edi+56]
	xor eax,dword ptr [edi+32]
	xor ebx,dword ptr [edi+36]
	xor eax,dword ptr [edi+ 8]
	xor ebx,dword ptr [edi+12]
	xor eax,dword ptr [edi   ]
	xor ebx,dword ptr [edi+ 4]
	rol eax,1
	rol ebx,1
	mov dword ptr [edi+64],eax
	mov dword ptr [edi+68],ebx
	mov eax,dword ptr [edi+60]
	mov ebx,dword ptr [edi+64]
	xor eax,dword ptr [edi+40]
	xor ebx,dword ptr [edi+44]
	xor eax,dword ptr [edi+16]
	xor ebx,dword ptr [edi+20]
	xor eax,dword ptr [edi+ 8]
	xor ebx,dword ptr [edi+12]
	rol eax,1
	rol ebx,1
	mov dword ptr [edi+72],eax
	mov dword ptr [edi+76],ebx
	add edi,16
	dec ecx
	jnz @xors

	mov edi,PARAM_DATA

	; to optimize further, use esi only, and store the add constant into edi
	; will make code smaller and faster
	
@round01to20:
	mov eax,dword ptr [ebp]
	subround1 dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround1 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround1 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround1 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround1 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround1 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround1 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround1 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround1 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround1 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround1 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround1 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround1 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround1 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround1 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround1 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround1 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround1 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround1 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround1 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	
@round21to40:
	subround2 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround2 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround2 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround2 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround2 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround2 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround2 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround2 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround2 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround2 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround2 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround2 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround2 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround2 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround2 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround2 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround2 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround2 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround2 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround2 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20

@round41to60:
	subround3 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround3 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround3 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround3 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround3 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround3 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround3 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround3 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround3 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround3 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround3 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround3 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround3 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround3 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround3 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround3 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround3 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround3 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround3 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround3 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20

@round61to80:
	subround4 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround4 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround4 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround4 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround4 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround4 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround4 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround4 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround4 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround4 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround4 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround4 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround4 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround4 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround4 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	add edi,20
	subround4 dword ptr [ebp+4 ], ebx, dword ptr [ebp+12], dword ptr [ebp+16], dword ptr [esi+edi]
	subround4 dword ptr [ebp   ], ebx, dword ptr [ebp+8 ], dword ptr [ebp+12], dword ptr [esi+edi+4 ]
	subround4 dword ptr [ebp+16], ebx, dword ptr [ebp+4 ], dword ptr [ebp+8 ], dword ptr [esi+edi+8 ]
	subround4 dword ptr [ebp+12], ebx, dword ptr [ebp   ], dword ptr [ebp+4 ], dword ptr [esi+edi+12]
	subround4 dword ptr [ebp+8 ], ebx, dword ptr [ebp+16], dword ptr [ebp   ], dword ptr [esi+edi+16]
	; add edi,20

	mov ecx,4

@adds:
	mov eax,dword ptr [ebp+ecx*4]
	add dword ptr [esi+ecx*4],eax
	dec ecx
	jns @adds

	add esp,20
	pop ebp
	pop ebx
	pop esi
	pop edi
	ret
sha1Process	endp

	end
