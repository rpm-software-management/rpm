/* 
   String utility functions
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <ctype.h> /* for isprint() etc in ne_strclean() */

#include "ne_alloc.h"
#include "ne_string.h"

char *ne_token(char **str, char separator)
{
    char *ret = *str, *pnt = strchr(*str, separator);

    if (pnt) {
	*pnt = '\0';
	*str = pnt + 1;
    } else {
	/* no separator found: return end of string. */
	*str = NULL;
    }
    
    return ret;
}

char *ne_qtoken(char **str, char separator, const char *quotes)
{
    char *pnt, *ret = NULL;

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
	    return ret;
	}
    }

    /* no separator found: return end of string. */
    ret = *str;
    *str = NULL;
    return ret;
}

char *ne_shave(char *str, const char *whitespace)
{
    char *pnt, *ret = str;

    while (*ret != '\0' && strchr(whitespace, *ret) != NULL) {
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
    ne_free(comps);
    pairs[2*count] = pairs[2*count+1] = NULL;    
    return pairs;
}

void split_string_free(char **components) 
{
    char **pnt = components;
    while (*pnt != NULL) {
	ne_free(*pnt);
	pnt++;
    }
    ne_free(components);
}

void pair_string_free(char **pairs) 
{
    int n;
    for (n = 0; pairs[n] != NULL; n+=2) {
	ne_free(pairs[n]);
    }
    ne_free(pairs);
}

void ne_buffer_clear(ne_buffer *buf) 
{
    memset(buf->data, 0, buf->length);
    buf->used = 1;
}  

/* Grows for given size, returns 0 on success, -1 on error. */
void ne_buffer_grow(ne_buffer *buf, size_t newsize) 
{
#define NE_BUFFER_GROWTH 512
    if (newsize > buf->length) {
	/* If it's not big enough already... */
	buf->length = ((newsize / NE_BUFFER_GROWTH) + 1) * NE_BUFFER_GROWTH;
	
	/* Reallocate bigger buffer */
	buf->data = ne_realloc(buf->data, buf->length);
    }
}

static size_t count_concat(va_list *ap)
{
    size_t total = 0;
    char *next;

    while ((next = va_arg(*ap, char *)) != NULL)
	total += strlen(next);

    return total;
}

static void do_concat(char *str, va_list *ap) 
{
    char *next;

    while ((next = va_arg(*ap, char *)) != NULL) {
#ifdef HAVE_STPCPY
        str = stpcpy(str, next);
#else
	size_t len = strlen(next);
	memcpy(str, next, len);
	str += len;
#endif
    }
}

void ne_buffer_concat(ne_buffer *buf, ...)
{
    va_list ap;
    ssize_t total;

    va_start(ap, buf);
    total = buf->used + count_concat(&ap);
    va_end(ap);    

    /* Grow the buffer */
    ne_buffer_grow(buf, total);
    
    va_start(ap, buf);    
    do_concat(buf->data + buf->used - 1, &ap);
    va_end(ap);    

    buf->used = total;
    buf->data[total - 1] = '\0';
}

char *ne_concat(const char *str, ...)
{
    va_list ap;
    size_t total, slen = strlen(str);
    char *ret;

    va_start(ap, str);
    total = slen + count_concat(&ap);
    va_end(ap);

    ret = memcpy(ne_malloc(total + 1), str, slen);

    va_start(ap, str);
    do_concat(ret + slen, &ap);
    va_end(ap);

    ret[total] = '\0';
    return ret;    
}

/* Append zero-terminated string... returns 0 on success or -1 on
 * realloc failure. */
void ne_buffer_zappend(ne_buffer *buf, const char *str) 
{
    ne_buffer_append(buf, str, strlen(str));
}

void ne_buffer_append(ne_buffer *buf, const char *data, size_t len) 
{
    ne_buffer_grow(buf, buf->used + len);
    memcpy(buf->data + buf->used - 1, data, len);
    buf->used += len;
    buf->data[buf->used - 1] = '\0';
}

ne_buffer *ne_buffer_create(void) 
{
    return ne_buffer_ncreate(512);
}

ne_buffer *ne_buffer_ncreate(size_t s) 
{
    ne_buffer *buf = ne_malloc(sizeof(*buf));
    buf->data = ne_malloc(s);
    buf->data[0] = '\0';
    buf->length = s;
    buf->used = 1;
    return buf;
}

void ne_buffer_destroy(ne_buffer *buf) 
{
    ne_free(buf->data);
    ne_free(buf);
}

char *ne_buffer_finish(ne_buffer *buf)
{
    char *ret = buf->data;
    ne_free(buf);
    return ret;
}

void ne_buffer_altered(ne_buffer *buf)
{
    buf->used = strlen(buf->data) + 1;
}

static const char *b64_alphabet =  
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/=";
    
char *ne_base64(const unsigned char *text, size_t inlen)
{
    /* The tricky thing about this is doing the padding at the end,
     * doing the bit manipulation requires a bit of concentration only */
    char *buffer, *point;
    size_t outlen;
    
    /* Use 'buffer' to store the output. Work out how big it should be...
     * This must be a multiple of 4 bytes */

    outlen = (inlen*4)/3;
    if ((inlen % 3) > 0) /* got to pad */
	outlen += 4 - (inlen % 3);
    
    buffer = ne_malloc(outlen + 1); /* +1 for the \0 */
    
    /* now do the main stage of conversion, 3 bytes at a time,
     * leave the trailing bytes (if there are any) for later */

    for (point=buffer; inlen>=3; inlen-=3, text+=3) {
	*(point++) = b64_alphabet[ (*text)>>2 ]; 
	*(point++) = b64_alphabet[ ((*text)<<4 & 0x30) | (*(text+1))>>4 ]; 
	*(point++) = b64_alphabet[ ((*(text+1))<<2 & 0x3c) | (*(text+2))>>6 ];
	*(point++) = b64_alphabet[ (*(text+2)) & 0x3f ];
    }

    /* Now deal with the trailing bytes */
    if (inlen > 0) {
	/* We always have one trailing byte */
	*(point++) = b64_alphabet[ (*text)>>2 ];
	*(point++) = b64_alphabet[ (((*text)<<4 & 0x30) |
				     (inlen==2?(*(text+1))>>4:0)) ]; 
	*(point++) = (inlen==1?'=':b64_alphabet[ (*(text+1))<<2 & 0x3c ]);
	*(point++) = '=';
    }

    /* Null-terminate */
    *point = '\0';

    return buffer;
}

/* VALID_B64: fail if 'ch' is not a valid base64 character */
#define VALID_B64(ch) (((ch) >= 'A' && (ch) <= 'Z') || \
                       ((ch) >= 'a' && (ch) <= 'z') || \
                       ((ch) >= '0' && (ch) <= '9') || \
                       (ch) == '/' || (ch) == '+' || (ch) == '=')

/* DECODE_B64: decodes a valid base64 character. */
#define DECODE_B64(ch) ((ch) >= 'a' ? ((ch) + 26 - 'a') : \
                        ((ch) >= 'A' ? ((ch) - 'A') : \
                         ((ch) >= '0' ? ((ch) + 52 - '0') : \
                          ((ch) == '+' ? 62 : 63))))

size_t ne_unbase64(const char *data, unsigned char **out)
{
    size_t inlen = strlen(data);
    unsigned char *outp;
    const unsigned char *in;

    if (inlen == 0 || (inlen % 4) != 0) return 0;
    
    outp = *out = ne_malloc(inlen * 3 / 4);

    for (in = (const unsigned char *)data; *in; in += 4) {
        unsigned int tmp;
        if (!VALID_B64(in[0]) || !VALID_B64(in[1]) || !VALID_B64(in[2]) ||
            !VALID_B64(in[3]) || in[0] == '=' || in[1] == '=' ||
            (in[2] == '=' && in[3] != '=')) {
            ne_free(*out);
            return 0;
        }
        tmp = (DECODE_B64(in[0]) & 0x3f) << 18 |
            (DECODE_B64(in[1]) & 0x3f) << 12;
        *outp++ = (tmp >> 16) & 0xff;
        if (in[2] != '=') {
            tmp |= (DECODE_B64(in[2]) & 0x3f) << 6;
            *outp++ = (tmp >> 8) & 0xff;
            if (in[3] != '=') {
                tmp |= DECODE_B64(in[3]) & 0x3f;
                *outp++ = tmp & 0xff;
            }
        }
    }

    return outp - *out;
}

char *ne_strclean(char *str)
{
    char *pnt;
    for (pnt = str; *pnt; pnt++)
        if (iscntrl(*pnt) || !isprint(*pnt)) *pnt = ' ';
    return str;
}

char *ne_strerror(int errnum, char *buf, size_t buflen)
{
#ifdef HAVE_STRERROR_R
#ifdef STRERROR_R_CHAR_P
    /* glibc-style strerror_r which may-or-may-not use provided buffer. */
    char *ret = strerror_r(errnum, buf, buflen);
    if (ret != buf)
	ne_strnzcpy(buf, ret, buflen);
#else /* POSIX-style strerror_r: */
    strerror_r(errnum, buf, buflen);
#endif
#else /* no strerror_r: */
    ne_strnzcpy(buf, strerror(errnum), buflen);
#endif
    return buf;
}
