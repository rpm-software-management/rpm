dnl  mpopt.s390x.m4
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


divert(-1)
dnl r2 contains count -> move elsewhere; return register = carry
dnl r3 contains result
dnl r4 contains data
dnl r5 contains multiplier
dnl r6 index; start value = (count << 3) - 8
dnl r7 zero register
dnl r0,r1 free for computations
C_FUNCTION_BEGIN(mpaddmul)
	stmg %r6,%r7,48(%r15)
	sllg %r6,%r2,3
	xgr %r7,%r7
	xgr %r2,%r2
	
LOCAL(mpaddmul_loop):
	lgr %r1,%r5
	mlg %r0,0(%r4,%r6)
	algr %r1,%r2
	alcgr %r0,%r7
	alg %r1,0(%r3,%r6)
	alcgr %r0,%r7
	stg %r1,0(%r3,%r6)
	lgr %r2,%r0
	aghi %r6,-8
	jle LOCAL(mpaddmul_loop)
	lmg %r6,%r7,48(%r15)
	br %r14
C_FUNCTION_END(mpaddmul)
divert(0)
