/*
 * Copyright (c) 2001, 2002 Virtual Unlimited B.V.
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

/*!\file beecrypt.api.h
 * \brief BeeCrypt API, portability headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 */

#ifndef _BEECRYPT_API_H
#define _BEECRYPT_API_H

#if defined(_WIN32) && !defined(WIN32)
# define WIN32 1
#endif

#if WIN32 && !__CYGWIN32__
# include "beecrypt.win.h"
# ifdef BEECRYPT_DLL_EXPORT
#  define BEECRYPTAPI __declspec(dllexport)
# else
#  define BEECRYPTAPI __declspec(dllimport)
# endif
#else
# include "beecrypt.gnu.h"
# define BEECRYPTAPI
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
# include <stdint.h>
# endif
#endif

#if defined(__GNUC__)
# if defined(__i386__)
static inline uint32_t ROTL32(uint32_t x, const unsigned char n)
{
	__asm__("roll %1,%0"
		:	"=r" (x)
		:	"0" (x), "I" (n));

	return x;
}

static inline uint32_t ROTR32(uint32_t x, const unsigned char n)
{
	__asm__("rorl %1,%0"
		:	"=r" (x)
		:	"0" (x), "I" (n));

	return x;
}
# elif defined(__powerpc__)
static inline uint32_t ROTL32(uint32_t x, const unsigned char n)
{
	register uint32_t r;

	__asm__("rotlwi %0,%1,%2"
		:	"=r" (r)
		:	"r" (x), "I" (n));

	return r;
}

static inline uint32_t ROTR32(uint32_t x, const unsigned char n)
{
	register uint32_t r;

	__asm__("rotrwi %0,%1,%2"
		:	"=r" (r)
		:	"r" (x), "I" (n));

	return r;
}
# endif
#endif

#ifndef ROTL32
# define ROTL32(x, s) (((x) << (s)) | ((x) >> (32 - (s))))
#endif
#ifndef ROTR32
# define ROTR32(x, s) (((x) >> (s)) | ((x) << (32 - (s))))
#endif

typedef uint8_t		byte;

typedef int8_t		javabyte;
typedef int16_t		javashort;
typedef int32_t		javaint;
typedef int64_t		javalong;

typedef uint16_t	javachar;

#endif
