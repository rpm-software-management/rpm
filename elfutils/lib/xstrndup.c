/* Copy a string with out of memory checking
   Copyright (C) 1990, 1996, 1997, 1999, 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined STDC_HEADERS || defined HAVE_STRING_H || _LIBC
# include <string.h>
#else
# include <strings.h>
#endif
void *xmalloc __P ((size_t n));
char *xstrndup __P ((char *string, size_t n));

/* Return a newly allocated copy of STRING.  */

char *
xstrndup (string, n)
     char *string;
     size_t n;
{
  char *res;
  size_t len = strnlen (string, n);
  if (len > n)
    len = n;
  res = memcpy (xmalloc (len + 1), string, len);
  res[len] = '\0';
  return res;
}
