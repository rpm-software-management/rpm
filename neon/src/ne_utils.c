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

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <ctype.h> /* isdigit() for ne_parse_statusline */

#include "ne_utils.h"

#include "ne_dates.h"

#ifdef ENABLE_SSL
#include <openssl/opensslv.h>
#endif

#ifdef HAVE_XMLVERSION_H
/* libxml2: pick up the version string. */
#include <xmlversion.h>
#endif

int ne_debug_mask = 0;
FILE *ne_debug_stream = NULL;

void ne_debug_init(FILE *stream, int mask)
{
    ne_debug_stream = stream;
    ne_debug_mask = mask;
}

void ne_debug(int ch, const char *template, ...) 
{
    va_list params;
    if ((ch&ne_debug_mask) != ch) return;
    fflush(stdout);
    va_start(params, template);
    vfprintf(ne_debug_stream, template, params);
    va_end(params);
    if ((ch&NE_DBG_FLUSH) == NE_DBG_FLUSH) {
	fflush(ne_debug_stream);
    }
}

static const char *version_string = "neon " NEON_VERSION ": " 
#ifdef NEON_IS_LIBRARY
  "Library build"
#else
  "Bundled build"
#endif
#ifdef HAVE_EXPAT
  ", Expat"
#else
#ifdef HAVE_LIBXML
  ", libxml"
#ifdef LIBXML_DOTTED_VERSION
  " " LIBXML_DOTTED_VERSION
#endif
#endif
#endif
#ifdef ENABLE_SSL
   ", SSL support using "
#ifdef OPENSSL_VERSION_TEXT
    OPENSSL_VERSION_TEXT
#else
   "unknown SSL library"
#endif /* OPENSSL_VERSION_TEXT */
#else /* !ENABLE_SSL */
   ", no OpenSSL support"
#endif /* ENABLE_SSL */
   "."
;

const char *ne_version_string(void)
{
    return version_string;
}

int ne_version_minimum(int major, int minor)
{
    return 
	(NEON_VERSION_MAJOR < major) || 
	(NEON_VERSION_MINOR < minor);
}

int ne_parse_statusline(const char *status_line, ne_status *st)
{
    const char *part;
    int major, minor, status_code, klass;

    /* Check they're speaking the right language */
    status_line = strstr(status_line, "HTTP/");
    if (status_line == NULL) {
	return -1;
    } 

    /* And find out which dialect of this peculiar language
     * they can talk... */
    major = 0;
    minor = 0; 
    /* Note, we're good children, and accept leading zero's on the
     * version numbers */
    for (part = status_line + 5; *part != '\0' && isdigit(*part); part++) {
	major = major*10 + (*part-'0');
    }
    if (*part != '.') { 
	return -1;
    }
    for (part++ ; *part != '\0' && isdigit(*part); part++) {
	minor = minor*10 + (*part-'0');
    }
    if (*part != ' ') {
	return -1;
    }
    /* Skip any spaces */
    for (; *part == ' ' ; part++) /* noop */;
    /* Now for the Status-Code. part now points at the first Y in
     * "HTTP/x.x YYY". We want value of YYY... could use atoi, but
     * probably quicker this way. */
    if (!isdigit(part[0]) || !isdigit(part[1]) || !isdigit(part[2]) ||
	part[3] != ' ') {
	return -1;
    }
    status_code = 100*(part[0]-'0') + 10*(part[1]-'0') + (part[2]-'0');
    klass = part[0]-'0';
    /* Skip whitespace between status-code and reason-phrase */
    for (part+=3; *part == ' ' || *part == '\t'; part++) /* noop */;
    if (*part == '\0') {
	return -1;
    }
    /* Fill in the results */
    st->major_version = major;
    st->minor_version = minor;
    st->reason_phrase = part;
    st->code = status_code;
    st->klass = klass;
    return 0;
}
