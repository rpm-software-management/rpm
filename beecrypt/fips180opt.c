/*
 * fips180pt.c
 *
 * Inline assembler optimized sha1 routines
 * 
 * Copyright (c) 2000 Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define BEECRYPT_DLL_EXPORT

#include "fips180.h"

#if WIN32
#if __INTEL__ && __MWERKS__

void sha1ProcessMMX(sha1Param *param)
{
	uint64 tstart, tstop;
	
	asm {
		rdtsc
		mov dword ptr tstart,eax
		mov dword ptr tstart+4,edx

		mov esi,dword ptr param
		lea edi,[esi+20+64]
		mov ecx,-15
		xor eax,eax
		
swaps:
		mov edx,[edi+ecx*4]
		bswap edx
		mov [edi+ecx*4],eax
		inc ecx
		jnz swaps
		
/* don't do this one in MMX */

		mov [esi+352],al
		mov ecx,16

/* don't unroll any futher */

		align 4
xors:
		mov eax,[edi-12]
		mov ebx,[edi-8]
		xor eax,[edi-24]
		xor ebx,[edi-20]
		xor eax,[edi-56]
		xor ebx,[edi-52]
		xor eax,[edi-64]
		xor ebx,[edi-60]
		rol eax,1
		rol ebx,1
		mov [edi],eax
		mov [edi+4],ebx
		mov eax,[edi-4]
		mov ebx,[edi]
		xor eax,[edi-16]
		xor ebx,[edi-8]
		xor eax,[edi-48]
		xor ebx,[edi-44]
		xor eax,[edi-56]
		xor ebx,[edi-52]
		rol eax,1
		rol ebx,1
		mov [edi+8],eax
		mov [edi+12],ebx
		lea edi,[edi+16]
		dec ecx
		jnz xors

		mov edi,20
		
/* non-mmx subround1 */
subround1:
		; do subround one in registers
		mov eax,dword ptr [esi+0]
		mov ebx,dword ptr [esi+4]
		mov ecx,dword ptr [esi+8]
		mov edx,dword ptr [esi+12]
		rol eax,5
		xor ecx,edx
		add eax,dword ptr [esi+16]
		and ecx,ebx
		add eax,0x5a827999
		ror ebx,2
		add eax,dword ptr [esi+edi+0]
		xor ecx,edx
		mov dword ptr [esi+4],ebx
		add eax,ecx
		mov dword ptr [esi+16],eax
		; eax is still okay!
		; ecx's value is in ebx
		mov ecx,ebx
		mov ebx,dword ptr [esi+0]
		mov edx,dword ptr [esi+8]
		rol eax,5
		xor ecx,edx
		add eax,dword ptr [esi+12]
		and ecx,ebx
		add eax,0x5a827999
		ror ebx,2
		add eax,dword ptr [esi+edi+4]
		xor ecx,edx
		mov dword ptr [esi+0],ebx
		add eax,ecx
		mov dword ptr [esi+12],eax
		; eax is still okay!
		; ecx's value is in ebx
		mov ecx,ebx
		mov ebx,dword ptr [esi+16]
		mov edx,dword ptr [esi+4]
		rol eax,5
		xor ecx,edx
		add eax,dword ptr [esi+8]
		and ecx,ebx
		add eax,0x5a827999
		ror ebx,2
		add eax,dword ptr [esi+edi+8]
		xor ecx,edx
		mov dword ptr [esi+16],ebx
		add eax,ecx
		mov dword ptr [esi+8],eax
		; eax is still okay!
		; ecx's value is in ebx
		mov ecx,ebx
		mov ebx,dword ptr [esi+12]
		mov edx,dword ptr [esi+0]
		rol eax,5
		xor ecx,edx
		add eax,dword ptr [esi+4]
		and ecx,ebx
		add eax,0x5a827999
		ror ebx,2
		add eax,dword ptr [esi+edi+12]
		xor ecx,edx
		mov dword ptr [esi+12],ebx
		add eax,ecx
		mov dword ptr [esi+4],eax
		; eax is still okay!
		; ecx's value is in ebx
		mov ecx,ebx
		mov ebx,dword ptr [esi+8]
		mov edx,dword ptr [esi+16]
		rol eax,5
		xor ecx,edx
		add eax,dword ptr [esi+0]
		and ecx,ebx
		add eax,0x5a827999
		ror ebx,2
		add eax,dword ptr [esi+edi+16]
		xor ecx,edx
		mov dword ptr [esi+8],ebx
		add eax,ecx
		mov dword ptr [esi+0],eax
		; this has to be repeated 5 times
		add edi,20
		cmp edi,120
		jne subround1

subround2:
		mov eax,dword ptr [esi+0]
		mov ebx,dword ptr [esi+4]
		mov ecx,dword ptr [esi+8]
		mov edx,dword ptr [esi+12]
		rol eax,5
		xor edx,ebx
		add eax,dword ptr [esi+16]
		xor edx,ecx
		add eax,0x6ed9eba1
		ror ebx,2
		add eax,dword ptr [esi+edi+0]
		mov dword ptr [esi+4],ebx
		add eax,edx
		mov dword ptr [esi+16],eax
		; eax is still okay
		; move ecx to edx
		; move ebx to ecx
		mov edx,ecx
		mov ecx,ebx
		mov ebx,dword ptr [esi+0]
		rol eax,5
		xor edx,ebx
		add eax,dword ptr [esi+12]
		xor edx,ecx
		add eax,0x6ed9eba1
		ror ebx,2
		add eax,dword ptr [esi+edi+4]
		mov dword ptr [esi+0],ebx
		add eax,edx
		mov dword ptr [esi+12],eax
		; eax is still okay
		; move ecx to edx
		; move ebx to ecx
		mov edx,ecx
		mov ecx,ebx
		mov ebx,dword ptr [esi+16]
		rol eax,5
		xor edx,ebx
		add eax,dword ptr [esi+8]
		xor edx,ecx
		add eax,0x6ed9eba1
		ror ebx,2
		add eax,dword ptr [esi+edi+8]
		mov dword ptr [esi+16],ebx
		add eax,edx
		mov dword ptr [esi+8],eax
		; eax is still okay
		; move ecx to edx
		; move ebx to ecx
		mov edx,ecx
		mov ecx,ebx
		mov ebx,dword ptr [esi+12]
		rol eax,5
		xor edx,ebx
		add eax,dword ptr [esi+4]
		xor edx,ecx
		add eax,0x6ed9eba1
		ror ebx,2
		add eax,dword ptr [esi+edi+12]
		mov dword ptr [esi+12],ebx
		add eax,edx
		mov dword ptr [esi+4],eax
		; eax is still okay
		; move ecx to edx
		; move ebx to ecx
		mov edx,ecx
		mov ecx,ebx
		mov ebx,dword ptr [esi+8]
		rol eax,5
		xor edx,ebx
		add eax,dword ptr [esi+0]
		xor edx,ecx
		add eax,0x6ed9eba1
		ror ebx,2
		add eax,dword ptr [esi+edi+16]
		mov dword ptr [esi+8],ebx
		add eax,edx
		mov dword ptr [esi+0],eax
		add edi,20
		cmp edi,240
		jne subround2

		; time it
		rdtsc
		mov dword ptr tstop,eax
		mov dword ptr tstop+4,edx
	}
	
	printf("took %lld clocks\n", tstop - tstart);
}

#endif
#endif
