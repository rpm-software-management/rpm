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

*/

#ifndef NE_ALLOC_H
#define NE_ALLOC_H

#include <sys/types.h>

void *ne_malloc(size_t len);
void *ne_calloc(size_t len);

char *ne_strdup(const char *s);

char *ne_strndup(const char *s, size_t n);

#endif /* NE_ALLOC_H */
