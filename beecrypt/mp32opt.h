/** \ingroup MP_m
 * \file mp32opt.h
 *
 * Multiprecision integer assembler-optimized routined for 32 bit cpu, header.
 */

/*
 * Copyright (c) 1999, 2000, 2001 Virtual Unlimited B.V.
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
# if __MWERKS__ && __INTEL__
#  define ASM_MP32ZERO
#  define ASM_MP32FILL
#  define ASM_MP32EVEN
#  define ASM_MP32ODD
#  define ASM_MP32ADDW
#  define ASM_MP32ADD
#  define ASM_MP32SUBW
#  define ASM_MP32SUB
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
#  define ASM_MP32ADDSQRTRC
# elif defined(_MSC_VER) && defined(_M_IX86)
#  define ASM_MP32ZERO
#  define ASM_MP32FILL
#  define ASM_MP32EVEN
#  define ASM_MP32ODD
#  define ASM_MP32ADDW
#  define ASM_MP32ADD
#  define ASM_MP32SUBW
#  define ASM_MP32SUB
#  define ASM_MP32DIVTWO
#  define ASM_MP32MULTWO
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
#  define ASM_MP32ADDSQRTRC
# endif
#endif

#if defined(__GNUC__)
# if defined(OPTIMIZE_ARM)
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
# endif
# if defined(OPTIMIZE_I386) || defined(OPTIMIZE_I486) || defined(OPTIMIZE_I586) || defined(OPTIMIZE_I686)
#  define ASM_MP32ZERO
#  define ASM_MP32FILL
#if 0	/* XXX BROKEN! */
#  define ASM_MP32EVEN
#  define ASM_MP32ODD
#endif
#  define ASM_MP32ADDW
#  define ASM_MP32ADD
#  define ASM_MP32SUBW
#  define ASM_MP32SUB
#  define ASM_MP32DIVTWO
#  define ASM_MP32MULTWO
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
#  define ASM_MP32ADDSQRTRC
# endif
# if defined(OPTIMIZE_IA64)
#  define ASM_MP32ZERO
#  define ASM_MP32COPY
#  define ASM_MP32ADD
#  define ASM_MP32SUB
#  undef ASM_MP32SETMUL
#  undef ASM_MP32ADDMUL
# endif
# if defined(OPTIMIZE_POWERPC)
#  define ASM_MP32ADDW
#  define ASM_MP32ADD
#  define ASM_MP32SUBW
#  define ASM_MP32SUB
#  define ASM_MP32MULTWO
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
#  define ASM_MP32ADDSQRTRC
# endif
# if defined(OPTIMIZE_SPARCV8)
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
# endif
# if defined(OPTIMIZE_SPARCV8PLUS) || defined(OPTIMIZE_SPARCV9)
#  define ASM_MP32ADDW
#  define ASM_MP32ADD
#  define ASM_MP32SUBW
#  define ASM_MP32SUB
#  define ASM_MP32MULTWO
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
#  define ASM_MP32ADDSQRTRC
# endif
#endif

#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
# if defined(OPTIMIZE_SPARCV8PLUS) /* || defined(OPTIMIZE_SPARCV9) */
#  define ASM_MP32ADDW
#  define ASM_MP32ADD
#  define ASM_MP32SUBW
#  define ASM_MP32SUB
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
#  define ASM_MP32ADDSQRTRC
#  endif
# if defined(OPTIMIZE_I386) || defined(OPTIMIZE_I486) || defined(OPTIMIZE_I586) || defined(OPTIMIZE_I686)
#  define ASM_MP32ADDW
#  define ASM_MP32ADD
#  define ASM_MP32SUBW
#  define ASM_MP32SUB
#  define ASM_MP32MULTWO
#  define ASM_MP32SETMUL
#  define ASM_MP32ADDMUL
#  define ASM_MP32ADDSQRTRC
# endif
#endif

#ifdef __cplusplus
}
#endif

#endif
