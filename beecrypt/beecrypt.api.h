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

/* Starting from GCC 3.2, the compiler seems smart enough to figure
 * out that we're trying to do a rotate without having to specify it.
 */
#if defined(__GNUC__) && (__GNUC__ < 3 || __GNUC_MINOR__ < 2)
# if defined(__i386__)
static inline uint32_t _rotl32(uint32_t x, const unsigned char n)
{
	__asm__("roll %2,%0"
		:	"=r" (x)
		:	"0" (x), "I" (n));

	return x;
}

#define ROTL32(x, n) _rotl32(x, n)

static inline uint32_t _rotr32(uint32_t x, const unsigned char n)
{
	__asm__("rorl %2,%0"
		:	"=r" (x)
		:	"0" (x), "I" (n));

	return x;
}

#define ROTR32(x, n) _rotr32(x, n)

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

#if (MP_WBITS == 64)
typedef uint64_t	mpw;
typedef uint32_t	mphw;
#elif (MP_WBITS == 32)
# if HAVE_UINT64_T
#  define HAVE_MPDW 1
typedef uint64_t	mpdw;
# endif
typedef uint32_t	mpw;
typedef uint16_t	mphw;
#else
# error
#endif

#endif
