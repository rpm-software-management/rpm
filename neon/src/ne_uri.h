/* 
   HTTP URI handling
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

#ifndef NE_URI_H
#define NE_URI_H

#include "ne_defs.h"

BEGIN_NEON_DECLS

/* Un-escapes a URI. Returns malloc-allocated URI on success,
 * or NULL on failure (malloc failure or invalid %<HEX><HEX> sequence). */
char *uri_unescape(const char *uri);

/* Escapes the abspath segment of a URI.
 * Returns malloc-allocated string on success, or NULL on malloc failure.
 */
char *uri_abspath_escape(const char *abs_path);

/* Returns abspath segment in (absolute) uri */
const char *uri_abspath(const char *uri);

/* Returns parent of path */
char *uri_parent(const char *path);

/* Returns strcmp-like value giving comparison between a and b,
 * ignoring trailing-slashes. */
int uri_compare(const char *a, const char *b);

/* Returns an absolute URI from a possibly-relative 'uri', using
 * given scheme + hostport segment.
 * Returns malloc-allocated string on success, or NULL on malloc failure. */
char *uri_absolute(const char *uri, const char *scheme, const char *hostport);

/* Returns non-zero if child is a child of parent */
int uri_childof(const char *parent, const char *child);

/* Returns non-zero if uri has a trailing slash character */
int uri_has_trailing_slash(const char *uri);

struct uri {
    char *scheme;
    char *host;
    int port;
    char *path;
    char *authinfo;
};

/* Parse 'uri' and place parsed segments in *parsed. */
int uri_parse(const char *uri, struct uri *parsed, const struct uri *defaults);

void uri_free(struct uri *parsed);

END_NEON_DECLS

#endif /* NE_URI_H */

