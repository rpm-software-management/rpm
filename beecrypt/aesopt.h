/*
 * Copyright (c) 2002 Bob Deblier
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

/*!\file aesopt.h
 * \brief AES block cipher, assembler-optimized routines, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup BC_aes_m
 */

#ifndef _AESOPT_H
#define _AESOPT_H

#include "beecrypt/beecrypt.h"
#include "beecrypt/aes.h"

#ifdef __cplusplus
extern "C" {
#endif

#if WIN32
# if defined(_MSC_VER) && defined(_M_IX86)
/* this space intentionally left blank */
# elif __INTEL__ && __MWERKS__
# endif
#endif

#if defined(__GNUC__)
# if defined(OPTIMIZE_I586) || defined(OPTIMIZE_I686)
#  if defined(OPTIMIZE_MMX)
#   define ASM_AESENCRYPT
#   define ASM_AESENCRYPTECB
#   define ASM_AESDECRYPT
#   define ASM_AESDECRYPTECB
#  endif
# endif
#endif

#if defined(__INTEL_COMPILER)
# if defined(OPTIMIZE_I586) || defined(OPTIMIZE_I686)
#  if defined(OPTIMIZE_MMX)
#   define ASM_AESENCRYPT
#   define ASM_AESENCRYPTECB
#   define ASM_AESDECRYPT
#   define ASM_AESDECRYPTECB
# endif
# endif
#endif

#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
/* this space intentionally left blank */
#endif               

#ifdef __cplusplus
}
#endif

#endif
