/* 
   HTTP utility functions
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

#ifndef NE_UTILS_H
#define NE_UTILS_H

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>

#include "ne_defs.h"

BEGIN_NEON_DECLS

/* Returns a human-readable version string like:
 * "neon 0.2.0: Library build, OpenSSL support"
 */
const char *ne_version_string(void);

/* Returns non-zero if the neon API compiled in is less than
 * major.minor. i.e.
 *   I am: 1.2 -  neon_version_check(1, 3) => -1
 *   I am: 0.10 -  neon_version_check(0, 9) => 0
 */
int ne_version_minimum(int major, int minor);

#define HTTP_QUOTES "\"'"
#define HTTP_WHITESPACE " \r\n\t"

#ifndef WIN32
#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifdef WIN32
/* some of the API uses ssize_t so we need to define it. */
#define ssize_t int
#endif

/* CONSIDER: mutt has a nicer way of way of doing debugging output... maybe
 * switch to like that. */

#ifdef __GNUC__
/* really, we want to do this if we have any C99-capable compiler, so
 * what's a better test? */

#ifndef NE_DEBUGGING
#define NE_DEBUG(x, fmt, args...)
#else
#define NE_DEBUG(x, fmt, args...) do {		\
 if (((x)&ne_debug_mask) == (x)) {			\
  fflush(stdout);				\
  fprintf(ne_debug_stream, fmt, ##args);	\
  if (((x) & NE_DBG_FLUSH) == NE_DBG_FLUSH)	\
   fflush(ne_debug_stream);			\
 }						\
} while (0)
#endif   

#else /* !__GNUC__ */

#ifndef NE_DEBUGGING
#define NE_DEBUG if (0) ne_debug
#else /* DEBUGGING */
#define NE_DEBUG ne_debug
#endif /* DEBUGGING */

#endif

#define NE_DBG_SOCKET (1<<0)
#define NE_DBG_HTTP (1<<1)
#define NE_DBG_XML (1<<2)
#define NE_DBG_HTTPAUTH (1<<3)
#define NE_DBG_HTTPPLAIN (1<<4)
#define NE_DBG_LOCKS (1<<5)
#define NE_DBG_XMLPARSE (1<<6)
#define NE_DBG_HTTPBODY (1<<7)
#define NE_DBG_HTTPBASIC (1<<8)
#define NE_DBG_FLUSH (1<<30)

void ne_debug_init(FILE *stream, int mask);
extern int ne_debug_mask;
extern FILE *ne_debug_stream;

void ne_debug(int ch, const char *, ...)
#ifdef __GNUC__
                __attribute__ ((format (printf, 2, 3)))
#endif /* __GNUC__ */
;

/* Storing an HTTP status result */
typedef struct {
    int major_version;
    int minor_version;
    int code; /* Status-Code value */
    /* We can't use 'class' as the member name since this crashes
     * with the C++ reserved keyword 'class', annoyingly.
     * This was '_class' previously, but that was even MORE annoying.
     * So know it is klass. */
    int klass; /* Class of Status-Code (1-5) */
    const char *reason_phrase;
} ne_status;

/* Parser for strings which follow the Status-Line grammar from 
 * RFC2616.
 *  Returns:
 *    0 on success, *s will be filled in.
 *   -1 on parse error.
 */
int ne_parse_statusline(const char *status_line, ne_status *s);

END_NEON_DECLS

#endif /* NE_UTILS_H */
