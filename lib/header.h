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
typedef unsigned short uint_16;

#else

typedef long long int int_64;
typedef int int_32;
typedef short int int_16;
typedef char int_8;

typedef unsigned int uint_32;
typedef unsigned short uint_16;
#endif

typedef struct headerToken *Header;
typedef struct headerIteratorS *HeaderIterator;

/* read and write a header from a file */
Header headerRead(int fd, int magicp);
void headerWrite(int fd, Header h, int magicp);
unsigned int headerSizeof(Header h, int magicp);

#define HEADER_MAGIC_NO   0
#define HEADER_MAGIC_YES  1

/* load and unload a header from a chunk of memory */
Header headerLoad(void *p);
void *headerUnload(Header h);

Header headerNew(void);
void headerFree(Header h);

/* dump a header to a file, in human readable format */
void headerDump(Header h, FILE *f, int flags);

#define HEADER_DUMP_INLINE   1

int headerGetEntry(Header h, int_32 tag, int_32 *type, void **p, int_32 *c);
int headerAddEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c);
int headerModifyEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c);

int headerIsEntry(Header h, int_32 tag);

HeaderIterator headerInitIterator(Header h);
int headerNextIterator(HeaderIterator iter,
		       int_32 *tag, int_32 *type, void **p, int_32 *c);
void headerFreeIterator(HeaderIterator iter);

Header headerCopy(Header h);

/* Entry Types */

#define RPM_NULL_TYPE         0
#define RPM_CHAR_TYPE         1
#define RPM_INT8_TYPE         2
#define RPM_INT16_TYPE        3
#define RPM_INT32_TYPE        4
#define RPM_INT64_TYPE        5
#define RPM_STRING_TYPE       6
#define RPM_BIN_TYPE          7
#define RPM_STRING_ARRAY_TYPE 8

#endif _header_h
