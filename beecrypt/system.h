/**
 * \file beecrypt/system.h
 */

#ifndef	H_SYSTEM
#define	H_SYSTEM

#define BEECRYPT_DLL_EXPORT

#if defined(_WIN32) && !defined(WIN32)
# define WIN32 1
#endif

#if WIN32 && !__CYGWIN32__
# include "config.win.h"
#else
# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif
#endif

#if defined(__LCLINT__)
/* XXX from /usr/include/bits/sigest.h in glibc-2.2.4 */
/*@-sizeoftype@*/
# define _SIGSET_NWORDS (1024 / (8 * sizeof (unsigned long int)))
/*@-redef@*/
typedef struct
  {
    unsigned long int __val[_SIGSET_NWORDS];
  } __sigset_t;
/*@=redef@*/
/*@=sizeoftype@*/
#endif

#include "beecrypt.gnu.h"

#if HAVE_SYS_STAT_H
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include <stdio.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_TIME_H
# include <time.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#if HAVE_ERRNO_H
# include <errno.h>
#endif

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif

#if HAVE_CTYPE_H
# include <ctype.h>
#endif

#if HAVE_MALLOC_H && !defined(__LCLINT__)
# include <malloc.h>
#endif

#endif	/* H_SYSTEM */
