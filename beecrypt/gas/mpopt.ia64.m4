dnl  mpopt.ia64.m4
dnl
dnl  Copyright (c) 2003 Bob Deblier
dnl 
dnl  Author: Bob Deblier <bob.deblier@pandora.be>
dnl 
dnl  This library is free software; you can redistribute it and/or
dnl  modify it under the terms of the GNU Lesser General Public
dnl  License as published by the Free Software Foundation; either
dnl  version 2.1 of the License, or (at your option) any later version.
dnl 
dnl  This library is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl  Lesser General Public License for more details.
dnl 
dnl  You should have received a copy of the GNU Lesser General Public
dnl  License along with this library; if not, write to the Free Software
dnl  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

include(config.m4)
include(ASM_SRCDIR/ia64.m4)

define(`sze',`r14')
define(`dst',`r15')
define(`src',`r16')
define(`alt',`r17')


C_FUNCTION_BEGIN(mpzero)
	.prologue
	alloc saved_pfs = ar.pfs,2,0,0,0
	mov saved_lc = ar.lc
	sub sze = in0,r0,1;;

dnl	adjust address
	shladd dst = sze,3,in1

dnl	prepare loop
	mov ar.lc = sze;;

	.body
LOCAL(mpzero_loop):
	st8 [dst] = r0,-8
	br.ctop.dptk LOCAL(mpzero_loop);;

	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mpzero)


C_FUNCTION_BEGIN(mpcopy)
	.prologue
	alloc saved_pfs = ar.pfs,3,6,0,8
	mov saved_lc = ar.lc
	mov saved_pr = pr
	sub sze = in0,r0,1;;

dnl	adjust addresses
	shladd dst = sze,3,in1
	shladd src = sze,3,in2

dnl	prepare modulo-scheduled loop
	mov ar.lc = sze
	mov ar.ec = 1
	mov pr.rot = (1 << 16);;

LOCAL(mpcopy_loop):
	(p16) ld8 r32 = [src],-8
	(p17) st8 [dst] = r33,-8
	br.ctop.dptk LOCAL(mpcopy_loop);;

dnl	epilogue
	(p17) st8 [dst] = r33,-8
	;;

	mov pr = saved_pr, -1
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mpcopy)


C_FUNCTION_BEGIN(mpadd)
	.prologue
	alloc saved_pfs = ar.pfs,3,5,0,8
	mov saved_lc = ar.lc
	mov saved_pr = pr
	sub sze = in0,r0,1;;

dnl	adjust addresses
	shladd dst = sze,3,in1
	shladd src = sze,3,in2
	shladd alt = sze,3,in1

dnl	prepare modulo-scheduled loop
	mov ar.lc = sze
	mov ar.ec = 2 
	mov pr.rot = ((1 << 16) | (1 << 19));;

	.body
LOCAL(mpadd_loop):
	.pred.rel.mutex p20,p22
	(p16) ld8 r32 = [alt],-8
	(p16) ld8 r35 = [src],-8
	(p20) add r36 = r33,r36
	(p22) add r36 = r33,r36,1
	;;
	(p20) cmp.leu p19,p21 = r33,r36
	(p22) cmp.ltu p19,p21 = r33,r36
	(p18) st8 [dst] = r37,-8
	br.ctop.dptk LOCAL(mpadd_loop);;

dnl	loop epilogue: final store
	(p18) st8 [dst] = r37,-8

dnl	return carry
	.pred.rel.mutex p20,p22
	(p20) add ret0 = r0,r0
	(p22) add ret0 = r0,r0,1
	;;
	mov pr = saved_pr, -1
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mpadd)


C_FUNCTION_BEGIN(mpsub)
	.prologue
	alloc saved_pfs = ar.pfs,3,5,0,8
	mov saved_lc = ar.lc
	mov saved_pr = pr
	sub sze = in0,r0,1;;

dnl	adjust addresses
	shladd dst = sze,3,in1
	shladd src = sze,3,in2
	shladd alt = sze,3,in1

dnl	prepare modulo-scheduled loop
	mov ar.lc = sze
	mov ar.ec = 2 
	mov pr.rot = ((1 << 16) | (1 << 19));;

	.body
LOCAL(mpsub_loop):
	.pred.rel.mutex p20,p22
	(p16) ld8 r32 = [alt],-8
	(p16) ld8 r35 = [src],-8
	(p20) sub r36 = r33,r36
	(p22) sub r36 = r33,r36,1
	;;
	(p20) cmp.geu p19,p21 = r33,r36
	(p22) cmp.gtu p19,p21 = r33,r36
	(p18) st8 [dst] = r37,-8
	br.ctop.dptk LOCAL(mpsub_loop);;

dnl	loop epilogue: final store
	(p18) st8 [dst] = r37,-8

dnl	return carry
	.pred.rel.mutex p20,p22
	(p20) add ret0 = r0,r0
	(p22) add ret0 = r0,r0,1
	;;
	mov pr = saved_pr, -1
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mpsub)


C_FUNCTION_BEGIN(mpmultwo)
	.prologue
	alloc saved_pfs = ar.pfs,2,6,0,8
	mov saved_lc = ar.lc
	mov saved_pr = pr
	sub sze = in0,r0,1;;

dnl	adjust addresses
	shladd dst = sze,3,in1
	shladd src = sze,3,in1

dnl	prepare modulo-scheduled loop
	mov ar.lc = sze
	mov ar.ec = 2 
	mov pr.rot = ((1 << 16) | (1 << 19));;

	.body
LOCAL(mpmultwo):
	.pred.rel.mutex p20,p22
	(p16) ld8 r32 = [src],-8
	(p20) add r36 = r33,r33
	(p22) add r36 = r33,r33,1
	;;
	(p20) cmp.leu p19,p21 = r33,r36
	(p22) cmp.ltu p19,p21 = r33,r36
	(p18) st8 [dst] = r37,-8
	br.ctop.dptk LOCAL(mpmultwo);;

dnl	loop epilogue: final store
	(p18) st8 [dst] = r37,-8

dnl	return carry
	.pred.rel.mutex p20,p22
	(p20) add ret0 = r0,r0
	(p22) add ret0 = r0,r0,1
	;;
	mov pr = saved_pr, -1
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mpmultwo)


C_FUNCTION_BEGIN(mpsetmul)
	.prologue
	alloc saved_pfs = ar.pfs,4,4,0,8
	mov saved_lc = ar.lc
	mov saved_pr = pr

	setf.sig f6 = in3
	setf.sig f7 = r0
	sub sze = in0,r0,1;;

dnl	adjust addresses
	shladd dst = sze,3,in1
	shladd src = sze,3,in2

dnl	prepare modulo-scheduled loop
	mov ar.lc = sze
	mov ar.ec = 3
	mov pr.rot = (1 << 16);;

	.body
LOCAL(mpsetmul_loop):
	(p16) ldf8 f32 = [src],-8
	(p18) stf8 [dst] = f35,-8
	(p17) xma.lu f34 = f6,f33,f7
	(p17) xma.hu f7  = f6,f33,f7
	br.ctop.dptk LOCAL(mpsetmul_loop);;

dnl	return carry
	getf.sig ret0 = f7;;

	mov pr = saved_pr, -1
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mpsetmul)


C_FUNCTION_BEGIN(mpaddmul)
	.prologue
	alloc saved_pfs = ar.pfs,4,4,0,8
	mov saved_lc = ar.lc
	mov saved_pr = pr

	setf.sig f6 = in3
	sub sze = in0,r0,1;;

dnl	adjust addresses
	shladd dst = sze,3,in1
	shladd src = sze,3,in2
	shladd alt = sze,3,in1;;

dnl	prepare the rotate-in carry
	mov r32 = r0

dnl	prepare modulo-scheduled loop
	mov ar.lc = sze
	mov ar.ec = 4
	mov pr.rot = ((1 << 16) | (1 << 21));;

	.body
LOCAL(mpaddmul_loop):
	.pred.rel.mutex p24,p26
	(p18) getf.sig r37 = f35
	(p24) add r35 = r38,r35
	(p17) xma.lu f34 = f6,f33,f37
	(p18) getf.sig r33 = f39
	(p26) add r35 = r38,r35,1
	(p17) xma.hu f38 = f6,f33,f37
	(p16) ldf8 f32 = [src],-8
	(p16) ldf8 f36 = [alt],-8
	;;
dnl	set carry from this operation
	(p24) cmp.leu p23,p25 = r38,r35
	(p26) cmp.ltu p23,p25 = r38,r35
	(p20) st8 [dst] = r36,-8
	br.ctop.dptk LOCAL(mpaddmul_loop);;

dnl	loop epilogue: final store
	(p20) st8 [dst] = r36,-8

dnl	return carry
	.pred.rel.mutex p24,p26
	(p24) add ret0 = r35,r0
	(p26) add ret0 = r35,r0,1

	mov pr = saved_pr, -1
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mpaddmul)


divert(-1)
C_FUNCTION_BEGIN(mpaddsqrtrc)
	.prologue
	alloc saved_pfs = ar.pfs,4,12,0,16
	mov saved_lc = ar.lc
	mov saved_pr = pr

	setf.sig f6 = in3
	sub sze = in0,r0,1;;

dnl	adjust addresses
dnl use two addresses for dst, and two for src
	shladd ? = sze,4,in1
	shladd ? = sze,4,in1
	shladd ? = sze,3,in2
	shladd ? = sze,3,in2;;

dnl	prepare the rotate-in carry
	mov r32 = r0

dnl	prepare modulo-scheduled loop
	mov ar.lc = sze
	mov ar.ec = 5
	mov pr.rot = ((1 << 16) | (1 << 22));;

	.body
LOCAL(mpaddsqrtrc_loop):
	(p16) ldf8 f32 = [src],-8
	(p17) xma.lu f34 = f33,f33,f37
	(p17) xma.hu f38 = f33,f33,f37
	(p18) getf.sig r32 = f35
	(p18) getf.sig r35 = f39
	(p18) ld8  rlo = [alt],-8
	.pred.rel.mutex p25,p29
	(p25) add r33 = r33,r??
	(p29) add r37 = r37,r??,1
	.pred.rel.mutex p27,p31
	(p27) add hi to carry
	(p31) add hi to carry+1
	;;
	(p16) ld8  r42 = [alt],-8
	(p25) cmpleu p24,p28 = lo
	(p29) cmpltu p24,p28 = lo
	(p20) st8 lo
	(p27) cmpleu p26,p30 = hi
	(p31) cmpltu p26,p30 = hi
	(p21) st8 hi
	;;
	br.ctop.dptk LOCAL(mpaddsqrtrc_loop);;

dnl	loop epilogue: final store
	(p21) st8 [dst] = r36,-8

	mov pr = saved_pr, -1
	mov ar.lc = saved_lc
	mov ar.pfs = saved_pfs
	br.ret.sptk b0
C_FUNCTION_END(mpaddsqrtrc)
divert(0)
