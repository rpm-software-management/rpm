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
 *	- @todo Document version2 headers.
 *
 * - version 3	(added in rpm-3.0)
 *	- Added RPM_I18NSTRING_TYPE as an associative array reference
 *	  for i18n locale dependent single element tags (i.e Group).
 *	- Added an 8 byte magic string to headers in packages on-disk. The
 *	  magic string was not added to headers in the database.
 *
 * - version 4	(added in rpm-4.0)
 *	- Represent file names as a (dirname/basename/dirindex) triple
 *	  rather than as an absolute path name. Legacy package headers are
 *	  converted when the header is read. Legacy database headers are
 *	  converted when the database is rebuilt.
 *	- Simplify dependencies by eliminating the implict check on
 *	  package name/version/release in favor of an explict check
 *	  on package provides. Legacy package headers are converted
 *	  when the header is read. Legacy database headers are
 *        converted when the database is rebuilt.
 *	- (rpm-4.0.2) The original package header (and all original
 *	  metadata) is preserved in what's called an "immutable header region".
 *	  The original header can be retrieved as an RPM_BIN_TYPE, just
 *	  like any other tag, and the original header reconstituted using
 *	  headerLoad().
 *	- (rpm-4.0.2) The signature tags are added (and renumbered to avoid
 *	  tag value collisions) to the package header during package
 *	  installation.
 *	- (rpm-4.0.3) A SHA1 digest of the original header is appended
 *	  (i.e. detached digest) to the immutable header region to verify
 *	  changes to the original header.
 * .
 *
 * \par Development Issues
 *
 * Here's a brief description of future features/incompatibilities that
 * will be added to headers.
 *
 * - Private header methods.
 *	- Private methods (e.g. headerLoad(), headerUnload(), etc.) to
 *	  permit header data to be manipulated opaquely. Initially
 *	  the transaction element file info TFI_t will be used as
 *	  proof-of-concept, binary XML will probably be implemented
 *	  soon thereafter.
 * - DSA signature for header metadata.
 *	- The manner in which rpm packages are signed is going to change.
 *	  The SHA1 digest in the header will be signed, equivalent to a DSA
 *	  digital signature on the original header metadata. As the original
 *	  header will contain "trusted" (i.e. because the header is signed
 *	  with DSA) file MD5 digests, there will be little or no reason
 *	  to sign the payload, but that may happen as well. Note that cpio
 *	  headers in the payload are not used to install package metadata,
 *	  only the name field in the cpio header is used to associate an
 *	  archive file member with the corresponding entry for the file
 *	  in header metadata.
 */

/* RPM - Copyright (C) 1995-2001 Red Hat Software */

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

/*@-redef@*/	/* LCL: no clue */
/** \ingroup header
 */
typedef const char *	errmsg_t;

/** \ingroup header
 */
typedef int_32 *	hTAG_t;
typedef int_32 *	hTYP_t;
typedef const void *	hPTR_t;
typedef int_32 *	hCNT_t;

/** \ingroup header
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct headerToken * Header;

/** \ingroup header
 */
typedef /*@abstract@*/ struct headerIteratorS * HeaderIterator;

/** \ingroup header
 * Associate tag names with numeric values.
 */
typedef /*@abstract@*/ struct headerTagTableEntry_s * headerTagTableEntry;
struct headerTagTableEntry_s {
/*@observer@*/ /*@null@*/ const char * name;	/*!< Tag name. */
    int val;					/*!< Tag numeric value. */
};

/** \ingroup header
 */
enum headerSprintfExtenstionType {
    HEADER_EXT_LAST = 0,	/*!< End of extension chain. */
    HEADER_EXT_FORMAT,		/*!< headerTagFormatFunction() extension */
    HEADER_EXT_MORE,		/*!< Chain to next table. */
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
typedef int (*headerTagTagFunction) (Header h,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * data,
		/*@null@*/ /*@out@*/ hCNT_t count,
		/*@null@*/ /*@out@*/ int * freeData);

/** \ingroup header
 * Define header tag output formats.
 */
typedef /*@abstract@*/ struct headerSprintfExtension_s * headerSprintfExtension;
struct headerSprintfExtension_s {
    enum headerSprintfExtenstionType type;	/*!< Type of extension. */
/*@observer@*/ /*@null@*/ const char * name;	/*!< Name of extension. */
    union {
/*@unused@*/ void * generic;			/*!< Private extension. */
	headerTagFormatFunction formatFunction; /*!< HEADER_EXT_TAG extension. */
	headerTagTagFunction tagFunction;	/*!< HEADER_EXT_FORMAT extension. */
	struct headerSprintfExtension_s * more;	/*!< Chained table extension. */
    } u;
};

/** \ingroup header
 * Supported default header tag output formats.
 */
/*@-redecl@*/
extern const struct headerSprintfExtension_s headerDefaultFormats[];
/*@=redecl@*/

/** \ingroup header
 * Include calculation for 8 bytes of (magic, 0)?
 */
enum hMagic {
	HEADER_MAGIC_NO		= 0,
	HEADER_MAGIC_YES	= 1
};

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
 * New rpm data types under consideration/development.
 * These data types may (or may not) be added to rpm at some point. In order
 * to avoid incompatibility with legacy versions of rpm, these data (sub-)types
 * are introduced into the header by overloading RPM_BIN_TYPE, with the binary
 * value of the tag a 16 byte image of what should/will be in the header index,
 * followed by per-tag private data.
 */
/*@-enummemuse -typeuse @*/
typedef enum rpmSubTagType_e {
	RPM_REGION_TYPE		= -10,
	RPM_BIN_ARRAY_TYPE	= -11,
  /*!<@todo Implement, kinda like RPM_STRING_ARRAY_TYPE for known (but variable)
	length binary data. */
	RPM_XREF_TYPE		= -12
  /*!<@todo Implement, intent is to to carry a (???,tagNum,valNum) cross
	reference to retrieve data from other tags. */
} rpmSubTagType;
/*@=enummemuse =typeuse @*/

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

/**
 */
/*@-typeuse -fielduse@*/
typedef union hRET_s {
    const void * ptr;
    const char ** argv;
    const char * str;
    uint_32 * ui32p;
    uint_16 * ui16p;
    int_32 * i32p;
    int_16 * i16p;
    int_8 * i8p;
} * hRET_t;
/*@=typeuse =fielduse@*/

/**
 */
/*@-typeuse -fielduse@*/
typedef struct HE_s {
    int_32 tag;
/*@null@*/ hTYP_t typ;
    union {
/*@null@*/ hPTR_t * ptr;
/*@null@*/ hRET_t * ret;
    } u;
/*@null@*/ hCNT_t cnt;
} * HE_t;
/*@=typeuse =fielduse@*/

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
typedef
Header (*HDRnew) (void)
	/*@*/;

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
typedef
/*@null@*/ Header (*HDRfree) (/*@null@*/ /*@killref@*/ Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
typedef
Header (*HDRlink) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Sort tags in header.
 * @todo Eliminate from API.
 * @param h		header
 */
typedef
void (*HDRsort) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Restore tags in header to original ordering.
 * @todo Eliminate from API.
 * @param h		header
 */
typedef
void (*HDRunsort) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
typedef
unsigned int (*HDRsizeof) (/*@null@*/ Header h, enum hMagic magicp)
        /*@modifies h @*/;

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header blob (i.e. with offsets)
 */
typedef
/*@only@*/ /*@null@*/ void * (*HDRunload) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
typedef
/*@null@*/ Header (*HDRreload) (/*@only@*/ Header h, int tag)
        /*@modifies h @*/;

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
typedef
Header (*HDRcopy) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
typedef
/*@null@*/ Header (*HDRload) (/*@kept@*/ void * uh)
	/*@modifies uh @*/;

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
typedef
/*@null@*/ Header (*HDRcopyload) (const void * uh)
	/*@*/;

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
typedef
/*@null@*/ Header (*HDRhdrread) (FD_t fd, enum hMagic magicp)
	/*@modifies fd, fileSystem @*/;

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @param magicp	prefix write with 8 bytes of (magic, 0)?
 * @return		0 on success, 1 on error
 */
typedef
int (*HDRhdrwrite) (FD_t fd, /*@null@*/ Header h, enum hMagic magicp)
	/*@modifies fd, h, fileSystem @*/;

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRisentry) (/*@null@*/Header h, int_32 tag)
        /*@*/;  

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef
/*@null@*/ void * (*HDRfreetag) (Header h,
		/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/;

/** \ingroup header
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRget) (Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ void ** p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/;

/** \ingroup header
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRgetmin) (Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hPTR_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/;

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
typedef
int (*HDRadd) (Header h, int_32 tag, int_32 type, const void * p, int_32 c)
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
typedef
int (*HDRappend) (Header h, int_32 tag, int_32 type, const void * p, int_32 c)
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
typedef
int (*HDRaddorappend) (Header h, int_32 tag, int_32 type, const void * p, int_32 c)
        /*@modifies h @*/;

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
typedef
int (*HDRaddi18n) (Header h, int_32 tag, const char * string,
                const char * lang)
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
typedef
int (*HDRmodify) (Header h, int_32 tag, int_32 type, const void * p, int_32 c)
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
typedef
int (*HDRremove) (Header h, int_32 tag)
        /*@modifies h @*/;

/** \ingroup header
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 *
 * @param h		header
 * @param fmt		format to use
 * @param tags		array of tag name/value pairs
 * @param extensions	chained table of formatting extensions.
 * @retval errmsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
typedef
/*@only@*/ char * (*HDRhdrsprintf) (Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tags,
		     const struct headerSprintfExtension_s * extensions,
		     /*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies *errmsg @*/;

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
typedef
void (*HDRcopytags) (Header headerFrom, Header headerTo, hTAG_t tagstocopy)
	/*@modifies headerFrom, headerTo @*/;

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
typedef
HeaderIterator (*HDRfreeiter) (/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/;

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
typedef
HeaderIterator (*HDRinititer) (Header h)
	/*@modifies h */;

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval tag		address of tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRnextiter) (HeaderIterator hi,
		/*@null@*/ /*@out@*/ hTAG_t tag,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies hi, *tag, *type, *p, *c @*/;

/** \ingroup header
 * Header method vectors.
 */
typedef /*@abstract@*/ struct HV_s * HV_t;
struct HV_s {
    HDRnew	hdrnew;
    HDRfree	hdrfree;
    HDRlink	hdrlink;
    HDRsort	hdrsort;
    HDRunsort	hdrunsort;
    HDRsizeof	hdrsizeof;
    HDRunload	hdrunload;
    HDRreload	hdrreload;
    HDRcopy	hdrcopy;
    HDRload	hdrload;
    HDRcopyload	hdrcopyload;
    HDRhdrread	hdrread;
    HDRhdrwrite	hdrwrite;
    HDRisentry	hdrisentry;
    HDRfreetag	hdrfreetag;
    HDRget	hdrget;
    HDRgetmin	hdrgetmin;
    HDRadd	hdradd;
    HDRappend	hdrappend;
    HDRaddorappend hdraddorappend;
    HDRaddi18n	hdraddi18n;
    HDRmodify	hdrmodify;
    HDRremove	hdrremove;
    HDRhdrsprintf hdrsprintf;
    HDRcopytags	hdrcopytags;
    HDRfreeiter	hdrfreeiter;
    HDRinititer	hdrinititer;
    HDRnextiter	hdrnextiter;
    void *	hdrvecs;
    void *	hdrdata;
    int		hdrversion;
};

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @deprecated Use headerFreeTag() instead.
 * @todo Remove from API.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/
void * headerFreeData( /*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/
{
    if (data) {
	/*@-branchstate@*/
	if (type == -1 ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		free((void *)data);
	/*@=branchstate@*/
    }
    return NULL;
}

#if defined(__HEADER_PROTOTYPES__)
#include <hdrproto.h>
#else
#include <hdrinline.h>
#endif

#ifdef __cplusplus
}
#endif

#endif	/* H_HEADER */
