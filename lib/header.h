/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * header.h - routines for managing rpm tagged structures
 */

/* WARNING: 1 means success, 0 means failure (yes, this is backwards) */

#ifndef H_HEADER
#define H_HEADER
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

struct headerTagTableEntry {
    char * name;
    int val;
};

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
void headerDump(Header h, FILE *f, int flags, 
		const struct headerTagTableEntry * tags);

#define HEADER_DUMP_INLINE   1

/* I18N items need an RPM_STRING_TYPE entry (used by default) and an
   RPM_18NSTRING_TYPE table added. Dups are okay, but only defined for
   iteration (with the exceptions noted below) */
int headerAddEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c);
int headerModifyEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c);

/* Appends item p to entry w/ tag and type as passed. Won't work on
   RPM_STRING_TYPE. Any pointers from headerGetEntry() for this entry
   are invalid after this call has been made! */
int headerAppendEntry(Header h, int_32 tag, int_32 type, void * p, int_32 c);

/* Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements w/
   RPM_I18NSTRING_TYPE equivalent enreies are translated (if HEADER_I18NTABLE
   entry is present). */
int headerGetEntry(Header h, int_32 tag, int_32 *type, void **p, int_32 *c);

/* If *type is RPM_NULL_TYPE any type will match, otherwise only *type will
   match. */
int headerGetRawEntry(Header h, int_32 tag, int_32 *type, void **p, int_32 *c);

int headerIsEntry(Header h, int_32 tag);
/* removes all entries of type tag from the header, returns 1 if none were
   found */
int headerRemoveEntry(Header h, int_32 tag);

HeaderIterator headerInitIterator(Header h);
int headerNextIterator(HeaderIterator iter,
		       int_32 *tag, int_32 *type, void **p, int_32 *c);
void headerFreeIterator(HeaderIterator iter);

Header headerCopy(Header h);

/* Entry Types */

#define RPM_NULL_TYPE		0
#define RPM_CHAR_TYPE		1
#define RPM_INT8_TYPE		2
#define RPM_INT16_TYPE		3
#define RPM_INT32_TYPE		4
/* #define RPM_INT64_TYPE	5   ---- These aren't supported (yet) */
#define RPM_STRING_TYPE		6
#define RPM_BIN_TYPE		7
#define RPM_STRING_ARRAY_TYPE	8
#define RPM_I18NSTRING_TYPE	9

/* Tags -- general use tags should start at 1000 (RPM's tag space starts 
   there) */

#define HEADER_I18NTABLE	100

#endif H_HEADER
