/**
 * \file lib/stringbuf.c
 */

#include "system.h"

#include "stringbuf.h"

#define BUF_CHUNK 1024

struct StringBufRec {
    /*@owned@*/ char *buf;
    /*@dependent@*/ char *tail;     /* Points to first "free" char */
    int allocated;
    int free;
};

StringBuf newStringBuf(void)
{
    StringBuf sb = xmalloc(sizeof(struct StringBufRec));

    sb->free = sb->allocated = BUF_CHUNK;
    sb->buf = xcalloc(sb->allocated, sizeof(*sb->buf));
    sb->buf[0] = '\0';
    sb->tail = sb->buf;
    
    return sb;
}

void freeStringBuf(StringBuf sb)
{
    if (! sb) {
	return;
    }
    
    free(sb->buf);
    free(sb);
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
	if (! isspace(*(sb->tail - 1))) {
	    break;
	}
	sb->free++;
	sb->tail--;
    }
    sb->tail[0] = '\0';
}

char *getStringBuf(StringBuf sb)
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
