#ifndef H_HEADER
#define H_HEADER

/** \ingroup header
 * \file lib/header.h
 *
 * An rpm header carries all information about a package. A header is
 * a collection of data elements called tags. Each tag has a data type,
 * and includes 1 or more values.
 * 
 * \par Historical Issues
 *
 * Here's a brief description of features/incompatibilities that
 * have been added to headers and tags.
 *
 * - version 1
 *	- Support for version 1 headers was removed in rpm-4.0.
 *
 * - version 2
 *	- (Before my time, sorry.)
 *
 * - version 3	(added in rpm-3.0)
 *	- added RPM_I18NSTRING_TYPE as an associative array reference
 *	  for i18n locale dependent single element tags (i.e Group).
 *	- added an 8 byte magic string to headers in packages on-disk. The
 *	  magic string was not added to headers in the database.
 *
 * - version 4	(added in rpm-4.0)
 *	- represent file names as a (dirname/basename/dirindex) triple
 *	  rather than as an absolute path name. Legacy package headers are
 *	  converted when the header is read. Legacy database headers are
 *	  converted when the database is rebuilt.
 *	- Simplify dependencies by eliminating the implict check on
 *	  package name/version/release in favor of an explict check
 *	  on package provides. Legacy package headers are converted
 *	  when the header is read. Legacy database headers are
 *        converted when the database is rebuilt.
 *
 * .
 *
 * \par Development Issues
 *
 * Here's a brief description of future features/incompatibilities that
 * will be added to headers.
 *
 * - Signature tags
 *	- Signatures are stored in A header, but not THE header
 *	  of a package. That means that signatures are discarded
 *	  when a package is installed, preventing verification
 *	  of header contents after install. All signature tags
 *	  will be added to THE package header so that they are
 *	  saved in the rpm database for later retrieval and verification.
 *	  Adding signatures to THE header will also permit signatures to
 *	  be accessed by Red Hat Network, i.e. retrieval by existing
 *	  Python bindings.
 *	- Signature tag values collide with existing rpm tags, and will
 *	  have to be renumbered. Part of this renumbering was accomplished
 *	  in rpm-4.0, but more remains to be done.
 *	- Signatures, because they involve MD5 and other 1-way hashes on
 *	  immutable data, will cause the header to be reconstituted as a
 *	  immutable section and a mutable section.
 */

/* RPM - Copyright (C) 1995-2000 Red Hat Software */

/* WARNING: 1 means success, 0 means failure (yes, this is backwards) */

#include <stdio.h>
#include <rpmio.h>

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

/** \ingroup header
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct headerToken *Header;

/** \ingroup header
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
/*@null@*/ Header headerRead(FD_t fd, enum hMagic magicp)
	/*@modifies fd @*/;

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @param magicp	prefix write with 8 bytes of (magic, 0)?
 * @return		0 on success, 1 on error
 */
int headerWrite(FD_t fd, /*@null@*/ Header h, enum hMagic magicp)
	/*@modifies fd, h @*/;

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
unsigned int headerSizeof(/*@null@*/ Header h, enum hMagic magicp)
	/*@modifies h @*/;

/** \ingroup header
 * Convert header to in-memory representation.
 * @param p		on-disk header (with offsets)
 * @return		header
 */
/*@null@*/ Header headerLoad(/*@kept@*/ void * p)	/*@*/;

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param p		on-disk header (with offsets)
 * @return		header
 */
/*@null@*/ Header headerCopyLoad(void * p)	/*@*/;

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header (with offsets)
 */
/*@only@*/ void * headerUnload(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
/*@null@*/ Header headerReload(/*@only@*/ Header h, int tag)
	/*@modifies h @*/;

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
Header headerLink(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 */
void headerFree( /*@null@*/ /*@killref@*/ Header h);

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

/*@-redef@*/	/* LCL: no clue */
typedef const char * errmsg_t;
/*@=redef@*/

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
		     /*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies *errmsg @*/;

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
/*@mayexit@*/
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
/*@only@*/ /*@null@*/ char ** headerGetLangs(Header h)	/*@*/;

/** \ingroup header
 * Add locale specific tag to header.
 * A NULL lang is interpreted as the C locale. Here are the rules:
 * \verbatim
 *	- If the tag isn't in the header, it's added with the passed string
 *	   as new value.
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
 * RPM_STRING_TYPE. Any pointers into header memory returned from
 * headerGetEntryMinMemory() for this entry are invalid after this
 * call has been made!
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
 * @todo Arg "p" should have const.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type, void * p, int_32 c)
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
int headerGetEntry(Header h, int_32 tag, /*@null@*/ /*@out@*/ int_32 *type,
	/*@null@*/ /*@out@*/ void **p, /*@null@*/ /*@out@*/int_32 *c)
		/*@modifies *type, *p, *c @*/;

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
int headerGetEntryMinMemory(Header h, int_32 tag, /*@out@*/ int_32 *type,
	/*@out@*/ const void **p, /*@out@*/ int_32 *c)
		/*@modifies *type, *p, *c @*/;

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
/*@-exportlocal@*/
int headerGetRawEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
	/*@out@*/ const void **p, /*@out@*/ int_32 *c)
		/*@modifies *type, *p, *c @*/;
/*@=exportlocal@*/

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
int headerIsEntry(/*@null@*/Header h, int_32 tag)	/*@*/;

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
	/*@modifies h*/;

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
	/*@out@*/ int_32 * tag, /*@out@*/ int_32 * type,
	/*@out@*/ const void ** p, /*@out@*/ int_32 * c)
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
/*@null@*/ Header headerCopy(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
/*@-exportlocal@*/
void headerSort(Header h)
	/*@modifies h @*/;

/** \ingroup header
 * Restore tags in header to original ordering.
 * @param h		header
 */
void headerUnsort(Header h)
	/*@modifies h @*/;
/*@=exportlocal@*/

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
void headerCopyTags(Header headerFrom, Header headerTo, int_32 *tagstocopy)
	/*@modifies headerFrom, headerTo @*/;

/** \ingroup header
 * The basic types of data in tags from headers.
 */
typedef enum rpmTagType_e {
#define	RPM_MIN_TYPE		0
    RPM_NULL_TYPE		=  0,
    RPM_CHAR_TYPE		=  1,
    RPM_INT8_TYPE		=  2,
    RPM_INT16_TYPE		=  3,
    RPM_INT32_TYPE		=  4,
/*    RPM_INT64_TYPE	= 5,   ---- These aren't supported (yet) */
    RPM_STRING_TYPE		=  6,
    RPM_BIN_TYPE		=  7,
    RPM_STRING_ARRAY_TYPE	=  8,
    RPM_I18NSTRING_TYPE		=  9
#define	RPM_MAX_TYPE		9
} rpmTagType;

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param data		address of data
 * @param type		type of data
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/
void * headerFreeData( /*@only@*/ /*@null@*/ const void * data, rpmTagType type)
{
    if (data) {
	if (type < 0 ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		free((void *)data);
    }
    return NULL;
}

/** \ingroup header
 * New rpm data types under consideration/development.
 * These data types may (or may not) be added to rpm at some point. In order
 * to avoid incompatibility with legacy versions of rpm, these data (sub-)types
 * are introduced into the header by overloading RPM_BIN_TYPE, with the binary
 * value of the tag a 16 byte image of what should/will be in the header index,
 * followed by per-tag private data.
 */
typedef enum rpmSubTagType_e {
	RPM_REGION_TYPE		= -10,
	RPM_BIN_ARRAY_TYPE	= -11,
  /*!<@todo Implement, kinda like RPM_STRING_ARRAY_TYPE for known (but variable)
	length binary data. */
	RPM_XREF_TYPE		= -12
  /*!<@todo Implement, intent is to to carry a (???,tagNum,valNum) cross
	reference to retrieve data from other tags. */
} rpmSubTagType;

/**
 * Header private tags.
 * @note General use tags should start at 1000 (RPM's tag space starts there).
 */
#define	HEADER_IMAGE		61
#define	HEADER_SIGNATURES	62
#define	HEADER_IMMUTABLE	63
#define	HEADER_REGIONS		64
#define HEADER_I18NTABLE	100
#define	HEADER_SIGBASE		256
#define	HEADER_TAGBASE		1000

#ifdef __cplusplus
}
#endif

#endif	/* H_HEADER */
