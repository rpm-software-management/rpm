/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * header.h - routines for managing rpm tagged structures
 */

#ifndef _header_h
#define _header_h
#include <stdio.h>

#if defined(__alpha__)
typedef long int int_64;
typedef int int_32;
typedef short int int_16;
typedef char int_8;

typedef unsigned int uint_32;

#else

typedef long long int int_64;
typedef int int_32;
typedef short int int_16;
typedef char int_8;

typedef unsigned int uint_32;
#endif

typedef struct headerToken *Header;
typedef struct headerIteratorS *HeaderIterator;

/* read and write a header from a file */
Header readHeader(int fd);
void writeHeader(int fd, Header h);
unsigned int sizeofHeader(Header h);

/* load and unload a header from a chunk of memory */
Header loadHeader(void *p);
void *unloadHeader(Header h);

Header newHeader(void);
void freeHeader(Header h);

/* dump a header to a file, in human readable format */
void dumpHeader(Header h, FILE *f, int flags);

#define DUMP_INLINE   1
#define DUMP_SYMBOLIC 2

int getEntry(Header h, int_32 tag, int_32 *type, void **p, int_32 *c);
int addEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c);
int modifyEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c);

int isEntry(Header h, int_32 tag);

HeaderIterator initIterator(Header h);
int nextIterator(HeaderIterator iter,
		 int_32 *tag, int_32 *type, void **p, int_32 *c);
void freeIterator(HeaderIterator iter);

Header copyHeader(Header h);

/* Entry Types */

#define NULL_TYPE 0
#define CHAR_TYPE 1
#define INT8_TYPE 2
#define INT16_TYPE 3
#define INT32_TYPE 4
#define INT64_TYPE 5
#define STRING_TYPE 6
#define BIN_TYPE 7
#define STRING_ARRAY_TYPE 8

#endif _header_h
