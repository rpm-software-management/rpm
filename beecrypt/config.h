/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/*
 * acconfig.h
 *
 * acconfig.h pre-announces symbols defines by configure.in
 *
 * Copyright (c) 2001 Virtual Unlimited B.V.
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

#ifndef _REENTRANT
#define _REENTRANT
#endif

#define AIX			0
#define BEOS		0
#define CYGWIN		0
#define DARWIN		0
#define FREEBSD		0
#define HPUX		0
#define LINUX 1
#define MACOSX		0
#define NETBSD		0
#define OPENBSD		0
#define OSF			0
#define QNX			0
#define SCO_UNIX	0
#define SOLARIS		0
#ifndef WIN32
# define WIN32		0
#endif

#define LEADING_UNDERSCORE	0
#define NO_UNDERSCORES 1

#define JAVAGLUE 0

#define HAVE_ERRNO_H 1
#define HAVE_STRING_H 1
#define HAVE_CTYPE_H 1
#define HAVE_STDLIB_H 1
/* #undef HAVE_MTMALLOC_H */

#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1

#define HAVE_TIME_H 1

#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1

#define ENABLE_THREADS 1
/* #undef HAVE_THREAD_H */
#define HAVE_PTHREAD_H 1
/* #undef HAVE_SYNCH_H */
#define HAVE_SEMAPHORE_H 1

#define ENABLE_AIO 1
#define HAVE_AIO_H 1

#define HAVE_TERMIO_H 1
#define HAVE_TERMIOS_H 1

/* #undef HAVE_SYS_AUDIOIO_H */
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_SOUNDCARD_H 1

#define HAVE_GETTIMEOFDAY 1
/* #undef HAVE_GETHRTIME */

#define HAVE_DEV_DSP 1
/* #undef HAVE_DEV_AUDIO */
#define HAVE_DEV_RANDOM 1
#define HAVE_DEV_URANDOM 1
#define HAVE_DEV_TTY 1

#define HAVE_LONG_LONG 1
#define HAVE_UNSIGNED_LONG_LONG 1

/* #undef INT8_TYPE */
/* #undef INT16_TYPE */
/* #undef INT32_TYPE */
/* #undef INT64_TYPE */

/* #undef UINT8_TYPE */
/* #undef UINT16_TYPE */
/* #undef UINT32_TYPE */
/* #undef UINT64_TYPE */

/* #undef FLOAT4_TYPE */
/* #undef DOUBLE8_TYPE */

#if LINUX
#define _LIBC_REENTRANT
#endif

#ifndef __cplusplus
/* #undef inline */
#endif

/* Define if you have the <aio.h> header file. */
#define HAVE_AIO_H 1

/* Define if you have the <ctype.h> header file. */
#define HAVE_CTYPE_H 1

/* Define if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have the `mtmalloc' library (-lmtmalloc). */
/* #undef HAVE_LIBMTMALLOC */

/* Define if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Define if you have the `thread' library (-lthread). */
/* #undef HAVE_LIBTHREAD */

/* Define if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have the <mtmalloc.h> header file. */
/* #undef HAVE_MTMALLOC_H */

/* Define if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define if you have the <semaphore.h> header file. */
#define HAVE_SEMAPHORE_H 1

/* Define if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define if you have the <synch.h> header file. */
/* #undef HAVE_SYNCH_H */

/* Define if you have the <sys/audioio.h> header file. */
/* #undef HAVE_SYS_AUDIOIO_H */

/* Define if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/soundcard.h> header file. */
#define HAVE_SYS_SOUNDCARD_H 1

/* Define if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define if you have the <termio.h> header file. */
#define HAVE_TERMIO_H 1

/* Define if you have the <thread.h> header file. */
/* #undef HAVE_THREAD_H */

/* Define if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Name of package */
#define PACKAGE "beecrypt"

/* The size of a `char', as computed by sizeof. */
#define SIZEOF_CHAR 1

/* The size of a `double', as computed by sizeof. */
#define SIZEOF_DOUBLE 8

/* The size of a `float', as computed by sizeof. */
#define SIZEOF_FLOAT 4

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of a `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of a `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of a `unsigned char', as computed by sizeof. */
#define SIZEOF_UNSIGNED_CHAR 1

/* The size of a `unsigned int', as computed by sizeof. */
#define SIZEOF_UNSIGNED_INT 4

/* The size of a `unsigned long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG 4

/* The size of a `unsigned long long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG_LONG 8

/* The size of a `unsigned short', as computed by sizeof. */
#define SIZEOF_UNSIGNED_SHORT 2

/* Define if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "2.2.0"

/* Define if your processor stores words with the most significant byte first
   (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define as `__inline' if that's what the C compiler calls it, or to nothing
   if it is not supported. */
/* #undef inline */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */
