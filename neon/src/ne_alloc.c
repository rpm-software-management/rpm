/* 
   Replacement memory allocation handling etc.
   Copyright (C) 1999-2000, Joe Orton <joe@orton.demon.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

   Id: ne_alloc.c,v 1.2 2000/08/02 10:23:03 joe Exp 
*/

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "ne_alloc.h"

void *ne_malloc(size_t len) 
{
    void *ptr = malloc(len);
    if (!ptr) {
	/* uh-oh */
	abort();
    }
    return ptr;
}

void *ne_calloc(size_t len) 
{
    return memset(ne_malloc(len), 0, len);
}

char *ne_strdup(const char *s) 
{
    return strcpy(ne_malloc(strlen(s) + 1), s);
}

char *ne_strndup(const char *s, size_t n)
{
    char *new = ne_malloc(n + 1);
    new[n] = '\0';
    memcpy(new, s, n);
    return new;
}
