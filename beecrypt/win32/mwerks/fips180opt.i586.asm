#
# fips180opt.i586.asm
#
# Assembler optimized SHA-1 routines for Intel Pentium processors
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

K00	.equ	0x5a827999
K20	.equ	0x6ed9eba1
K40	.equ	0x8f1bbcdc
K60	.equ	0xca62c1d6

PARAM_H			.equ	0
PARAM_DATA		.equ	20
PARAM_OFFSET	.equ	352

subround1:	.macro b,c,d,e,w
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
	.endm

subround2:	.macro b,c,d,e,w
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
	.endm

subround3:	.macro b,c,d,e,w
	mov ecx,c
	rol eax,5
	mov ebx,b
	mov edx,ecx
	add eax,e
	or ecx,ebx # (b|c)
	and edx,ebx # (b&c)
	and ecx,d # (b|c)&d
	add eax,K40
	or ecx,edx
	add eax,w
	ror ebx,2
	add eax,ecx
	mov b,ebx
	mov e,eax
	.endm

subround4:	.macro b,c,d,e,w
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
	.endm

	.text

	.align	4
	.globl _sha1Process

_sha1Process:
	push edi
	push esi
	push ebx
	# allocate local variables
	push ebp
	lea ebp,[esp-24]

	mov esi,[esp+20]
	lea edi,[esi+PARAM_DATA]

	mov ecx,4
@loads:
	mov edx,[esi+ecx*4]
	mov [ebp+ecx*4],edx
	dec ecx
	jns @loads

	mov ecx,15
	xor eax,eax

	.align 4
@swaps:
	mov edx,[edi+ecx*4]
	bswap edx
	mov	[edi+ecx*4],edx
	dec ecx
	jns @swaps

	lea edi,[esi+PARAM_DATA]
	mov [esi+PARAM_OFFSET],al
	mov ecx,16

	.align 4
@xors:
	mov eax,[edi+52]
	mov ebx,[edi+56]
	xor eax,[edi+32]
	xor ebx,[edi+36]
	xor eax,[edi+ 8]
	xor ebx,[edi+12]
	xor eax,[edi   ]
	xor ebx,[edi+ 4]
	rol eax,1
	rol ebx,1
	mov [edi+64],eax
	mov [edi+68],ebx
	mov eax,[edi+60]
	mov ebx,[edi+64]
	xor eax,[edi+40]
	xor ebx,[edi+44]
	xor eax,[edi+16]
	xor ebx,[edi+20]
	xor eax,[edi+ 8]
	xor ebx,[edi+12]
	rol eax,1
	rol ebx,1
	mov [edi+72],eax
	mov [edi+76],ebx
	add edi,16
	dec ecx
	jnz @xors

	mov edi,PARAM_DATA

	# to optimize further, use esi only, and store the add constant into edi
	# will make code smaller and faster
	
@round01to20:
	mov eax,[ebp]
	subround1 [ebp+4],[ebp+8],[ebp+12],[ebp+16],[esi+edi]
	subround1 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround1 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround1 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround1 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround1 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround1 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround1 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround1 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround1 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround1 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround1 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround1 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround1 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround1 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround1 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround1 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround1 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround1 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround1 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	
@round21to40:
	subround2 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround2 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround2 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround2 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround2 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround2 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround2 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround2 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround2 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround2 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround2 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround2 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround2 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround2 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround2 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround2 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround2 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround2 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround2 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround2 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20

@round41to60:
	subround3 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround3 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround3 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround3 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround3 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround3 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround3 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround3 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround3 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround3 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround3 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround3 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround3 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround3 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround3 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround3 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround3 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround3 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround3 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround3 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20

@round61to80:
	subround4 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround4 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround4 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround4 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround4 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround4 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround4 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround4 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround4 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround4 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround4 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround4 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround4 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround4 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround4 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	add edi,20
	subround4 [ebp+4],ebx,[ebp+12],[ebp+16],[esi+edi]
	subround4 [ebp],ebx,[ebp+8],[ebp+12],[esi+edi+4]
	subround4 [ebp+16],ebx,[ebp+4],[ebp+8],[esi+edi+8]
	subround4 [ebp+12],ebx,[ebp],[ebp+4],[esi+edi+12]
	subround4 [ebp+8],ebx,[ebp+16],[ebp],[esi+edi+16]
	# add edi,20

	mov ecx,4

@adds:
	mov eax,[ebp+ecx*4]
	add [esi+ecx*4],eax
	dec ecx
	jns @adds

	pop ebp
	pop ebx
	pop esi
	pop edi
	ret
