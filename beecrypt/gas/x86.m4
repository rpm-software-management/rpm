dnl  x86.m4
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

ifelse(substr(ASM_ARCH,0,6),athlon,`
define(USE_BSWAP)
')
ifelse(substr(ASM_ARCHi,0,7),pentium,`
define(USE_BSWAP)
')
ifelse(ASM_ARCH,i586,`
define(USE_BSWAP)
')
ifelse(ASM_ARCH,i686,`
define(USE_BSWAP)
')
ifelse(ASM_ARCH,pentium4,`
define(USE_BSWAP)
define(USE_SSE2)
')
