dnl  asmdefs.m4
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

ifelse(substr(ASM_OS,0,5),linux,`
define(USE_SIZE_DIRECTIVE,yes)
define(USE_TYPE_DIRECTIVE,yes)
')

define(SYMNAME,`GSYM_PREFIX`$1'')
define(LOCAL,`LSYM_PREFIX`$1'')

ifdef(`ALIGN',,`define(`ALIGN',`')')

ifelse(USE_TYPE_DIRECTIVE,yes,`
ifelse(SUBSTR(ASM_ARCH,0,3),arm,`
define(FUNCTION_TYPE,`function')
',`
ifelse(SUBSTR(ASM_ARCH,0,5),sparc,`
define(FUNCTION_TYPE,`#function')
',`
define(FUNCTION_TYPE,`@function')
')
')
define(C_FUNCTION_BEGIN,`
	TEXTSEG
	ALIGN
	GLOBL SYMNAME($1)
	.type SYMNAME($1),FUNCTION_TYPE
SYMNAME($1):
')
',`
define(C_FUNCTION_BEGIN,`
	TEXTSEG
	ALIGN
	GLOBL SYMNAME($1)
SYMNAME($1):
')
')

ifelse(USE_SIZE_DIRECTIVE,yes,`
define(C_FUNCTION_END,`
LOCAL($1)_size:
	.size SYMNAME($1), LOCAL($1)_size  - SYMNAME($1)
')
',`
define(C_FUNCTION_END,`')
')
