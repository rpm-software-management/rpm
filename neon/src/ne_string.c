/* 
   String utility functions
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>

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


/* Wrapper for ne_snprintf. */
size_t ne_snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef HAVE_TRIO
    trio_vsnprintf(str, size, fmt, ap);
#else
    vsnprintf(str, size, fmt, ap);
#endif
    va_end(ap);
    str[size-1] = '\0';
    return strlen(str);
}

/* Wrapper for ne_vsnprintf. */
size_t ne_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
#ifdef HAVE_TRIO
    trio_vsnprintf(str, size, fmt, ap);
#else
    vsnprintf(str, size, fmt, ap);
#endif
    str[size-1] = '\0';
    return strlen(str);
}
