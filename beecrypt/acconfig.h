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

#undef PACKAGE
#undef VERSION

#define AIX			0
#define BEOS		0
#define CYGWIN		0
#define DARWIN		0
#define FREEBSD		0
#define HPUX		0
#define LINUX		0
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
#define NO_UNDERSCORES		0

#define JAVAGLUE	0

#undef HAVE_ERRNO_H
#undef HAVE_STRING_H
#undef HAVE_CTYPE_H
#undef HAVE_STDLIB_H
#undef HAVE_MTMALLOC_H

#undef HAVE_UNISTD_H
#undef HAVE_FCNTL_H

#undef HAVE_TIME_H

#undef HAVE_SYS_TYPES_H
#undef HAVE_SYS_STAT_H
#undef HAVE_SYS_TIME_H

#undef ENABLE_THREADS
#undef HAVE_THREAD_H
#undef HAVE_PTHREAD_H
#undef HAVE_SYNCH_H
#undef HAVE_SEMAPHORE_H

#undef ENABLE_AIO
#undef HAVE_AIO_H

#undef HAVE_TERMIO_H
#undef HAVE_TERMIOS_H

#undef HAVE_SYS_AUDIOIO_H
#undef HAVE_SYS_IOCTL_H
#undef HAVE_SYS_SOUNDCARD_H

#undef HAVE_GETTIMEOFDAY
#undef HAVE_GETHRTIME

#undef HAVE_DEV_DSP
#undef HAVE_DEV_AUDIO
#undef HAVE_DEV_RANDOM
#undef HAVE_DEV_URANDOM
#undef HAVE_DEV_TTY

#undef HAVE_LONG_LONG
#undef HAVE_UNSIGNED_LONG_LONG

#undef INT8_TYPE
#undef INT16_TYPE
#undef INT32_TYPE
#undef INT64_TYPE

#undef UINT8_TYPE
#undef UINT16_TYPE
#undef UINT32_TYPE
#undef UINT64_TYPE

#undef FLOAT4_TYPE
#undef DOUBLE8_TYPE

#if LINUX
#define _LIBC_REENTRANT
#endif

#ifndef __cplusplus
#undef inline
#endif
