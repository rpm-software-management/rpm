/*
 * config.win.h
 *
 * Win32 config file
 *
 * Copyright (c) 2000, Virtual Unlimited B.V.
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

#if __INTEL__
#define WORDS_BIGENDIAN		0
#else
#error Trying to compile for WIN32 on non-Intel hardware
#endif

#if __MWERKS__
#define HAVE_ERRNO_H		1
#define HAVE_STRING_H		1
#define HAVE_STDLIB_H		1
#define HAVE_CTYPE_H		1
#define HAVE_UNISTD_H		1
#define HAVE_FCNTL_H		1
#define HAVE_TIME_H			1

#define HAVE_SYS_TYPES_H	0
#define HAVE_SYS_STAT_H		0
#define HAVE_SYS_TIME_H		0

#define HAVE_THREAD_H		0
#define HAVE_PTHREAD_H		0
#define HAVE_SYNCH_H		0

#define HAVE_TERMIO_H			0
#define HAVE_SYS_AUDIOIO_H		0
#define HAVE_SYS_IOCTL_H		0
#define HAVE_SYS_SOUNDCARD_H	0

#define HAVE_GETTIMEOFDAY	0
#define HAVE_GETHRTIME		0

#define HAVE_DEV_TTY		0
#define HAVE_DEV_AUDIO		0
#define HAVE_DEV_DSP		0
#define HAVE_DEV_RANDOM		0

#define SIZEOF_CHAR					1
#define SIZEOF_UNSIGNED_CHAR		1
#define SIZEOF_SHORT				2
#define SIZEOF_UNSIGNED_SHORT		2
#define SIZEOF_INT					4
#define SIZEOF_UNSIGNED_INT			4
#define SIZEOF_LONG					4
#define SIZEOF_UNSIGNED_LONG		4
#define SIZEOF_LONG_LONG			8
#define SIZEOF_UNSIGNED_LONG_LONG	8

#define SIZEOF_FLOAT				4
#define SIZEOF_DOUBLE				8
#else
#error Not set up for this compiler
#endif

#if (SIZEOF_CHAR == 1)
typedef char int8;
typedef char javabyte;
#else
#error sizeof(char) not 1
#endif

#if (SIZEOF_SHORT == 2)
typedef short int16;
typedef short javashort;
#else
#error sizeof(short) is not 2
#endif

#if (SIZEOF_INT == 4)
typedef int int32;
typedef int javaint;
#elif (SIZEOF_LONG == 4)
typedef int int32;
typedef long javaint;
#else
#error compiler has no 32 bit integer
#endif

#if (SIZEOF_LONG == 8)
typedef long int64;
typedef long javalong;
#elif (SIZEOF_LONG_LONG == 8)
typedef long long int64;
typedef long long javalong;
#else
#error compiler has no 64 bit integer
#endif

#if (SIZEOF_FLOAT == 4)
typedef float javafloat;
#else
#error compiler has no 32 bit float
#endif

#if (SIZEOF_DOUBLE == 8)
typedef double javadouble;
#else
#error compiler has no 64 bit double;
#endif

#if (SIZEOF_UNSIGNED_CHAR == 1)
typedef unsigned char uint8;
#else
#error sizeof(unsigned char) is not 1
#endif

#if (SIZEOF_UNSIGNED_SHORT == 2)
typedef unsigned short uint16;
typedef unsigned short javachar;
typedef unsigned short unicode;
#else
#error sizeof(unsigned short) is not 2
#endif

#if (SIZEOF_UNSIGNED_INT == 4)
typedef unsigned int uint32;
#elif (SIZEOF_UNSIGNED_LONG == 4)
typedef unsigned long uint32;
#else
#error compiler has no 32 bit unsigned integer
#endif

#if (SIZEOF_UNSIGNED_LONG == 8)
typedef unsigned long uint64;
#elif (SIZEOF_UNSIGNED_LONG_LONG == 8)
typedef unsigned long long uint64;
#else
#error compiler has no 64 bit unsigned integer
#endif

/* typedef uint8 byte */

#endif
