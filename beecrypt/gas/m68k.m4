dnl  m68k.m4
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
 
ifelse(REGISTERS_NEED_PERCENT,yes,`
define(d0,%d0)
define(d1,%d1)
define(d2,%d2)
define(d3,%d3)
define(d4,%d4)
define(d5,%d5)
define(d6,%d6)
define(d7,%d7)
define(a0,%a0)
define(a1,%a1)
define(a2,%a2)
define(a3,%a3)
define(a4,%a4)
define(a5,%a5)
define(a6,%a6)
define(a7,%a7)
define(sp,%sp)
')
ifelse(INSTRUCTIONS_NEED_DOT_SIZE_QUALIF,yes,`
define(addal,adda.l)
define(addl,add.l)
define(addql,addq.l)
define(addxl,addx.l)
define(clrl,clr.l)
define(lsll,lsl.l)
define(movel,move.l)
define(moveml,movem.l)
define(moveal,movea.l)
define(umull,umul.l)
define(subl,sub.l)
define(subql,subq.l)
define(subxl,subx.l)
')
