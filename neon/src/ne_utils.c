/* 
   HTTP utility functions
   Copyright (C) 1999-2004, Joe Orton <joe@manyfish.co.uk>

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

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>
#include <ctype.h> /* isdigit() for ne_parse_statusline */

#ifdef NEON_SSL
#include <openssl/opensslv.h>
#endif

/* libxml2: pick up the version string. */
#if defined(HAVE_LIBXML)
#include <libxml/xmlversion.h>
#elif defined(HAVE_EXPAT) && !defined(HAVE_XMLPARSE_H)
#include <expat.h>
#endif

#ifdef NEON_ZLIB
#include <zlib.h>
#endif

#include "ne_utils.h"
#include "ne_string.h" /* for ne_strdup */
#include "ne_dates.h"

int ne_debug_mask = 0;
FILE *ne_debug_stream = NULL;

void ne_debug_init(FILE *stream, int mask)
{
    ne_debug_stream = stream;
    ne_debug_mask = mask;
#if defined(HAVE_SETVBUF) && defined(_IONBF)
    /* If possible, turn off buffering on the debug log.  this is very
     * helpful if debugging segfaults. */
    if (stream) setvbuf(stream, NULL, _IONBF, 0);
#endif        
}

void ne_debug(int ch, const char *template, ...) 
{
    va_list params;
    if ((ch & ne_debug_mask) == 0) return;
    fflush(stdout);
    va_start(params, template);
    vfprintf(ne_debug_stream, template, params);
    va_end(params);
    if ((ch & NE_DBG_FLUSH) == NE_DBG_FLUSH)
	fflush(ne_debug_stream);
}

#define NE_STRINGIFY(x) # x
#define NE_EXPAT_VER(x,y,z) NE_STRINGIFY(x) "." NE_STRINGIFY(y) "." NE_STRINGIFY(z)

static const char *version_string = "neon " NEON_VERSION ": " 
#ifdef NEON_IS_LIBRARY
  "Library build"
#else
  "Bundled build"
#endif
#ifdef HAVE_EXPAT
  ", Expat"
/* expat >=1.95.2 exported the version */
#ifdef XML_MAJOR_VERSION
" " NE_EXPAT_VER(XML_MAJOR_VERSION, XML_MINOR_VERSION, XML_MICRO_VERSION)
#endif
#else /* !HAVE_EXPAT */
#ifdef HAVE_LIBXML
  ", libxml " LIBXML_DOTTED_VERSION
#endif /* HAVE_LIBXML */
#endif /* !HAVE_EXPAT */
#if defined(NEON_ZLIB) && defined(ZLIB_VERSION)
  ", zlib " ZLIB_VERSION
#endif /* NEON_ZLIB && ... */
#ifdef NEON_SOCKS
   ", SOCKSv5"
#endif
#ifdef NEON_SSL
#ifdef OPENSSL_VERSION_TEXT
    ", " OPENSSL_VERSION_TEXT
#else
   "OpenSSL (unknown version)"
#endif /* OPENSSL_VERSION_TEXT */
#endif
   "."
;

const char *ne_version_string(void)
{
    return version_string;
}

int ne_version_match(int major, int minor)
{
    return (NEON_VERSION_MAJOR != major) || (NEON_VERSION_MINOR < minor);
}

int ne_supports_ssl(void)
{
#ifdef NEON_SSL
    return 1;
#else
    return 0;
#endif
}

int ne_parse_statusline(const char *status_line, ne_status *st)
{
    const char *part;
    int major, minor, status_code, klass;

    /* skip leading garbage if any. */
    part = strstr(status_line, "HTTP/");
    if (part == NULL) return -1;

    minor = major = 0;

    /* Parse version string, skipping leading zeroes. */
    for (part += 5; *part != '\0' && isdigit(*part); part++)
	major = major*10 + (*part-'0');

    if (*part++ != '.') return -1;

    for (;*part != '\0' && isdigit(*part); part++)
	minor = minor*10 + (*part-'0');

    if (*part != ' ') return -1;

    /* Skip any spaces */
    for (; *part == ' '; part++) /* noop */;

    /* Parse the Status-Code; part now points at the first Y in
     * "HTTP/x.x YYY". */
    if (!isdigit(part[0]) || !isdigit(part[1]) || !isdigit(part[2]) ||
	(part[3] != '\0' && part[3] != ' ')) return -1;
    status_code = 100*(part[0]-'0') + 10*(part[1]-'0') + (part[2]-'0');
    klass = part[0]-'0';

    /* Skip whitespace between status-code and reason-phrase */
    for (part+=3; *part == ' ' || *part == '\t'; part++) /* noop */;

    /* part now may be pointing to \0 if reason phrase is blank */

    /* Fill in the results */
    st->major_version = major;
    st->minor_version = minor;
    st->reason_phrase = ne_strclean(ne_strdup(part));
    st->code = status_code;
    st->klass = klass;
    return 0;
}
