/*
 * mp64opt.ia64.S
 *
 * Assembler optimized multiprecision integer routines for ia64 (Intel Itanium)
 *
 * Compile target is GNU Assembler
 *
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
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

#include "config.gas.h"

#define saved_pfs	r14
#define	saved_lc	r15

#define size		r16
#define dst			r17
#define src			r18
#define alt			r19

	.text

	.explicit

/* functions to add, in order of priority:
 * mp64addsqrtrc
 * mp64neg -> can vectorize
 * mp64multwo -> can vectorize
 * mp32divtwo -> ..
 * mp64fill -> easy
 * mp64z -> vectorizable with br.wtop
 * mp64nz -> vectorizable with br.wtop
 * mp64eq -> ..
 * mp64eqx -> ..
 * mp64ne -> ..
 * mp64nex -> ..
 * mp64gt -> ..
 * mp64gtx -> ..
 * mp64lt -> ..
 * mp64ltx -> ..
 * mp64ge -> substitute with mp64lt with swap of parameters
 * mp64gex -> .. mp64ltx
 * mp64le -> .. mp64gt
 * mp64lex -> .. mp64gtx
 * mp64isone -> vectorizable with br.wtop
 * mp64istwo -> ..
 * mp64leone -> ..
 * mp64size -> ..

/* mp64zero works */
C_FUNCTION_BEGIN(mp64zero)
	alloc saved_pfs = ar.pfs,2,0,0,0
	mov saved_lc = ar.lc
	sub size = in0,r0,1;;
	mov src = in1
	mov ar.lc = size;;

.Lmp64zero_loop:
	st8 [src] = r0,8
	br.ctop.sptk .Lmp64zero_loop;;

	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mp64zero)


/* mp64copy works */
C_FUNCTION_BEGIN(mp64copy)
	alloc saved_pfs = ar.pfs,3,5,0,8
	mov saved_lc = ar.lc
	sub size = in0,r0,1;;
	mov dst = in1
	mov src = in2
	/* prepare loop */
	mov ar.lc = size
	mov ar.ec = 2
	mov pr.rot = (1 << 16);;

.Lmp64copy_loop:
	(p17) st8 [dst] = r33,-8
	(p16) ld8 r32 = [src],-8;;
	br.ctop.sptk .Lmp64copy_loop;;

	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mp64copy)


#if 0
/* mp64z is in development */
C_FUNCTION_BEGIN(mp64z)
	alloc saved_pfs = ar.pfs,2,6,0,8
	mov saved_lc = ar.lc
	sub size = in0,r0,1;;

	mov ret0 = 1
	mov src = in1

	mov ar.lc = size
	mov ar.ec = 2
	mov pr.rot = ((1 << 16) | (1 << 20));;

.Lmp64z_loop:
	(p16) ld8 r32 = [src],8
	(p17) cmp.ne p1,p0 = r33,r0
	(p1) br.exit.dpnt .Lmp64z_exit;;
	br.ctop.dptk .Lmp64z_loop;;
.Lmp64z_exit:
	(p1) mov ret0 = r0

	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mp64z)
#endif


/* mp64add works */
C_FUNCTION_BEGIN(mp64add)
	alloc saved_pfs = ar.pfs,3,5,0,8
	mov saved_lc = ar.lc
	sub size = in0,r0,1;;

	/* adjust addresses */
	shladd dst = size,3,in1
	shladd src = size,3,in2
	shladd alt = size,3,in1

	/* prepare modulo-scheduled loop */
	mov ar.lc = size
	mov ar.ec = 3 
	mov pr.rot = ((1 << 16) | (1 << 19));;

.Lmp64add_loop:
	(p16) ld8 r32 = [src],-8
	(p16) ld8 r35 = [alt],-8
	(p20) add r36 = r33,r36		/* no carry add */
	(p22) add r36 = r33,r36,1	/* carry add */
	;;
	(p20) cmp.leu p19,p21 = r33,r36	/* no previous carry */
	(p22) cmp.ltu p19,p21 = r33,r36	/* previous carry */
	(p18) st8 [dst] = r37,-8
	br.ctop.dptk .Lmp64add_loop;;

	/* return carry */
	(p21) add ret0 = r0,r0
	(p23) add ret0 = r0,r0,1
	;;
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mp64add)


/* mp64sub is in development */
C_FUNCTION_BEGIN(mp64sub)
	alloc saved_pfs = ar.pfs,3,5,0,8
	mov saved_lc = ar.lc
	sub size = in0,r0,1;;

	/* adjust addresses */
	shladd dst = size,3,in1
	shladd src = size,3,in2
	shladd alt = size,3,in1

	/* prepare modulo-scheduled loop */
	mov ar.lc = size
	mov ar.ec = 3 
	mov pr.rot = ((1 << 16) | (1 << 19));;

.Lmp64sub_loop:
	(p16) ld8 r32 = [src],-8
	(p16) ld8 r35 = [alt],-8
	(p20) sub r36 = r33,r36		/* no carry sub */
	(p22) sub r36 = r33,r36,1	/* carry sub */
	;;
	(p20) cmp.geu p19,p21 = r33,r36	/* no previous carry */
	(p22) cmp.gtu p19,p21 = r33,r36	/* previous carry */
	(p18) st8 [dst] = r37,-8
	br.ctop.dptk .Lmp64sub_loop;;

	/* return carry */
	(p21) add ret0 = r0,r0
	(p23) add ret0 = r0,r0,1
	;;
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mp64sub)


/* mp64setmul works */
C_FUNCTION_BEGIN(mp64setmul)
	alloc saved_pfs = ar.pfs,4,4,0,8
	mov saved_lc = ar.lc

	setf.sig f6 = in3	/* the multiplier */
	setf.sig f7 = r0 	/* the carry */
	sub size = in0,r0,1;;

	/* adjust addresses */
	shladd dst = size,3,in1
	shladd src = size,3,in2

	/* prepare modulo-scheduled loop */
	mov ar.lc = size
	mov ar.ec = 3
	mov pr.rot = (1 << 16);;

.Lmp64setmul_loop:
	(p16) ldf8 f36 = [src],-8
	(p18) stf8 [dst] = f33,-8
	(p17) xma.lu f32 = f6,f37,f7
	(p17) xma.hu f7  = f6,f37,f7;;
	br.ctop.dptk .Lmp64setmul_loop;;

	/* return carry */
	getf.sig ret0 = f7;;

	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mp64setmul)


/* mp64addmul needs fixing */
C_FUNCTION_BEGIN(mp64addmul)
	alloc saved_pfs = ar.pfs,4,12,0,16
	mov saved_lc = ar.lc

	sub size = in0,r0,1;;
	setf.sig f6 = in3	/* the multiplier */

	/* adjust addresses */
	shladd dst = size,3,in1
	shladd src = size,3,in2
	shladd alt = size,3,in1;;

	/* prepare the rotate-in carry */
	mov r32 = r0		

	/* prepare modulo-scheduled loop */
	mov ar.lc = size
	mov ar.ec = 5
	mov pr.rot = ((1 << 16) | (1 << 21));

.Lmp64addmul_loop:
	(p18) getf.sig r33 = f34	/* hi 64 bit word */
	(p24) add r38 = r35,r38
	(p17) xma.lu f37 = f6,f41,f45
	(p18) getf.sig r37 = f38	/* lo 64 bit word */
	(p26) add r38 = r35,r38,1
	(p17) xma.hu f33 = f6,f41,f45
	(p16) ldf8 f40 = [src],-8
	(p16) ldf8 f44 = [alt],-8
	;;
	/* set carry from this operation */
	(p24) cmp.leu p23,p25 = r35,r38
	(p26) cmp.ltu p23,p25 = r35,r38
	(p20) st8 [dst] = r39,-8
	br.ctop.dptk .Lmp64addmul_loop;;

	/* return carry */
	(p25) add ret0 = r36,r0
	(p27) add ret0 = r36,r0,1

	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mp64addmul)

/* mp64addsqrtrc will be a little more challenging */

/* the primary loop will look like this:

.Lmp64addsqrtrc_loop:
	/* stage 1 */
	(p16) ldf8 to_square
	(p16) ld8 lo_to_add
	(p16) ld8 hi_to_add
	/* stage 2 */
	(p17) xma.lu to_square,to_square,carry
	(p17) xma.hu to_square,to_square,carry
	/* stage 3 */
	(p18) getf lo xma
	(p18) getf hi xma
	/* stage 4 */
	(p?) add lo no carry 
	(p?) add lo carry
	/* stage 5 */
    (p?+1) add hi no carry
	(p?+1) add hi carry
	;;
	/* also stage 4 */
	(p?) cmp lo for carry
	(p?) cmp lo for carry
	/* also stage 5 */
	(p?+1) cmp hi for carry
	(p?+1) cmp hi for carry
	st8 lo
	st8 hi
	br.ctop
*/
