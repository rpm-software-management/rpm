/**
 * \file rpmio/rpmstring.c
 */

#include "system.h"

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

char ** splitString(const char * str, int length, char sep)
{
    const char * source;
    char * s, * dest;
    char ** list;
    int i;
    int fields;

    s = xmalloc(length + 1);

    fields = 1;
    for (source = str, dest = s, i = 0; i < length; i++, source++, dest++) {
	*dest = *source;
	if (*dest == sep) fields++;
    }

    *dest = '\0';

    list = xmalloc(sizeof(*list) * (fields + 1));

    dest = s;
    list[0] = dest;
    i = 1;
    while (i < fields) {
	if (*dest == sep) {
	    list[i++] = dest + 1;
	    *dest = 0;
	}
	dest++;
    }

    list[i] = NULL;

/* FIX: list[i] is NULL */
    return list;
}

void freeSplitString(char ** list)
{
    list[0] = _free(list[0]);
    list = _free(list);
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
	if (! xisspace(*(sb->tail - 1)))
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
