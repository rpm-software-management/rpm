/**
 * \file lib/stringbuf.c
 */

#include "system.h"

#include "stringbuf.h"
#include "debug.h"

#define BUF_CHUNK 1024

struct StringBufRec {
    /*@owned@*/ char *buf;
    /*@dependent@*/ char *tail;     /* Points to first "free" char */
    int allocated;
    int free;
};

/**
 * Locale insensitive isspace(3).
 */
/*@unused@*/ static inline int xisspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p) /*@modifies *p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

StringBuf newStringBuf(void)
{
    StringBuf sb = xmalloc(sizeof(struct StringBufRec));

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
	if (! xisspace(*(sb->tail - 1))) {
	    break;
	}
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
    
    /*@-mayaliasunique@*/ /* FIX: shrug */
    strcpy(sb->tail, s);
    /*@=mayaliasunique@*/
    sb->tail += l;
    sb->free -= l;
    if (nl) {
        sb->tail[0] = '\n';
        sb->tail[1] = '\0';
	sb->tail++;
	sb->free--;
    }
}
