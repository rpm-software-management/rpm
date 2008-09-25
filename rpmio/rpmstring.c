/**
 * \file rpmio/rpmstring.c
 */

#include "system.h"

#include <stdarg.h>

#include <rpm/rpmstring.h>
#include "debug.h"

#define BUF_CHUNK 1024

struct StringBufRec {
    char *buf;
    char *tail;     /* Points to first "free" char */
    int allocated;
    int free;
};

char * stripTrailingChar(char * s, char c)
{
    char * t;
    for (t = s + strlen(s) - 1; *t == c && t >= s; t--)
	*t = '\0';
    return s;
}

StringBuf newStringBuf(void)
{
    StringBuf sb = xmalloc(sizeof(*sb));

    sb->free = sb->allocated = BUF_CHUNK;
    sb->buf = xcalloc(sb->allocated, sizeof(*sb->buf));
    sb->buf[0] = '\0';
    sb->tail = sb->buf;
    
    return sb;
}

StringBuf freeStringBuf(StringBuf sb)
{
    if (sb) {
	sb->buf = _free(sb->buf);
	sb = _free(sb);
    }
    return sb;
}

void truncStringBuf(StringBuf sb)
{
    sb->buf[0] = '\0';
    sb->tail = sb->buf;
    sb->free = sb->allocated;
}

void stripTrailingBlanksStringBuf(StringBuf sb)
{
    while (sb->free != sb->allocated) {
	if (! risspace(*(sb->tail - 1)))
	    break;
	sb->free++;
	sb->tail--;
    }
    sb->tail[0] = '\0';
}

char * getStringBuf(StringBuf sb)
{
    return sb->buf;
}

void appendStringBufAux(StringBuf sb, const char *s, int nl)
{
    int l;

    l = strlen(s);
    /* If free == l there is no room for NULL terminator! */
    while ((l + nl + 1) > sb->free) {
        sb->allocated += BUF_CHUNK;
	sb->free += BUF_CHUNK;
        sb->buf = xrealloc(sb->buf, sb->allocated);
	sb->tail = sb->buf + (sb->allocated - sb->free);
    }
    
    /* FIX: shrug */
    strcpy(sb->tail, s);
    sb->tail += l;
    sb->free -= l;
    if (nl) {
        sb->tail[0] = '\n';
        sb->tail[1] = '\0';
	sb->tail++;
	sb->free--;
    }
}

int rstrcasecmp(const char * s1, const char * s2)
{
  const char * p1 = s1;
  const char * p2 = s2;
  char c1, c2;

  if (p1 == p2)
    return 0;

  do
    {
      c1 = rtolower (*p1++);
      c2 = rtolower (*p2++);
      if (c1 == '\0')
        break;
    }
  while (c1 == c2);

  return (int)(c1 - c2);
}

int rstrncasecmp(const char *s1, const char *s2, size_t n)
{
  const char * p1 = s1;
  const char * p2 = s2;
  char c1, c2;

  if (p1 == p2 || n == 0)
    return 0;

  do
    {
      c1 = rtolower (*p1++);
      c2 = rtolower (*p2++);
      if (c1 == '\0' || c1 != c2)
	break;
    } while (--n > 0);

  return (int)(c1 - c2);
}

/* 
 * Simple and stupid asprintf() clone.
 * FIXME: write to work with non-C99 vsnprintf or check for one in configure.
 */
int rasprintf(char **strp, const char *fmt, ...)
{
    int n;
    va_list ap;
    char * p = NULL;
  
    if (strp == NULL) 
	return -1;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n >= -1) {
	size_t nb = n + 1;
	p = xmalloc(nb);
    	va_start(ap, fmt);
        n = vsnprintf(p, nb, fmt, ap);
    	va_end(ap);
    } 
    *strp = p;
    return n;
}

/*
 * Concatenate two strings with dynamically (re)allocated
 * memory what prevents static buffer overflows by design.
 * *dest is reallocated to the size of strings to concatenate.
 *
 * Note:
 * 1) char *buf = rstrcat(NULL,"string"); is the same like rstrcat(&buf,"string");
 * 2) rstrcat(&buf,NULL) returns buf
 * 3) rstrcat(NULL,NULL) returns NULL
 * 4) *dest and src can overlap
 */
char *rstrcat(char **dest, const char *src)
{
    if ( src == NULL ) {
	return dest != NULL ? *dest : NULL;
    }

    if ( dest == NULL ) {
	return xstrdup(src);
    }

    {
	size_t dest_size = *dest != NULL ? strlen(*dest) : 0;
	size_t src_size = strlen(src);

	*dest = xrealloc(*dest, dest_size+src_size+1);		/* include '\0' */
	memmove(&(*dest)[dest_size], src, src_size+1);
    }

    return *dest;
}

/*
 * Concatenate strings with dynamically (re)allocated
 * memory what prevents static buffer overflows by design.
 * *dest is reallocated to the size of strings to concatenate.
 * List of strings has to be NULL terminated.
 *
 * Note:
 * 1) char *buf = rstrscat(NULL,"string",NULL); is the same like rstrscat(&buf,"string",NULL);
 * 2) rstrscat(&buf,NULL) returns buf
 * 3) rstrscat(NULL,NULL) returns NULL
 * 4) *dest and argument strings can overlap
 */
char *rstrscat(char **dest, const char *arg, ...)
{
    va_list ap;
    size_t arg_size, dst_size;
    const char *s;
    char *dst, *p;

    dst = dest ? *dest : NULL;

    if ( arg == NULL ) {
        return dst;
    }

    va_start(ap, arg);
    for (arg_size=0, s=arg; s; s = va_arg(ap, const char *))
        arg_size += strlen(s);
    va_end(ap);

    dst_size = dst ? strlen(dst) : 0;
    dst = xrealloc(dst, dst_size+arg_size+1);    /* include '\0' */
    p = &dst[dst_size];

    va_start(ap, arg);
    for (s = arg; s; s = va_arg(ap, const char *)) {
        size_t size = strlen(s);
        memmove(p, s, size);
        p += size;
    }
    *p = '\0';

    if ( dest ) {
        *dest = dst;
    }

    return dst;
}

/*
 * Adapted from OpenBSD, strlcpy() originally developed by
 * Todd C. Miller <Todd.Miller@courtesan.com>
 */
size_t rstrlcpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    size_t len = n;

    /* Copy as many bytes as will fit */
    if (len != 0) {
	while (--len != 0) {
	    if ((*d++ = *s++) == '\0')
		break;
	}
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (len == 0) {
	if (n != 0)
	    *d = '\0'; /* NUL-terminate dst */
	while (*s++)
	    ;
    }

    return s - src - 1; /* count does not include NUL */
}
