#ifndef H_HEADER
#define H_HEADER

/** \file lib/header.h
 *
 */

/* RPM - Copyright (C) 1995 Red Hat Software */

/* WARNING: 1 means success, 0 means failure (yes, this is backwards) */

#include <stdio.h>
#include <zlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__alpha__) || defined(__alpha)
typedef long int int_64;
typedef int int_32;
typedef short int int_16;
typedef char int_8;

typedef unsigned int uint_32;
typedef unsigned short uint_16;

#else

#if 0	/* XXX hpux needs -Ae in CFLAGS to grok this */
typedef long long int int_64;
#endif
typedef int int_32;
typedef short int int_16;
typedef char int_8;

typedef unsigned int uint_32;
typedef unsigned short uint_16;
#endif

/** !ingroup header
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct headerToken *Header;

/** !ingroup header
 */
typedef /*@abstract@*/ struct headerIteratorS *HeaderIterator;

/** \ingroup header
 * Associate tag names with numeric values.
 */
struct headerTagTableEntry {
    const char * name;		/*!< Tag name. */
    int val;			/*!< Tag numeric value. */
};

/** \ingroup header
 */
enum headerSprintfExtenstionType {
	HEADER_EXT_LAST = 0,	/*!< End of extension chain. */
	HEADER_EXT_FORMAT,	/*!< headerTagFormatFunction() extension */
	HEADER_EXT_MORE,	/*!< Chain to next table. */
	HEADER_EXT_TAG		/*!< headerTagTagFunction() extension */
};

/** \ingroup header
 * HEADER_EXT_TAG format function prototype.
 * This will only ever be passed RPM_TYPE_INT32 or RPM_TYPE_STRING to
 * help keep things simple
 *
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element
 * @return		formatted string
 */
typedef /*only@*/ char * (*headerTagFormatFunction)(int_32 type,
				const void * data, char * formatPrefix,
				int padding, int element);
/** \ingroup header
 * HEADER_EXT_FORMAT format function prototype.
 * This is allowed to fail, which indicates the tag doesn't exist.
 *
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
typedef int (*headerTagTagFunction)(Header h, int_32 * type, const void ** data,
				       int_32 * count, int * freeData);

/** \ingroup header
 * Define header tag output formats.
 */
struct headerSprintfExtension {
    enum headerSprintfExtenstionType type;	/*!< Type of extension. */
    char * name;				/*!< Name of extension. */
    union {
	void * generic;				/*!< Private extension. */
	headerTagFormatFunction formatFunction; /*!< HEADER_EXT_TAG extension. */
	headerTagTagFunction tagFunction;	/*!< HEADER_EXT_FORMAT extension. */
	struct headerSprintfExtension * more;	/*!< Chained table extension. */
    } u;
};

/** \ingroup header
 * Supported default header tag output formats.
 */
extern const struct headerSprintfExtension headerDefaultFormats[];

/** \ingroup header
 * Include calculation for 8 bytes of (magic, 0)?
 */
enum hMagic {
	HEADER_MAGIC_NO		= 0,
	HEADER_MAGIC_YES	= 1
};

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
Header headerRead(FD_t fd, enum hMagic magicp)
	/*@modifies fd @*/;

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @param magicp	prefix write with 8 bytes of (magic, 0)?
 * @return		0 on success, 1 on error
 */
int headerWrite(FD_t fd, Header h, enum hMagic magicp)
	/*@modifies fd, h @*/;

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
unsigned int headerSizeof(Header h, enum hMagic magicp)
	/*@modifies h @*/;

/** \ingroup header
 * Convert header to in-memory representation.
 * @param p		on-disk header (with offsets)
 * @return		header
 */
Header headerLoad(void *p)	/*@*/;

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header (with offsets)
 */
void *headerUnload(Header h)
	/*@modifes h @*/;

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
Header headerNew(void)	/*@*/;

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
Header headerLink(Header h)	/*@modifies h @*/;

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 */
void headerFree( /*@killref@*/ Header h);

/** \ingroup header
 * Return header reference count.
 * @param h		header
 * @return		no. of references
 */
int headerUsageCount(Header h)	/*@*/;

/** \ingroup header
 * Dump a header in human readable format (for debugging).
 * @param h		header
 * @param flags		0 or HEADER_DUMP_LINLINE
 * @param tags		array of tag name/value pairs
 */
void headerDump(Header h, FILE *f, int flags,
		const struct headerTagTableEntry * tags);
#define HEADER_DUMP_INLINE   1

typedef /*@observer@*/ const char * errmsg_t;

/** \ingroup header
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 *
 * @param h		header
 * @param fmt		format to use
 * @param tags		array of tag name/value pairs
 * @param extentions	chained table of formatting extensions.
 * @retval errmsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
/*@only@*/ char * headerSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry * tags,
		     const struct headerSprintfExtension * extentions,
		     /*@out@*/ errmsg_t * errmsg)
	/*@modifies h, *errmsg @*/;

/** \ingroup header
 * Add tag to header.
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
int headerAddEntry(Header h, int_32 tag, int_32 type, const void *p, int_32 c)
	/*@modifies h @*/;

/** \ingroup header
 * Modify tag in header.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
int headerModifyEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c)
	/*@modifies h @*/;

/** \ingroup header
 * Return array of locales found in header.
 * The array is terminated with a NULL sentinel.
 * @param h		header
 * @return		array of locales (or NULL on error)
 */
char ** headerGetLangs(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Add locale specific tag to header.
 * A NULL lang is interpreted as the C locale. Here are the rules:
 * \verbatim
 *	- If the tag isn't in the Header, it's added with the passed string
 *	   as a version.
 *	- If the tag occurs multiple times in entry, which tag is affected
 *	   by the operation is undefined.
 *	- If the tag is in the header w/ this language, the entry is
 *	   *replaced* (like headerModifyEntry()).
 * \endverbatim
 * This function is intended to just "do the right thing". If you need
 * more fine grained control use headerAddEntry() and headerModifyEntry().
 *
 * @param h		header
 * @param tag		tag
 * @param string	tag value
 * @param lang		locale
 * @return		1 on success, 0 on failure
 */
int headerAddI18NString(Header h, int_32 tag, const char * string,
	const char * lang)
	/*@modifies h @*/;

/** \ingroup header
 * Append element to tag array in header.
 * Appends item p to entry w/ tag and type as passed. Won't work on
 * RPM_STRING_TYPE. Any pointers from headerGetEntry() for this entry
 * are invalid after this call has been made!
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
int headerAppendEntry(Header h, int_32 tag, int_32 type, void * p, int_32 c)
	/*@modifies h @*/;

/** \ingroup header
 * Add or append element to tag array in header.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type,
			   void * p, int_32 c)
		/*@modifies h @*/;

/** \ingroup header
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
int headerGetEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
	/*@out@*/ void **p, /*@out@*/int_32 *c)
		/*@modifies h, *type, *p, *c @*/;

/** \ingroup header
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
int headerGetEntryMinMemory(Header h, int_32 tag, int_32 *type,
	/*@out@*/ void **p, /*@out@*/ int_32 *c)
		/*@modifies h, *type, *p, *c @*/;

/** \ingroup header
 * Retrieve tag value with type match.
 * If *type is RPM_NULL_TYPE any type will match, otherwise only *type will
 * match.
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
int headerGetRawEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
	/*@out@*/ void **p, /*@out@*/ int_32 *c)
		/*@modifies h, *type, *p, *c @*/;

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
int headerIsEntry(Header h, int_32 tag)
	/*@modifies h @*/;

/** \ingroup header
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
int headerRemoveEntry(Header h, int_32 tag)
	/*@modifies h @*/;

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
HeaderIterator headerInitIterator(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Return next tag from header.
 * @param iter		header tag iterator
 * @retval tag		address of tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
int headerNextIterator(HeaderIterator iter,
	/*@out@*/ int_32 *tag, /*@out@*/ int_32 *type, /*@out@*/ void **p,
	/*@out@*/ int_32 *c)
		/*@modifies iter, *tag, *type, *p, *c @*/;

/** \ingroup header
 * Destroy header tag iterator.
 * @param iter		header tag iterator
 */
void headerFreeIterator( /*@only@*/ HeaderIterator iter);

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
Header headerCopy(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
void headerSort(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom		source header
 * @param headerTo		destination header
 * @param tagstocopy		array of tags that are copied
 */
void headerCopyTags(Header headerFrom, Header headerTo, int_32 *tagstocopy)
	/*@modifies headerFrom, headerTo @*/;

/* Entry Types */

#define	RPM_MIN_TYPE		0

/**
 * @todo Add RPM_XREF_TYPE to carry (hdrNum,tagNum,valNum) cross reference.
 */
typedef enum rpmTagType_e {
	RPM_NULL_TYPE		= 0,
	RPM_CHAR_TYPE		= 1,
	RPM_INT8_TYPE		= 2,
	RPM_INT16_TYPE		= 3,
	RPM_INT32_TYPE		= 4,
/*	RPM_INT64_TYPE	= 5,   ---- These aren't supported (yet) */
	RPM_STRING_TYPE		= 6,
	RPM_BIN_TYPE		= 7,
	RPM_STRING_ARRAY_TYPE	= 8,
	RPM_I18NSTRING_TYPE	= 9
} rpmTagType;

#define	RPM_MAX_TYPE		9

/* Tags -- general use tags should start at 1000 (RPM's tag space starts
   there) */

#define HEADER_I18NTABLE	100

#ifdef __cplusplus
}
#endif

#endif	/* H_HEADER */
