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

#ifdef __cplusplus
extern "C" {
#endif

#if WIN32
# if defined(_MSC_VER) && defined(_M_IX86)
#  define ASM_AESENCRYPT
#  define ASM_AESDECRYPT
# elif __INTEL__ && __MWERKS__
#  undef ASM_AESENCRYPT
#  undef ASM_AESDECRYPT
# endif
#endif

#if defined(__GNUC__)
# if defined(OPTIMIZE_I586) || defined(OPTIMIZE_I686)
#  define ASM_AESENCRYPT
#  define ASM_AESDECRYPT
# endif
#endif

#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#if defined(OPTIMIZE_I586) || defined(OPTIMIZE_I686)
#define ASM_AESENCRYPT
#define ASM_AESDECRYPT
#endif               
#endif               

#ifdef __cplusplus
}
#endif

#endif
