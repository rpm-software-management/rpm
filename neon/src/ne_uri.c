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

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <ctype.h>

#include "ne_utils.h" /* for 'min' */
#include "ne_string.h" /* for CONCAT3 */
#include "ne_alloc.h"
#include "ne_uri.h"

char *uri_parent(const char *uri) 
{
    const char *pnt;
    char *ret;
    pnt = uri+strlen(uri)-1;
    while (*(--pnt) != '/' && pnt >= uri) /* noop */;
    if (pnt < uri) {
	/* not a valid absPath */
	return NULL;
    }
    /*  uri    
     *   V
     *   |---|
     *   /foo/bar/
     */
    ret = ne_malloc((pnt - uri) + 2);
    memcpy(ret, uri, (pnt - uri) + 1);
    ret[1+(pnt-uri)] = '\0';
    pnt++;
    return ret;
}

int uri_has_trailing_slash(const char *uri) 
{
    size_t len = strlen(uri);
    return ((len > 0) &&
	    (uri[len-1] == '/'));
}

const char *uri_abspath(const char *uri) 
{
    const char *ret;
    /* Look for the scheme: */
    ret = strstr(uri, "://");
    if (ret == NULL) {
	/* No scheme */
	ret = uri;
    } else {
	/* Look for the abs_path */
	ret = strchr(ret+3, '/');
	if (ret == NULL) {
	    /* Uh-oh */
	    ret = uri;
	}
    }
    return ret;
}

/* TODO: not a proper URI parser */
int uri_parse(const char *uri, struct uri *parsed, 
	      const struct uri *defaults)
{
    const char *pnt, *slash, *colon, *atsign;

    parsed->port = -1;
    parsed->host = NULL;
    parsed->path = NULL;
    parsed->scheme = NULL;
    parsed->authinfo = NULL;

    if (strlen(uri) == 0) {
	return -1;
    }

    pnt = strstr(uri, "://");
    if (pnt) {
	parsed->scheme = ne_strndup(uri, pnt - uri);
	pnt += 3; /* start of hostport segment */
    } else {
	pnt = uri;
	if (defaults && defaults->scheme != NULL) {
	    parsed->scheme = ne_strdup(defaults->scheme);
	}
    }
    
    atsign = strchr(pnt, '@');
    slash = strchr(pnt, '/');

    /* Check for an authinfo segment in the hostport segment. */
    if (atsign != NULL && (slash == NULL || atsign < slash)) {
	parsed->authinfo = ne_strndup(pnt, atsign - pnt);
	pnt = atsign + 1;
    }

    colon = strchr(pnt, ':');

    if (slash == NULL) {
	parsed->path = ne_strdup("/");
	if (colon == NULL) {
	    if (defaults) parsed->port = defaults->port;
	    parsed->host = ne_strdup(pnt);
	} else {
	    parsed->port = atoi(colon+1);
	    parsed->host = ne_strndup(pnt, colon - pnt);
	}
    } else {
	if (colon == NULL || colon > slash) {
	    /* No port segment */
	    if (defaults) parsed->port = defaults->port;
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

void uri_free(struct uri *uri)
{
    NE_FREE(uri->host);
    NE_FREE(uri->path);
    NE_FREE(uri->scheme);
    NE_FREE(uri->authinfo);
}

/* Returns an absoluteURI */
char *uri_absolute(const char *uri, const char *scheme, 
		   const char *hostport) 
{
    char *ret;
    /* Is it absolute already? */
    if (strncmp(uri, scheme, strlen(scheme)) == 0)  {
	/* Yes it is */
	ret = ne_strdup(uri);
    } else {
	/* Oh no it isn't */
	CONCAT3(ret, scheme, hostport, uri);
    }
    return ret;
}

/* Un-escapes a URI. Returns ne_malloc-allocated URI */
char *uri_unescape(const char *uri) 
{
    const char *pnt;
    char *ret, *retpos, buf[5] = { "0x00\0" };
    retpos = ret = ne_malloc(strlen(uri) + 1);
    for (pnt = uri; *pnt != '\0'; pnt++) {
	if (*pnt == '%') {
	    if (!isxdigit((unsigned char) pnt[1]) || 
		!isxdigit((unsigned char) pnt[2])) {
		/* Invalid URI */
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

/* Escapes the abspath segment of a URI.
 * Returns ne_malloc-allocated string.
 */
char *uri_abspath_escape(const char *abs_path) 
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

/* TODO: implement properly */
int uri_compare(const char *a, const char *b) 
{
    int ret = strcasecmp(a, b);
    if (ret) {
	/* This logic says: "If the lengths of the two URIs differ by
	 * exactly one, and the LONGER of the two URIs has a trailing
	 * slash and the SHORTER one DOESN'T, then..." */
	int traila = uri_has_trailing_slash(a),
	    trailb = uri_has_trailing_slash(b),
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

/* Give it a path segment, it returns non-zero if child is 
 * a child of parent. */
int uri_childof(const char *parent, const char *child) 
{
    char *root = ne_strdup(child);
    int ret;
    if (strlen(parent) >= strlen(child)) {
	ret = 0;
    } else {
	/* root is the first of child, equal to length of parent */
	root[strlen(parent)] = '\0';
	ret = (uri_compare(parent, root) == 0);
    }
    free(root);
    return ret;
}
