;
; aesopt.i586.asm
;
; Assembler optimized AES routines for Intel Pentium processors
;
; Compile target is Microsoft Macro Assembler
;
; Copyright (c) 2002 Bob Deblier <bob.deblier@pandora.be>
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

EXTRN _ae0:DWORD
EXTRN _ae1:DWORD
EXTRN _ae2:DWORD
EXTRN _ae3:DWORD
EXTRN _ae4:DWORD

EXTRN _ad0:DWORD
EXTRN _ad1:DWORD
EXTRN _ad2:DWORD
EXTRN _ad3:DWORD
EXTRN _ad4:DWORD

	.code

; esp points to s and t (on stack; 32 bytes altogether)
; ebp points to rk
; edi points to dst
; esi points to src

sxrk	macro
	; compute swap(src) xor rk
	mov eax,dword ptr [esi   ]
	mov ebx,dword ptr [esi+ 4]
	mov ecx,dword ptr [esi+ 8]
	mov edx,dword ptr [esi+12]
	bswap eax
	bswap ebx
	bswap ecx
	bswap edx
	xor eax,dword ptr [ebp   ]
	xor ebx,dword ptr [ebp+ 4]
	xor ecx,dword ptr [ebp+ 8]
	xor edx,dword ptr [ebp+12]
	mov dword ptr [esp   ],eax
	mov dword ptr [esp+ 4],ebx
	mov dword ptr [esp+ 8],ecx
	mov dword ptr [esp+12],edx
	endm

etfs	macro	offset
	; compute t0 and t1
	mov ecx,[ebp+offset  ]
	mov edx,[ebp+offset+4]

	movzx eax,byte ptr [esp+ 3]
	movzx ebx,byte ptr [esp+ 7]
	xor ecx,dword ptr [eax*4+_ae0]
	xor edx,dword ptr [ebx*4+_ae0]

	movzx eax,byte ptr [esp+ 6]
	movzx ebx,byte ptr [esp+10]
	xor ecx,dword ptr [eax*4+_ae1]
	xor edx,dword ptr [ebx*4+_ae1]

	movzx eax,byte ptr [esp+ 9]
	movzx ebx,byte ptr [esp+13]
	xor ecx,dword ptr [eax*4+_ae2]
	xor edx,dword ptr [ebx*4+_ae2]

	movzx eax,byte ptr [esp+12]
	movzx ebx,byte ptr [esp   ]
	xor ecx,dword ptr [eax*4+_ae3]
	xor edx,dword ptr [ebx*4+_ae3]

	mov dword ptr [esp+16],ecx
	mov dword ptr [esp+20],edx

	; compute t2 and t3
	mov ecx,dword ptr [ebp+offset+ 8]
	mov edx,dword ptr [ebp+offset+12]

	movzx eax,byte ptr [esp+11]
	movzx ebx,byte ptr [esp+15]
	xor ecx,dword ptr [eax*4+_ae0]
	xor edx,dword ptr [ebx*4+_ae0]

	movzx eax,byte ptr [esp+14]
	movzx ebx,byte ptr [esp+ 2]
	xor ecx,dword ptr [eax*4+_ae1]
	xor edx,dword ptr [ebx*4+_ae1]

	movzx eax,byte ptr [esp+ 1]
	movzx ebx,byte ptr [esp+ 5]
	xor ecx,dword ptr [eax*4+_ae2]
	xor edx,dword ptr [ebx*4+_ae2]

	movzx eax,byte ptr [esp+ 4]
	movzx ebx,byte ptr [esp+ 8]
	xor ecx,dword ptr [eax*4+_ae3]
	xor edx,dword ptr [ebx*4+_ae3]

	mov dword ptr [esp+24],ecx
	mov dword ptr [esp+28],edx
	endm

esft	macro	offset
	; compute s0 and s1
	mov ecx,[ebp+offset  ]
	mov edx,[ebp+offset+4]

	movzx eax,byte ptr [esp+19]
	movzx ebx,byte ptr [esp+23]
	xor ecx,dword ptr [eax*4+_ae0]
	xor edx,dword ptr [ebx*4+_ae0]

	movzx eax,byte ptr [esp+22]
	movzx ebx,byte ptr [esp+26]
	xor ecx,dword ptr [eax*4+_ae1]
	xor edx,dword ptr [ebx*4+_ae1]

	movzx eax,byte ptr [esp+25]
	movzx ebx,byte ptr [esp+29]
	xor ecx,dword ptr [eax*4+_ae2]
	xor edx,dword ptr [ebx*4+_ae2]

	movzx eax,byte ptr [esp+28]
	movzx ebx,byte ptr [esp+16]
	xor ecx,dword ptr [eax*4+_ae3]
	xor edx,dword ptr [ebx*4+_ae3]

	mov dword ptr [esp   ],ecx
	mov dword ptr [esp+ 4],edx

	; compute s2 and s3
	mov ecx,dword ptr [ebp+offset+ 8]
	mov edx,dword ptr [ebp+offset+12]

	movzx eax,byte ptr [esp+27]
	movzx ebx,byte ptr [esp+31]
	xor ecx,dword ptr [eax*4+_ae0]
	xor edx,dword ptr [ebx*4+_ae0]

	movzx eax,byte ptr [esp+30]
	movzx ebx,byte ptr [esp+18]
	xor ecx,dword ptr [eax*4+_ae1]
	xor edx,dword ptr [ebx*4+_ae1]

	movzx eax,byte ptr [esp+17]
	movzx ebx,byte ptr [esp+21]
	xor ecx,dword ptr [eax*4+_ae2]
	xor edx,dword ptr [ebx*4+_ae2]

	movzx eax,byte ptr [esp+20]
	movzx ebx,byte ptr [esp+24]
	xor ecx,dword ptr [eax*4+_ae3]
	xor edx,dword ptr [ebx*4+_ae3]

	mov dword ptr [esp+ 8],ecx
	mov dword ptr [esp+12],edx
	endm

elr	macro
	mov ecx,dword ptr [ebp+ 0]
	mov edx,dword ptr [ebp+ 4]

	movzx eax,byte ptr [esp+19]
	movzx ebx,byte ptr [esp+23]
	mov eax,dword ptr [eax*4+_ae4]
	mov ebx,dword ptr [ebx*4+_ae4]
	and eax,0ff000000h
	and ebx,0ff000000h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+22]
	movzx ebx,byte ptr [esp+26]
	mov eax,dword ptr [eax*4+_ae4]
	mov ebx,dword ptr [ebx*4+_ae4]
	and eax,0ff0000h
	and ebx,0ff0000h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+25]
	movzx ebx,byte ptr [esp+29]
	mov eax,dword ptr [eax*4+_ae4]
	mov ebx,dword ptr [ebx*4+_ae4]
	and eax,0ff00h
	and ebx,0ff00h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+28]
	movzx ebx,byte ptr [esp+16]
	mov eax,dword ptr [eax*4+_ae4]
	mov ebx,dword ptr [ebx*4+_ae4]
	and eax,0ffh
	and ebx,0ffh
	xor ecx,eax
	xor edx,ebx

	mov dword ptr [esp+ 0],ecx
	mov dword ptr [esp+ 4],edx

	mov ecx,dword ptr [ebp+ 8]
	mov edx,dword ptr [ebp+12]

	movzx eax,byte ptr [esp+27]
	movzx ebx,byte ptr [esp+31]
	mov eax,dword ptr [eax*4+_ae4]
	mov ebx,dword ptr [ebx*4+_ae4]
	and eax,0ff000000h
	and ebx,0ff000000h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+30]
	movzx ebx,byte ptr [esp+18]
	mov eax,dword ptr [eax*4+_ae4]
	mov ebx,dword ptr [ebx*4+_ae4]
	and eax,0ff0000h
	and ebx,0ff0000h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+17]
	movzx ebx,byte ptr [esp+21]
	mov eax,dword ptr [eax*4+_ae4]
	mov ebx,dword ptr [ebx*4+_ae4]
	and eax,0ff00h
	and ebx,0ff00h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+20]
	movzx ebx,byte ptr [esp+24]
	mov eax,dword ptr [eax*4+_ae4]
	mov ebx,dword ptr [ebx*4+_ae4]
	and eax,0ffh
	and ebx,0ffh
	xor ecx,eax
	xor edx,ebx

	mov dword ptr [esp+ 8],ecx
	mov dword ptr [esp+12],edx
	endm

eblock	macro	label
	; load initial values for s0 thru s3
	sxrk

	; do 9 rounds
	etfs 16
	esft 32
	etfs 48
	esft 64
	etfs 80
	esft 96
	etfs 112
	esft 128
	etfs 144
	; test if we had to do 10 rounds, if yes jump to last round
	mov eax,dword ptr [ebp+256]
	cmp eax,10
	je @label
	; do two more rounds
	esft 160
	etfs 176
	; test if we had to do 12 rounds, if yes jump to last round
	mov eax,dword ptr [ebp+256]
	cmp eax,12
	je @label
	; do two more rounds
	esft 192
	etfs 208
	; prepare for last round
	mov eax,dword ptr [ebp+256]
@label:
	; add 16 times the number of rounds to ebp
	sal eax,4
	add ebp,eax
	; do last round
	elr
	endm

eblockc	macro	label
	; encrypt block in cbc mode
	sxrfxrk

	; do 9 rounds
	etfs 16
	esft 32
	etfs 48
	esft 64
	etfs 80
	esft 96
	etfs 112
	esft 128
	etfs 144
	; test if we had to do 10 rounds, if yes jump to last round
	mov eax,dword ptr [ebp+256]
	cmp eax,10
	je @label
	; do two more rounds
	esft 160
	etfs 176
	; test if we had to do 12 rounds, if yes jump to last round
	mov eax,dword ptr [ebp+256]
	cmp eax,12
	je @label
	; do two more rounds
	esft 192
	etfs 208
	; prepare for last round
	mov eax,dword ptr [ebp+256]
@label:
	; add 16 times the number of rounds to ebp
	sal eax,4
	add ebp,eax
	; do last round
	elr
	endm

dtfs	macro	offset
	; compute t0 and t1
	mov ecx,[ebp+offset  ]
	mov edx,[ebp+offset+4]

	movzx eax,byte ptr [esp+ 3]
	movzx ebx,byte ptr [esp+ 7]
	xor ecx,dword ptr [eax*4+_ad0]
	xor edx,dword ptr [ebx*4+_ad0]

	movzx eax,byte ptr [esp+14]
	movzx ebx,byte ptr [esp+ 2]
	xor ecx,dword ptr [eax*4+_ad1]
	xor edx,dword ptr [ebx*4+_ad1]

	movzx eax,byte ptr [esp+ 9]
	movzx ebx,byte ptr [esp+13]
	xor ecx,dword ptr [eax*4+_ad2]
	xor edx,dword ptr [ebx*4+_ad2]

	movzx eax,byte ptr [esp+ 4]
	movzx ebx,byte ptr [esp+ 8]
	xor ecx,dword ptr [eax*4+_ad3]
	xor edx,dword ptr [ebx*4+_ad3]

	mov dword ptr [esp+16],ecx
	mov dword ptr [esp+20],edx

	; compute t2 and t3
	mov ecx,dword ptr [ebp+offset+ 8]
	mov edx,dword ptr [ebp+offset+12]

	movzx eax,byte ptr [esp+11]
	movzx ebx,byte ptr [esp+15]
	xor ecx,dword ptr [eax*4+_ad0]
	xor edx,dword ptr [ebx*4+_ad0]

	movzx eax,byte ptr [esp+ 6]
	movzx ebx,byte ptr [esp+10]
	xor ecx,dword ptr [eax*4+_ad1]
	xor edx,dword ptr [ebx*4+_ad1]

	movzx eax,byte ptr [esp+ 1]
	movzx ebx,byte ptr [esp+ 5]
	xor ecx,dword ptr [eax*4+_ad2]
	xor edx,dword ptr [ebx*4+_ad2]

	movzx eax,byte ptr [esp+12]
	movzx ebx,byte ptr [esp   ]
	xor ecx,dword ptr [eax*4+_ad3]
	xor edx,dword ptr [ebx*4+_ad3]

	mov dword ptr [esp+24],ecx
	mov dword ptr [esp+28],edx
	endm

dsft	macro	offset
	; compute s0 and s1
	mov ecx,[ebp+offset  ]
	mov edx,[ebp+offset+4]

	movzx eax,byte ptr [esp+19]
	movzx ebx,byte ptr [esp+23]
	xor ecx,dword ptr [eax*4+_ad0]
	xor edx,dword ptr [ebx*4+_ad0]

	movzx eax,byte ptr [esp+30]
	movzx ebx,byte ptr [esp+18]
	xor ecx,dword ptr [eax*4+_ad1]
	xor edx,dword ptr [ebx*4+_ad1]

	movzx eax,byte ptr [esp+25]
	movzx ebx,byte ptr [esp+29]
	xor ecx,dword ptr [eax*4+_ad2]
	xor edx,dword ptr [ebx*4+_ad2]

	movzx eax,byte ptr [esp+20]
	movzx ebx,byte ptr [esp+24]
	xor ecx,dword ptr [eax*4+_ad3]
	xor edx,dword ptr [ebx*4+_ad3]

	mov dword ptr [esp   ],ecx
	mov dword ptr [esp+ 4],edx

	; compute s2 and s3
	mov ecx,dword ptr [ebp+offset+ 8]
	mov edx,dword ptr [ebp+offset+12]

	movzx eax,byte ptr [esp+27]
	movzx ebx,byte ptr [esp+31]
	xor ecx,dword ptr [eax*4+_ad0]
	xor edx,dword ptr [ebx*4+_ad0]

	movzx eax,byte ptr [esp+22]
	movzx ebx,byte ptr [esp+26]
	xor ecx,dword ptr [eax*4+_ad1]
	xor edx,dword ptr [ebx*4+_ad1]

	movzx eax,byte ptr [esp+17]
	movzx ebx,byte ptr [esp+21]
	xor ecx,dword ptr [eax*4+_ad2]
	xor edx,dword ptr [ebx*4+_ad2]

	movzx eax,byte ptr [esp+28]
	movzx ebx,byte ptr [esp+16]
	xor ecx,dword ptr [eax*4+_ad3]
	xor edx,dword ptr [ebx*4+_ad3]

	mov dword ptr [esp+ 8],ecx
	mov dword ptr [esp+12],edx
	endm

dlr	macro
	mov ecx,dword ptr [ebp+ 0]
	mov edx,dword ptr [ebp+ 4]

	movzx eax,byte ptr [esp+19]
	movzx ebx,byte ptr [esp+23]
	mov eax,dword ptr [eax*4+_ad4]
	mov ebx,dword ptr [ebx*4+_ad4]
	and eax,0ff000000h
	and ebx,0ff000000h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+30]
	movzx ebx,byte ptr [esp+18]
	mov eax,dword ptr [eax*4+_ad4]
	mov ebx,dword ptr [ebx*4+_ad4]
	and eax,0ff0000h
	and ebx,0ff0000h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+25]
	movzx ebx,byte ptr [esp+29]
	mov eax,dword ptr [eax*4+_ad4]
	mov ebx,dword ptr [ebx*4+_ad4]
	and eax,0ff00h
	and ebx,0ff00h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+20]
	movzx ebx,byte ptr [esp+24]
	mov eax,dword ptr [eax*4+_ad4]
	mov ebx,dword ptr [ebx*4+_ad4]
	and eax,0ffh
	and ebx,0ffh
	xor ecx,eax
	xor edx,ebx

	mov dword ptr [esp+ 0],ecx
	mov dword ptr [esp+ 4],edx

	mov ecx,dword ptr [ebp+ 8]
	mov edx,dword ptr [ebp+12]

	movzx eax,byte ptr [esp+27]
	movzx ebx,byte ptr [esp+31]
	mov eax,dword ptr [eax*4+_ad4]
	mov ebx,dword ptr [ebx*4+_ad4]
	and eax,0ff000000h
	and ebx,0ff000000h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+22]
	movzx ebx,byte ptr [esp+26]
	mov eax,dword ptr [eax*4+_ad4]
	mov ebx,dword ptr [ebx*4+_ad4]
	and eax,0ff0000h
	and ebx,0ff0000h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+17]
	movzx ebx,byte ptr [esp+21]
	mov eax,dword ptr [eax*4+_ad4]
	mov ebx,dword ptr [ebx*4+_ad4]
	and eax,0ff00h
	and ebx,0ff00h
	xor ecx,eax
	xor edx,ebx

	movzx eax,byte ptr [esp+28]
	movzx ebx,byte ptr [esp+16]
	mov eax,dword ptr [eax*4+_ad4]
	mov ebx,dword ptr [ebx*4+_ad4]
	and eax,0ffh
	and ebx,0ffh
	xor ecx,eax
	xor edx,ebx

	mov dword ptr [esp+ 8],ecx
	mov dword ptr [esp+12],edx
	endm

dblock	macro	label
	; load initial values for s0 thru s3
	sxrk

	; do 9 rounds
	dtfs 16
	dsft 32
	dtfs 48
	dsft 64
	dtfs 80
	dsft 96
	dtfs 112
	dsft 128
	dtfs 144
	; test if we had to do 10 rounds, if yes jump to last round
	mov eax,dword ptr [ebp+256]
	cmp eax,10
	je @label
	; do two more rounds
	dsft 160
	dtfs 176
	; test if we had to do 12 rounds, if yes jump to last round
	mov eax,dword ptr [ebp+256]
	cmp eax,12
	je @label
	; do two more rounds
	dsft 192
	dtfs 208
	; prepare for last round
	mov eax,dword ptr [ebp+256]
@label:
	; add 16 times the number of rounds to ebp
	sal eax,4
	add ebp,eax
	; do last round
	dlr
	endm

aesEncrypt proc c export
	push edi
	push esi
	push ebp
	push ebx

	; set pointers
	mov ebp,dword ptr [esp+20] ; rk
	mov edi,dword ptr [esp+24] ; dst
	mov esi,dword ptr [esp+28] ; src

	; add local storage for s and t variables, 32 bytes total
	sub esp,32

	eblock e

	; save stuff back
	mov eax,dword ptr [esp+ 0]
	mov ebx,dword ptr [esp+ 4]
	mov ecx,dword ptr [esp+ 8]
	mov edx,dword ptr [esp+12]
	bswap eax
	bswap ebx
	bswap ecx
	bswap edx
	mov dword ptr [edi   ],eax
	mov dword ptr [edi+ 4],ebx
	mov dword ptr [edi+ 8],ecx
	mov dword ptr [edi+12],edx

	; remove local storage
	add esp,32

	xor eax,eax

	pop ebx
	pop ebp
	pop esi
	pop edi
	ret
aesEncrypt endp

aesDecrypt proc c export
	push edi
	push esi
	push ebp
	push ebx

	; set pointers
	mov ebp,dword ptr [esp+20] ; rk
	mov edi,dword ptr [esp+24] ; dst
	mov esi,dword ptr [esp+28] ; src

	; add local storage for s and t variables, 32 bytes total
	sub esp,32

	dblock d

	; save stuff back
	mov eax,dword ptr [esp+ 0]
	mov ebx,dword ptr [esp+ 4]
	mov ecx,dword ptr [esp+ 8]
	mov edx,dword ptr [esp+12]
	bswap eax
	bswap ebx
	bswap ecx
	bswap edx
	mov dword ptr [edi   ],eax
	mov dword ptr [edi+ 4],ebx
	mov dword ptr [edi+ 8],ecx
	mov dword ptr [edi+12],edx

	; remove local storage
	add esp,32

	xor eax,eax

	pop ebx
	pop ebp
	pop esi
	pop edi
	ret
aesDecrypt endp

	end
