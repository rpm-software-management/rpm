/* 
   HTTP URI handling
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

#ifndef URI_H
#define URI_H

/* Un-escapes a URI. Returns malloc-allocated URI on success,
 * or NULL on failure (malloc failure or invalid %<HEX><HEX> sequence). */
char *uri_unescape( const char *uri );

/* Escapes the abspath segment of a URI.
 * Returns malloc-allocated string on success, or NULL on malloc failure.
 */
char *uri_abspath_escape( const char *abs_path );

const char *uri_abspath( const char *uri );

char *uri_parent( const char *uri );

int uri_compare( const char *a, const char *b );

/* Returns an absolute URI from a possibly-relative 'uri', using
 * given scheme + hostport segment.
 * Returns malloc-allocated string on success, or NULL on malloc failure. */
char *uri_absolute( const char *uri, const char *scheme, 
		    const char *hostport );

/* Returns non-zero if child is a child of parent */
int uri_childof( const char *parent, const char *child );

int uri_has_trailing_slash( const char *uri );

struct uri {
    char *scheme;
    char *host;
    int port;
    char *path;
};

int uri_parse( const char *uri, struct uri *parsed, 
	       const struct uri *defaults );

void uri_free( struct uri *parsed );

#endif URI_H
