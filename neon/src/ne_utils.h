/* 
   HTTP utility functions
   Copyright (C) 1999-2002, Joe Orton <joe@manyfish.co.uk>

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

#ifndef NE_UTILS_H
#define NE_UTILS_H

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>

#include "ne_defs.h"

#ifdef NEON_TRIO
/* no HAVE_TRIO_H check so this works from outside neon build tree. */
#include <trio.h>
#endif

BEGIN_NEON_DECLS

/* Returns a human-readable version string like:
 * "neon 0.2.0: Library build, OpenSSL support"
 */
const char *ne_version_string(void);

/* Returns non-zero if library version is not of major version
 * 'major', or if minor version is not greater than or equal to
 * 'minor'. */
int ne_version_match(int major, int minor);

/* Returns non-zero if neon has support for SSL. */
int ne_supports_ssl(void);

/* Use replacement snprintf's if trio is being used. */
#ifdef NEON_TRIO
#define ne_snprintf trio_snprintf
#define ne_vsnprintf trio_vsnprintf
#else
#define ne_snprintf snprintf
#define ne_vsnprintf vsnprintf
#endif

#ifndef WIN32
#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

/* CONSIDER: mutt has a nicer way of way of doing debugging output... maybe
 * switch to like that. */

#ifndef NE_DEBUGGING
#define NE_DEBUG if (0) ne_debug
#else /* DEBUGGING */
#define NE_DEBUG ne_debug
#endif /* DEBUGGING */

#define NE_DBG_SOCKET (1<<0)
#define NE_DBG_HTTP (1<<1)
#define NE_DBG_XML (1<<2)
#define NE_DBG_HTTPAUTH (1<<3)
#define NE_DBG_HTTPPLAIN (1<<4)
#define NE_DBG_LOCKS (1<<5)
#define NE_DBG_XMLPARSE (1<<6)
#define NE_DBG_HTTPBODY (1<<7)
#define NE_DBG_SSL (1<<8)
#define NE_DBG_FLUSH (1<<30)

/* Send debugging output to 'stream', for all of the given debug
 * channels.  To disable debugging, pass 'stream' as NULL and 'mask'
 * as 0. */
void ne_debug_init(FILE *stream, int mask);

/* The current debug mask and stream set by the last call to
 * ne_debug_init. */
extern int ne_debug_mask;
extern FILE *ne_debug_stream;

/* Produce debug output if any of channels 'ch' is enabled for
 * debugging. */
void ne_debug(int ch, const char *, ...) ne_attribute((format(printf, 2, 3)));

/* Storing an HTTP status result */
typedef struct {
    int major_version;
    int minor_version;
    int code; /* Status-Code value */
    int klass; /* Class of Status-Code (1-5) */
    char *reason_phrase;
} ne_status;

/* NB: couldn't use 'class' in ne_status because it would clash with
 * the C++ reserved word. */

/* Parser for strings which follow the Status-Line grammar from 
 * RFC2616.  s->reason_phrase is malloc-allocated if non-NULL, and
 * must be free'd by the caller.
 *  Returns:
 *    0 on success, *s will be filled in.
 *   -1 on parse error.
 */
int ne_parse_statusline(const char *status_line, ne_status *s);

END_NEON_DECLS

#endif /* NE_UTILS_H */
