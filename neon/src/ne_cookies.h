/* 
   HTTP Request Handling
   Copyright (C) 1999-2001, Joe Orton <joe@light.plus.com>

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

#ifndef NE_COOKIES_H
#define NE_COOKIES_H

#include "ne_request.h"
#include "ne_defs.h"

BEGIN_NEON_DECLS

struct ne_cookie_s;
typedef struct ne_cookie_s ne_cookie;

struct ne_cookie_s {
    char *name, *value;
    unsigned int secure:1;
    unsigned int discard:1;
    char *domain, *path;
    time_t expiry; /* time at which the cookie expires */
    ne_cookie *next;
};

typedef struct {
    ne_cookie *cookies;
} ne_cookie_cache;

extern ne_request_hooks ne_cookie_hooks;

END_NEON_DECLS

#endif /* NE_COOKIES_H */
