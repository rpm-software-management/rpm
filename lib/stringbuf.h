#ifndef _STRINGBUF_H_
#define _STRINGBUF_H_

typedef struct StringBufRec *StringBuf;

StringBuf newStringBuf(void);
void freeStringBuf(StringBuf sb);
void truncStringBuf(StringBuf sb);
char *getStringBuf(StringBuf sb);
void stripTrailingBlanksStringBuf(StringBuf sb);

#define appendStringBuf(sb, s)     appendStringBufAux(sb, s, 0)
#define appendLineStringBuf(sb, s) appendStringBufAux(sb, s, 1)

void appendStringBufAux(StringBuf sb, char *s, int nl);

#endif _STRINGBUF_H_
