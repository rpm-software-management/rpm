/*
memset.c - dumb and inefficient replacement for memset(3).
Copyright (C) 1995, 1996 Michael Riepe <michael@stud.uni-hannover.de>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#if STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
extern void bcopy();
extern void *memcpy();
#endif

#if !HAVE_MEMCPY
# define memcpy(d,s,n)	bcopy(s,d,n)
#endif

void*
__memset(void *s, int c, size_t n) {
    if (n > 64) {
	__memset((char*)s + n / 2, c, n - n / 2);
	memcpy(s, (char*)s + n / 2, n / 2);
    }
    else {
	while (n > 0) {
	    ((char*)s)[--n] = c;
	}
    }
}
