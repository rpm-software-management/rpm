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


C_FUNCTION_BEGIN(mpsetmul)
	stmg %r6,%r7,48(%r15)
	sllg %r6,%r2,3
	aghi %r6,-8
	xgr %r2,%r2
	xgr %r7,%r7

LOCAL(mpsetmul_loop):
	lgr %r1,%r5
	mlg %r0,0(%r4,%r6)
	algr %r1,%r2
	alcgr %r0,%r7
	stg %r1,0(%r3,%r6)
	lgr %r2,%r0
	aghi %r6,-8
	jhe LOCAL(mpsetmul_loop)

	lmg %r6,%r7,48(%r15)
	br %r14
C_FUNCTION_END(mpsetmul)


C_FUNCTION_BEGIN(mpaddmul)
	stmg %r6,%r7,48(%r15)
	sllg %r6,%r2,3
	aghi %r6,-8
	xgr %r2,%r2
	xgr %r7,%r7
	
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
	jhe LOCAL(mpaddmul_loop)

	lmg %r6,%r7,48(%r15)
	br %r14
C_FUNCTION_END(mpaddmul)


divert(-1)
dnl function fails; illegal instruction on mlgr
dnl I've tried many alternative, but nothing seems to work so far
C_FUNCTION_BEGIN(mpaddsqrtrc)
	stmg %r6,%r7,48(%r15)
	sllg %r5,%r2,3
	sllg %r6,%r2,4
	aghi %r5,-8
	aghi %r6,-16
	xgr %r2,%r2
	xgr %r7,%r7

LOCAL(mpaddsqrtrc_loop):
	lg %r1,0(%r4,%r5)
	mlgr %r1,%r1
	algr %r1,%r2
	alcgr %r0,%r7
	xgr %r2,%r2
	alg %r1,8(%r3,%r6)
	alcg %r0,0(%r3,%r6)
	alcgr %r2,%r7
	stg %r1,8(%r3,%r6)
	stg %r0,0(%r3,%r6)
	aghi %r5,-8
	jhe LOCAL(mpaddsqrtrc_loop)

	lmg %r6,%r7,48(%r15)
	br %r14
C_FUNCTION_END(mpaddsqrtrc)
divert(0)
