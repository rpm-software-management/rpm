/* 
   String utility functions
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ne_alloc.h"
#include "ne_string.h"

char *ne_token(char **str, char separator, const char *quotes)
{
    char *pnt, *ret = NULL;

    if (quotes) {

	for (pnt = *str; *pnt != '\0'; pnt++) {
	    char *quot = strchr(quotes, *pnt);
	    
	    if (quot) {
		char *qclose = strchr(pnt+1, *quot);
		
		if (!qclose) {
		    /* no closing quote: invalid string. */
		    return NULL;
		}
		
		pnt = qclose;
	    } else if (*pnt == separator) {
		/* found end of token. */
		*pnt = '\0';
		ret = *str;
		*str = pnt + 1;
		break;
	    }
	}

    } else {
	pnt = strchr(*str, separator);
	if (pnt != NULL) {
	    *pnt = '\0';
	    ret = *str;
	    *str = pnt + 1;
	}
    }

    if (ret == NULL) {
	/* no separator found: return end of string. */
	ret = *str;
	*str = NULL;
    }

    return ret;
}

char *ne_shave(char *str, const char *whitespace)
{
    char *pnt, *ret = str;

    while (strchr(whitespace, *ret) != NULL) {
	ret++;
    }

    /* pnt points at the NUL terminator. */
    pnt = &ret[strlen(ret)];
    
    while (pnt > ret && strchr(whitespace, *(pnt-1)) != NULL) {
	pnt--;
    }

    *pnt = '\0';
    return ret;
}

/* TODO: deprecate all these and use ne_token() instead. */

char **split_string(const char *str, const char separator,
		     const char *quotes, const char *whitespace) 
{
    return split_string_c(str, separator, quotes, whitespace, NULL);
}

char **split_string_c(const char *str, const char separator,
		      const char *quotes, const char *whitespace,
		      int *give_count) 
{
    char **comps;
    const char *pnt, *quot = NULL,
	*start, *end; /* The start of the current component */
    int count, /* The number of components */
	iswhite, /* is it whitespace */
	issep, /* is it the separator */
	curr, /* current component index */
	length, /* length of component */
	leading_wspace; /* in leading whitespace still? */

    /* Inefficient, but easier - first off, count the number of 
     * components we have. */
    count = 1;
    for (pnt = str; *pnt!='\0'; pnt++) {
	if (quotes != NULL) {
	    quot = strchr(quotes, *pnt);
	}
	if (quot != NULL) {
	    /* We found a quote, so skip till the next quote */
	    for (pnt++; (*pnt!=*quot) && (*pnt!='\0'); pnt++)
		/* nullop */;
	} else if (*pnt == separator) {
	    count++;
	}
    }

    if (give_count) {
	/* Write the count */
	*give_count = count;
    }

    /* Now, have got the number of components.
     * Allocate the comps array. +1 for the NULL */
    comps = ne_malloc(sizeof(char *) * (count + 1));

    comps[count] = NULL;
    
    quot = end = start = NULL;
    curr = 0;
    leading_wspace = 1;

    /* Now fill in the array */
    for (pnt = str; *pnt != '\0'; pnt++) {
	/* What is the current character - quote, whitespace, separator? */
	if (quotes != NULL) {
	    quot = strchr(quotes, *pnt);
	}
	iswhite = (whitespace!=NULL) && 
	    (strchr(whitespace, *pnt) != NULL);
	issep = (*pnt == separator);
	/* What to do? */
	if (leading_wspace) {
	    if (quot!=NULL) {
		/* Quoted bit */
		start = pnt;
		length = 1;
		leading_wspace = 0;
	    } else if (issep) {
		/* Zero-length component */
		comps[curr++] = ne_strdup("");
	    } else if (!iswhite) {
		start = end = pnt;
		length = 1;
		leading_wspace = 0;
	    }
	} else {
	    if (quot!=NULL) {
		/* Quoted bit */
		length++;
	    } else if (issep) {
		/* End of component - enter it into the array */
		length = (end - start) + 1;
		comps[curr] = ne_malloc(length+1);
		memcpy(comps[curr], start, length);
		comps[curr][length] = '\0';
		curr++;
		leading_wspace = 1;
	    } else if (!iswhite) {
		/* Not whitespace - update end marker */
		end = pnt;
	    }
	}
	if (quot != NULL) {
	    /* Skip to closing quote */
	    for (pnt++; *pnt!=*quot && *pnt != '\0'; ++pnt)
		/* nullop */;
	    /* Last non-wspace char is closing quote */
	    end = pnt;
	}
    }
    /* Handle final component */
    if (leading_wspace) {
	comps[curr] = ne_strdup("");
    } else {
	/* End of component - enter it into the array */
	length = (end - start) + 1;
	comps[curr] = ne_malloc(length+1);
	memcpy(comps[curr], start, length);
	comps[curr][length] = '\0';
    }
    return comps;
}

char **pair_string(const char *str, const char compsep, const char kvsep, 
		 const char *quotes, const char *whitespace) 
{
    char **comps, **pairs, *split;
    int count = 0, n, length;
    comps = split_string_c(str, compsep, quotes, whitespace, &count);
    /* Allocate space for 2* as many components as split_string returned,
     * +2 for the NULLS. */
    pairs = ne_malloc((2*count+2) * sizeof(char *));
    if (pairs == NULL) {
	return NULL;
    }
    for (n = 0; n < count; n++) {
	/* Find the split */
	split = strchr(comps[n], kvsep);
	if (split == NULL) {
	    /* No seperator found */
	    length = strlen(comps[n]);
	} else {
	    length = split-comps[n];
	}
	/* Enter the key into the array */
	pairs[2*n] = comps[n];
	/* Null-terminate the key */
	pairs[2*n][length] = '\0';
	pairs[2*n+1] = split?(split + 1):NULL;
    }
    free(comps);
    pairs[2*count] = pairs[2*count+1] = NULL;    
    return pairs;
}

void split_string_free(char **components) 
{
    char **pnt = components;
    while (*pnt != NULL) {
	free(*pnt);
	pnt++;
    }
    free(components);
}

void pair_string_free(char **pairs) 
{
    int n;
    for (n = 0; pairs[n] != NULL; n+=2) {
	free(pairs[n]);
    }
    free(pairs);
}

char *shave_string(const char *str, const char ch) 
{
    size_t len = strlen(str);
    char *ret;
    if (str[len-1] == ch) {
	len--;
    }
    if (str[0] == ch) {
	len--;
	str++;
    }
    ret = ne_malloc(len + 1);
    memcpy(ret, str, len);
    ret[len] = '\0';
    return ret;
}

char *ne_concat(const char *str, ...)
{
    va_list ap;
    ne_buffer *tmp = ne_buffer_create();

    ne_buffer_zappend(tmp, str);

    va_start(ap, str);
    ne_buffer_concat(tmp, ap);
    va_end(ap);
    
    return ne_buffer_finish(tmp);
}

void ne_buffer_clear(ne_buffer *buf) 
{
    memset(buf->data, 0, buf->length);
    buf->used = 1;
}  

/* Grows for given size, returns 0 on success, -1 on error. */
int ne_buffer_grow(ne_buffer *buf, size_t newsize) 
{
    size_t newlen, oldbuflen;

#define NE_BUFFER_GROWTH 512

    if (newsize <= buf->length) return 0; /* big enough already */
    /* FIXME: ah, can't remember my maths... better way to do this? */
    newlen = ((newsize / NE_BUFFER_GROWTH) + 1) * NE_BUFFER_GROWTH;
    
    oldbuflen = buf->length;
    /* Reallocate bigger buffer */
    buf->data = realloc(buf->data, newlen);
    if (buf->data == NULL) return -1;
    buf->length = newlen;
    /* Zero-out the new bit of buffer */
    memset(buf->data+oldbuflen, 0, newlen-oldbuflen);

    return 0;
}

int ne_buffer_concat(ne_buffer *buf, ...) 
{
    va_list ap;
    char *next;
    size_t totallen = buf->used; 

    /* Find out how much space we need for all the args */
    va_start(ap, buf);
    do {
	next = va_arg(ap, char *);
	if (next != NULL) {
	    totallen += strlen(next);
	}
    } while (next != NULL);
    va_end(ap);
    
    /* Grow the buffer */
    if (ne_buffer_grow(buf, totallen))
	return -1;
    
    /* Now append the arguments to the buffer */
    va_start(ap, buf);
    do {
	next = va_arg(ap, char *);
	if (next != NULL) {
	    /* TODO: use stpcpy */
	    strcat(buf->data, next);
	}
    } while (next != NULL);
    va_end(ap);
    
    buf->used = totallen;
    return 0;
}

/* Append zero-terminated string... returns 0 on success or -1 on
 * realloc failure. */
int ne_buffer_zappend(ne_buffer *buf, const char *str) 
{
    size_t len = strlen(str);

    if (ne_buffer_grow(buf, buf->used + len)) {
	return -1;
    }
    strcat(buf->data, str);
    buf->used += len;
    return 0;
}

int ne_buffer_append(ne_buffer *buf, const char *data, size_t len) 
{
    if (ne_buffer_grow(buf, buf->used + len)) {
	return -1;
    }
    memcpy(buf->data + buf->used - 1, data, len);
    buf->used += len;
    buf->data[buf->used - 1] = '\0';
    return 0;
}

ne_buffer *ne_buffer_create(void) 
{
    return ne_buffer_create_sized(512);
}

ne_buffer *ne_buffer_create_sized(size_t s) 
{
    ne_buffer *buf = ne_malloc(sizeof(struct ne_buffer_s));
    buf->data = ne_calloc(s);
    buf->length = s;
    buf->used = 1;
    return buf;
}

void ne_buffer_destroy(ne_buffer *buf) 
{
    if (buf->data) {
	free(buf->data);
    }
    free(buf);
}

char *ne_buffer_finish(ne_buffer *buf)
{
    char *ret = buf->data;
    free(buf);
    return ret;
}

void ne_buffer_altered(ne_buffer *buf)
{
    buf->used = strlen(buf->data) + 1;
}

char *ne_utf8_encode(const char *str)
{
    char *buffer = ne_malloc(strlen(str) * 2 + 1);
    int in, len = strlen(str);
    char *out;

    for (in = 0, out = buffer; in < len; in++, out++) {
	if ((unsigned char)str[in] <= 0x7D) {
	    *out = str[in];
	} else {
	    *out++ = 0xC0 | ((str[in] & 0xFC) >> 6);
	    *out = str[in] & 0xBF;
	}
    }

    /* nul-terminate */
    *out = '\0';
    return buffer;
}

/* Single byte range 0x00 -> 0x7F */
#define SINGLEBYTE_UTF8(ch) (((unsigned char) (ch)) < 0x80)

char *ne_utf8_decode(const char *str)
{
    int n, m, len = strlen(str);
    char *dest = ne_malloc(len + 1);
    
    for (n = 0, m = 0; n < len; n++, m++) {
	if (SINGLEBYTE_UTF8(str[n])) {
	    dest[m] = str[n];
	} else {
	    /* We only deal with 8-bit data, which will be encoded as
	     * two bytes of UTF-8 */
	    if ((len - n < 2) || (str[n] & 0xFC) != 0xC0) {
		free(dest);
		return NULL;
	    } else {
		/* see hip_xml.c for comments. */
		dest[m] = ((str[n] & 0x03) << 6) | (str[n+1] & 0x3F);
		n++;
	    }
	}
    }
    
    dest[m] = '\0';
    return dest;
}
