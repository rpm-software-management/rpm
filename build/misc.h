#ifndef _MISC_H_
#define _MISC_H_

#include "spec.h"
#include "ctype.h"

#define FREE(x) { if (x) free(x); x = NULL; }

#define SKIPSPACE(s) { while (*(s) && isspace(*(s))) (s)++; }

#define SKIPNONSPACE(s) { while (*(s) && !isspace(*(s))) (s)++; }

#define SKIPTONEWLINE(s) { while (*s && *s != '\n') s++; }

#define PART_SUBNAME  0
#define PART_NAME     1

void addOrAppendListEntry(Header h, int_32 tag, char *line);
int parseSimplePart(char *line, char **name, int *flag);
char *findLastChar(char *s);
int parseYesNo(char *s);
int parseNum(char *line, int *res);
StringBuf getOutputFrom(char *dir, char *argv[],
			char *writePtr, int writeBytesLeft,
			int failNonZero);

#endif
