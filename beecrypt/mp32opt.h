/*
 * mp32opt.h
 *
 * Multiprecision integer assembler-optimized routined for 32 bit cpu, header
 *
 * Copyright (c) 1999-2000 Virtual Unlimited B.V.
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

#ifndef _MP32OPT_H
#define _MP32OPT_H

#include "beecrypt.h"

#ifdef __cplusplus
extern "C" {
#endif

#if WIN32
#if __INTEL__ && __MWERKS__
#define ASM_MP32ADDW
#define ASM_MP32ADD
#define ASM_MP32SUBW
#define ASM_MP32SUB
#define ASM_MP32MULTWO
#define ASM_MP32SETMUL
#define ASM_MP32ADDMUL
#define ASM_MP32ADDSQRTRC
#endif
#endif

#ifdef __GNUC__
#ifdef i386
#define ASM_MP32ADDW
#define ASM_MP32ADD
#define ASM_MP32SUBW
#define ASM_MP32SUB
#define ASM_MP32MULTWO
#define ASM_MP32SETMUL
#define ASM_MP32ADDMUL
#define ASM_MP32ADDSQRTRC
#endif
#ifdef powerpc
#define ASM_MP32ADDW
#define ASM_MP32ADD
/* there's still a problem to be fixed in the subtraction routines */
#undef ASM_MP32SUBW
#undef ASM_MP32SUB
#define ASM_MP32SETMUL
#define ASM_MP32ADDMUL
#define ASM_MP32ADDSQRTRC
#endif
#endif

#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#if defined(sparcv9) || defined(sparcv8plus)
#define ASM_MP32ADDW
#define ASM_MP32ADD
#define ASM_MP32SUBW
#define ASM_MP32SUB
#define ASM_MP32SETMUL
#define ASM_MP32ADDMUL
#undef ASM_MP32ADDSQRTRC
# endif
#endif

#ifdef __cplusplus
}
#endif

#endif
