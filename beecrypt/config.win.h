/*
 * config.win.h
 *
 * Win32 config file
 *
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
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

#ifndef _CONFIG_WIN_H
#define _CONFIG_WIN_H

#define _REENTRANT

#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0400
#endif

#include <windows.h>

#if __MWERKS__
# if __INTEL__
#  define WORDS_BIGENDIAN		0
# else
#  error Unknown CPU type in MetroWerks CodeWarrior
# endif
#elif defined(_MSC_VER)
# if defined(_M_IX86)
#  define WORDS_BIGENDIAN		0
#  define ROTL32(x, s) _rotl(x, s)
#  define ROTR32(x, s) _rotr(x, s)
# else
#  error Unknown CPU type in Microsoft Visual C
# endif
#else
# error Unknown compiler for WIN32
#endif

#if defined(_MSC_VER) || __MWERKS__
#define HAVE_ERRNO_H			1
#define HAVE_STRING_H			1
#define HAVE_STDLIB_H			1
#define HAVE_CTYPE_H			1
#define HAVE_FCNTL_H			1
#define HAVE_TIME_H				1

#define HAVE_SYS_TYPES_H		0
#define HAVE_SYS_STAT_H			0
#define HAVE_SYS_TIME_H			0

#define HAVE_THREAD_H			0
#define HAVE_SYNCH_H			0
#define HAVE_PTHREAD_H			0
#define HAVE_SEMAPHORE_H		0

#define HAVE_TERMIO_H			0
#define HAVE_SYS_AUDIOIO_H		0
#define HAVE_SYS_IOCTL_H		0
#define HAVE_SYS_SOUNDCARD_H	0

#define HAVE_GETTIMEOFDAY		0
#define HAVE_GETHRTIME			0

#define HAVE_DEV_TTY			0
#define HAVE_DEV_AUDIO			0
#define HAVE_DEV_DSP			0
#define HAVE_DEV_RANDOM			0
#define HAVE_DEV_URANDOM		0
#define HAVE_DEV_TTY			0

#else
#error Not set up for this compiler
#endif

#if __MWERKS__
#define HAVE_UNISTD_H			1
#define HAVE_MALLOC_H			1

#define HAVE_LONG_LONG			1

#define INT8_TYPE		char
#define INT16_TYPE		short
#define INT32_TYPE		int
#define INT64_TYPE		long long
#define UINT8_TYPE		unsigned char
#define UINT16_TYPE		unsigned short
#define UINT32_TYPE		unsigned int
#define UINT64_TYPE		unsigned long long
#define FLOAT4_TYPE		float
#define DOUBLE8_TYPE	double

#elif defined(_MSC_VER)
#define HAVE_UNISTD_H			0
#define HAVE_MALLOC_H			1

#define HAVE_LONG_LONG			0

#define INT8_TYPE		__int8
#define INT16_TYPE		__int16
#define INT32_TYPE		__int32
#define INT64_TYPE		__int64
#define UINT8_TYPE		unsigned __int8
#define UINT16_TYPE		unsigned __int16
#define UINT32_TYPE		unsigned __int32
#define UINT64_TYPE		unsigned __int64
#define FLOAT4_TYPE		float
#define DOUBLE8_TYPE	double
#endif

#endif
