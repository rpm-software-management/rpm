/*
 * mp32opt.arm.S
 *
 * Assembler optimized multiprecision integer routines for ARM processors
 *
 * Compile target is GNU Assembler
 *
 * Copyright (c) 2001, 2002 Virtual Unlimited B.V.
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

#include "beecrypt.gas.h"

	.file "mp32opt.arm.S"

	.text

/* ARM uses up to four registers for parameter passing */

#if 0
C_FUNCTION_BEGIN(mp32addw)
LABEL(mp32addw)
	/* r0 is xsize and must be at least one, r1 = xdata, r2 = y */
	stmfd sp!, {r4, lr}
	add r1, r1, r0, asl #2
	mov r3, #0
LOCAL(mp32addw_loop):
	ldr r4, [r1, #-4]
	adds r4, r4, r2
	adc r2, r3, r3
	str r4, [r1, #-4]!
	subs r0, r0, #1
	bne LOCAL(mp32addw_loop)
	mov r0, r2
	ldmfd sp!, {r4, pc}
C_FUNCTION_END(mp32addw, LOCAL(mp32addw_size))


C_FUNCTION_BEGIN(mp32subw)
LABEL(mp32subw)
	/* r0 is xsize and must be at least one, r1 = xdata, r2 = y */
	stmfd sp!, {r4, lr}
	add r1, r1, r0, asl #2
	mov r3, #0
LOCAL(mp32subw_loop):
	ldr r4, [r1, #-4]
	subs r4, r4, r2
	adc r2, r3, r3
	str r4, [r1, #-4]!
	subs r0, r0, #1
	bne LOCAL(mp32subw_loop)
	mov r0, r2
	ldmfd sp!, {r4, pc}
C_FUNCTION_END(mp32subw, LOCAL(mp32subw_size))


C_FUNCTION_BEGIN(mp32add)
LABEL(mp32add)
	/* r0 is size, r1 = xdata, r2 = ydata */
	stmfd sp!, {r4, r5, lr}
	/* copy cpsr to r5 and clear the carry bit */
	mrs r5, cpsr
	bic r5, r5, #0x20000000
	/* adjust the addresses */
	add r1, r1, r0, asl #2
	add r2, r2, r0, asl #2
LOCAL(mp32add_loop):
	/* restore the carry bit */
	msr cpsr_c, r5
	ldr r3, [r1, #-4]!
	ldr r4, [r2, #-4]!
	adcs r3, r3, r2
	str r3, [r1, #0]
	/* save the carry bit */
	mrs r5, cpsr
	subs r0, r0, #1
	bne LOCAL(mp32add_loop)

	/* restore the carry bit */
	msr cpsr_c, r5

	/* set the result to the proper value */
	adc r0, r0, r0
	ldmfd sp!, {r4, r5, pc}
C_FUNCTION_END(mp32add, LOCAL(mp32add_size))
#endif


C_FUNCTION_BEGIN(mp32setmul)
LABEL(mp32setmul)
	stmfd sp!, {r4, r5, lr}
	/* adjust the addresses */
	add r1, r1, r0, asl #2
	add r2, r2, r0, asl #2
	/* r3 is the multiplicand; r4 load from memory, r5 is scratch, ip is carry */
	mov ip, #0
LOCAL(mp32setmul_loop):
	ldr r4, [r2, #-4]!
	mov r5, #0
	umlal ip, r5, r3, r4
	str ip, [r1, #-4]!
	mov ip, r5
	subs r0, r0, #1
	bne LOCAL(mp32setmul_loop)
	/* return carry */
	mov r0, ip
	ldmfd sp!, {r4, r5, pc}
C_FUNCTION_END(mp32setmul, LOCAL(mp32setmul_size))

	
C_FUNCTION_BEGIN(mp32addmul)
LABEL(mp32addmul)
	stmfd sp!, {r4, r5, r6, lr}
	/* adjust the addresses */
	add r1, r1, r0, asl #2
	add r2, r2, r0, asl #2
	/* r3 is the multiplicand; r4 & r5 load from memory, r6 is scratch, ip is carry */
	mov ip, #0
LOCAL(mp32addmul_loop):
	ldr r4, [r2, #-4]!
	ldr r5, [r1, #-4]
	mov r6, #0
	umlal ip, r6, r3, r4
	adds r5, r5, ip
	adc ip, r6, #0
	str r5, [r1, #-4]!
	subs r0, r0, #1
	bne LOCAL(mp32addmul_loop)
	/* return carry */
	mov r0, ip
	ldmfd sp!, {r4, r5, r6, pc}
C_FUNCTION_END(mp32addmul, LOCAL(mp32addmul_size))


#if 0
/* this routine needs fixing; it causes a core dump for some reason */
/* unfortunately the system I test this on has no debugger */
C_FUNCTION_BEGIN(mp32addsqrtrc)
LABEL(mp32addsqrtrc):
	stmfd sp!, {r4, r5, r6, lr}
	/* adjust the addresses */
	add r1, r1, r0, asl #2
	add r2, r2, r0, asl #2
	/* r3 is a zero register, ip is the carry */
	mov r3, #0
	mov ip, #0
LOCAL(mp32addsqrtrc_loop):
	ldr r4, [r2, #-4]!
	mov r6, #0
	umlal ip, r6, r4, r4
	ldr r5, [r1, #-4] /* lo word */
	ldr r4, [r1, #-8] /* hi word */
	adds r5, r5, ip
	adcs r4, r4, r6
	str r5, [r1, #-4]
	str r4, [r1, #-8]!
	adc ip, r3, #0 /* set carry */
	subs r0, r0, #1
	bne LOCAL(mp32addsqrtrc_loop)
	/* return carry */
	mov r0, ip
	ldmfd sp!, {r4, r5, r6, pc}
C_FUNCTION_END(mp32addsqrtrc, LOCAL(mp32addsqrtrc_size))
#endif
