/* 
   HTTP URI handling
   Copyright (C) 1999-2003, Joe Orton <joe@manyfish.co.uk>

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

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdio.h>

#include <ctype.h>

#include "ne_string.h" /* for ne_buffer */
#include "ne_alloc.h"
#include "ne_uri.h"

char *ne_path_parent(const char *uri) 
{
    size_t len = strlen(uri);
    const char *pnt = uri + len - 1;
    /* skip trailing slash (parent of "/foo/" is "/") */
    if (pnt >= uri && *pnt == '/')
	pnt--;
    /* find previous slash */
    while (pnt > uri && *pnt != '/')
	pnt--;
    if (pnt < uri || (pnt == uri && *pnt != '/'))
	return NULL;
    return ne_strndup(uri, pnt - uri + 1);
}

int ne_path_has_trailing_slash(const char *uri) 
{
    size_t len = strlen(uri);
    return ((len > 0) &&
	    (uri[len-1] == '/'));
}

unsigned int ne_uri_defaultport(const char *scheme)
{
    /* RFC2616/3.2.3 says use case-insensitive comparisons here. */
    if (strcasecmp(scheme, "http") == 0)
	return 80;
    else if (strcasecmp(scheme, "https") == 0)
	return 443;
    else
	return 0;
}

/* TODO: Also, maybe stop malloc'ing here, take a "char *" uri, modify
 * it in-place, and have fields point inside passed uri.  More work
 * for the caller then though. */
/* TODO: not a proper URI parser */
int ne_uri_parse(const char *uri, ne_uri *parsed)
{
    const char *pnt, *slash, *colon, *atsign, *openbk;

    parsed->port = 0;
    parsed->host = NULL;
    parsed->path = NULL;
    parsed->scheme = NULL;
    parsed->authinfo = NULL;

    if (uri[0] == '\0') {
	return -1;
    }

    pnt = strstr(uri, "://");
    if (pnt) {
	parsed->scheme = ne_strndup(uri, pnt - uri);
	pnt += 3; /* start of hostport segment */
    } else {
	pnt = uri;
    }
    
    atsign = strchr(pnt, '@');
    slash = strchr(pnt, '/');
    openbk = strchr(pnt, '[');

    /* Check for an authinfo segment in the hostport segment. */
    if (atsign != NULL && (slash == NULL || atsign < slash)) {
	parsed->authinfo = ne_strndup(pnt, atsign - pnt);
	pnt = atsign + 1;
    }
    
    if (openbk && (!slash || openbk < slash)) {
	const char *closebk = strchr(openbk, ']');
	if (closebk == NULL)
	    return -1;
	colon = strchr(closebk + 1, ':');
    } else {
	colon = strchr(pnt, ':');
    }

    if (slash == NULL) {
	parsed->path = ne_strdup("/");
	if (colon == NULL) {
	    parsed->host = ne_strdup(pnt);
	} else {
	    parsed->port = atoi(colon+1);
	    parsed->host = ne_strndup(pnt, colon - pnt);
	}
    } else {
	if (colon == NULL || colon > slash) {
	    /* No port segment */
	    if (slash != uri) {
		parsed->host = ne_strndup(pnt, slash - pnt);
	    } else {
		/* No hostname segment. */
	    }
	} else {
	    /* Port segment */
	    parsed->port = atoi(colon + 1);
	    parsed->host = ne_strndup(pnt, colon - pnt);
	}
	parsed->path = ne_strdup(slash);
    }

    return 0;
}

void ne_uri_free(ne_uri *u)
{
    if (u->host) ne_free(u->host);
    if (u->path) ne_free(u->path);
    if (u->scheme) ne_free(u->scheme);
    if (u->authinfo) ne_free(u->authinfo);
    memset(u, 0, sizeof *u);
}

char *ne_path_unescape(const char *uri) 
{
    const char *pnt;
    char *ret, *retpos, buf[5] = { "0x00\0" };
    retpos = ret = ne_malloc(strlen(uri) + 1);
    for (pnt = uri; *pnt != '\0'; pnt++) {
	if (*pnt == '%') {
	    if (!isxdigit((unsigned char) pnt[1]) || 
		!isxdigit((unsigned char) pnt[2])) {
		/* Invalid URI */
                ne_free(ret);
		return NULL;
	    }
	    buf[2] = *++pnt; buf[3] = *++pnt; /* bit faster than memcpy */
	    *retpos++ = (char)strtol(buf, NULL, 16);
	} else {
	    *retpos++ = *pnt;
	}
    }
    *retpos = '\0';
    return ret;
}

/* RFC2396 spake:
 * "Data must be escaped if it does not have a representation 
 * using an unreserved character".
 */

/* Lookup table: character classes from 2396. (This is overkill) */

#define SP 0   /* space    = <US-ASCII coded character 20 hexadecimal>                 */
#define CO 0   /* control  = <US-ASCII coded characters 00-1F and 7F hexadecimal>      */
#define DE 0   /* delims   = "<" | ">" | "#" | "%" | <">                               */
#define UW 0   /* unwise   = "{" | "}" | "|" | "\" | "^" | "[" | "]" | "`"             */
#define MA 1   /* mark     = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"       */
#define AN 2   /* alphanum = alpha | digit                                             */
#define RE 2   /* reserved = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | "," */

/*@unchecked@*/
static const char uri_chars[128] = {
/*                +2      +4      +6      +8     +10     +12     +14     */
/*   0 */ CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO,
/*  16 */ CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO, CO,
/*  32 */ SP, MA, DE, DE, RE, DE, RE, MA, MA, MA, MA, RE, RE, MA, MA, RE,
/*  48 */ AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, RE, RE, DE, RE, DE, RE,
/*  64 */ RE, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN,
/*  80 */ AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, UW, UW, UW, UW, MA,
/*  96 */ UW, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN,
/* 112 */ AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, AN, UW, UW, UW, MA, CO 
};

#define ESCAPE(ch) (((const signed char)(ch) < 0 || \
		uri_chars[(unsigned int)(ch)] == 0))

#undef SP
#undef CO
#undef DE
#undef UW
#undef MA
#undef AN
#undef RE

char *ne_path_escape(const char *abs_path) 
{
    const char *pnt;
    char *ret, *retpos;
    int count = 0;
    for (pnt = abs_path; *pnt != '\0'; pnt++) {
	if (ESCAPE(*pnt)) {
	    count++;
	}
    }
    if (count == 0) {
	return ne_strdup(abs_path);
    }
    /* An escaped character is "%xx", i.e., two MORE
     * characters than the original string */
    retpos = ret = ne_malloc(strlen(abs_path) + 2*count + 1);
    for (pnt = abs_path; *pnt != '\0'; pnt++) {
	if (ESCAPE(*pnt)) {
	    /* Escape it - %<hex><hex> */
	    sprintf(retpos, "%%%02x", (unsigned char) *pnt);
	    retpos += 3;
	} else {
	    /* It's cool */
	    *retpos++ = *pnt;
	}
    }
    *retpos = '\0';
    return ret;
}

#undef ESCAPE

#define CASECMP(field) do { \
n = strcasecmp(u1->field, u2->field); if (n) return n; } while(0)

#define CMP(field) do { \
n = strcmp(u1->field, u2->field); if (n) return n; } while(0)

/* As specified by RFC 2616, section 3.2.3. */
int ne_uri_cmp(const ne_uri *u1, const ne_uri *u2)
{
    int n;
    
    if (u1->path[0] == '\0' && strcmp(u2->path, "/") == 0)
	return 0;
    if (u2->path[0] == '\0' && strcmp(u1->path, "/") == 0)
	return 0;

    CMP(path);
    CASECMP(host);
    CASECMP(scheme);
    if (u1->port > u2->port)
	return 1;
    else if (u1->port < u2->port)
	return -1;
    return 0;
}

#undef CMP
#undef CASECMP

#ifndef WIN32
#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

/* TODO: implement properly */
int ne_path_compare(const char *a, const char *b) 
{
    int ret = strcasecmp(a, b);
    if (ret) {
	/* This logic says: "If the lengths of the two URIs differ by
	 * exactly one, and the LONGER of the two URIs has a trailing
	 * slash and the SHORTER one DOESN'T, then..." */
	int traila = ne_path_has_trailing_slash(a),
	    trailb = ne_path_has_trailing_slash(b),
	    lena = strlen(a), lenb = strlen(b);
	if (traila != trailb && abs(lena - lenb) == 1 &&
	    ((traila && lena > lenb) || (trailb && lenb > lena))) {
	    /* Compare them, ignoring the trailing slash on the longer
	     * URI */
	    if (strncasecmp(a, b, min(lena, lenb)) == 0)
		ret = 0;
	}
    }
    return ret;
}

char *ne_uri_unparse(const ne_uri *uri)
{
    ne_buffer *buf = ne_buffer_create();

    ne_buffer_concat(buf, uri->scheme, "://", uri->host, NULL);

    if (uri->port > 0 && ne_uri_defaultport(uri->scheme) != uri->port) {
	char str[20];
	ne_snprintf(str, 20, ":%d", uri->port);
	ne_buffer_zappend(buf, str);
    }

    ne_buffer_zappend(buf, uri->path);

    return ne_buffer_finish(buf);
}

/* Give it a path segment, it returns non-zero if child is 
 * a child of parent. */
int ne_path_childof(const char *parent, const char *child) 
{
    char *root = ne_strdup(child);
    int ret;
    if (strlen(parent) >= strlen(child)) {
	ret = 0;
    } else {
	/* root is the first of child, equal to length of parent */
	root[strlen(parent)] = '\0';
	ret = (ne_path_compare(parent, root) == 0);
    }
    ne_free(root);
    return ret;
}
