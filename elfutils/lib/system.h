/* Copyright (C) 2000, 2002 Free Software Foundation, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef LIB_SYSTEM_H
#define LIB_SYSTEM_H	1

#include <stddef.h>
#include <stdint.h>

extern void *xmalloc (size_t) __attribute__ ((__malloc__))
	/*@*/;
extern void *xcalloc (size_t, size_t) __attribute__ ((__malloc__))
	/*@*/;
extern void *xrealloc (void *, size_t) __attribute__ ((__malloc__))
	/*@*/;

extern char *xstrdup (const char *) __attribute__ ((__malloc__))
	/*@*/;
extern char *xstrndup (const char *, size_t) __attribute__ ((__malloc__))
	/*@*/;


extern uint32_t crc32 (uint32_t crc, unsigned char *buf, size_t len)
	/*@*/;


/* A special gettext function we use if the strings are too short.  */
#define sgettext(Str) \
  ({ const char *__res = strrchr (gettext (Str), '|');			      \
     __res ? __res + 1 : Str; })

#define gettext_noop(Str) Str

#endif /* system.h */
