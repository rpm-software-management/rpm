#include <stdlib.h>
#include <ctype.h>
#include "stringbuf.h"

#define BUF_CHUNK 1024

struct StringBufRec {
    char *buf;
    char *tail;     /* Points to first "free" char (usually '\0') */
    int allocated;
    int free;
};

StringBuf newStringBuf(void)
{
    StringBuf sb = malloc(sizeof(struct StringBufRec));

    sb->buf = malloc(BUF_CHUNK * sizeof(char));
    sb->buf[0] = '\0';
    sb->tail = sb->buf;
    sb->allocated = BUF_CHUNK;
    sb->free = BUF_CHUNK;
    
    return sb;
}

void freeStringBuf(StringBuf sb)
{
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

void appendStringBufAux(StringBuf sb, char *s, int nl)
{
    int l;

    l = strlen(s);
    /* If free == l there is no room for NULL terminator! */
    while ((l + nl + 1) > sb->free) {
        sb->allocated += BUF_CHUNK;
	sb->free += BUF_CHUNK;
        sb->buf = realloc(sb->buf, sb->allocated);
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
