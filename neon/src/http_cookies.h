/* 
   HTTP Request Handling
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

#ifndef COOKIES_H
#define COOKIES_H

struct http_cookie_s;
typedef struct http_cookie_s http_cookie;

struct http_cookie_s {
    char *name, *value;
    unsigned int secure:1;
    unsigned int discard:1;
    char *domain, *path;
    time_t expiry; /* time at which the cookie expires */
    http_cookie *next;
};

typedef struct {
    http_cookie *cookies;
} http_cookie_cache;

extern http_request_hooks http_cookie_hooks;

#endif /* COOKIES_H */
