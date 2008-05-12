#ifndef H_HEADER
#define H_HEADER

/** \ingroup header
 * \file lib/header.h
 *
 * An rpm header carries all information about a package. A header is
 * a collection of data elements called tags. Each tag has a data type,
 * and includes 1 or more values.
 * 
 */

/* RPM - Copyright (C) 1995-2001 Red Hat Software */

#include <rpm/rpmio.h>
#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup header
 * Associate tag names with numeric values.
 */
typedef struct headerTagTableEntry_s * headerTagTableEntry;
struct headerTagTableEntry_s {
    const char * name;		/*!< Tag name. */
    const char * shortname;	/*!< "Human readable" short name. */
    rpmTag val;			/*!< Tag numeric value. */
    rpmTagType type;		/*!< Tag type. */
};

/**
 */
typedef struct headerTagIndices_s * headerTagIndices;

/** \ingroup header
 */
enum headerSprintfExtensionType {
    HEADER_EXT_LAST = 0,	/*!< End of extension chain. */
    HEADER_EXT_FORMAT,		/*!< headerTagFormatFunction() extension */
    HEADER_EXT_MORE,		/*!< Chain to next table. */
    HEADER_EXT_TAG		/*!< headerTagTagFunction() extension */
};

/** \ingroup header
 * HEADER_EXT_TAG format function prototype.
 * This will only ever be passed RPM_INT32_TYPE or RPM_STRING_TYPE to
 * help keep things simple.
 *
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	RPM_BIN_TYPE: no. bytes of data
 * @return		formatted string
 */
typedef char * (*headerTagFormatFunction)(rpmTagType type,
				rpm_constdata_t data, char * formatPrefix,
				size_t padding, rpm_count_t element);

/** \ingroup header
 * HEADER_EXT_FORMAT format function prototype.
 * This is allowed to fail, which indicates the tag doesn't exist.
 *
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freedata	data-was-malloc'ed indicator
 * @return		0 on success
 */
typedef int (*headerTagTagFunction) (Header h,
		rpmTagType * type,
		rpm_data_t * data,
		rpm_count_t * count,
		int * freeData);

/** \ingroup header
 * Define header tag output formats.
 */
typedef struct headerSprintfExtension_s * headerSprintfExtension;
struct headerSprintfExtension_s {
    enum headerSprintfExtensionType type;	/*!< Type of extension. */
    const char * name;				/*!< Name of extension. */
    union {
	void * generic;				/*!< Private extension. */
	headerTagFormatFunction formatFunction; /*!< HEADER_EXT_TAG extension. */
	headerTagTagFunction tagFunction;	/*!< HEADER_EXT_FORMAT extension. */
	struct headerSprintfExtension_s * more;	/*!< Chained table extension. */
    } u;
};

/** \ingroup header
 * Supported default header tag output formats.
 */
extern const struct headerSprintfExtension_s headerDefaultFormats[];

/** \ingroup rpmtag
 * Automatically generated table of tag name/value pairs.
 */
extern const struct headerTagTableEntry_s * const rpmTagTable;

/** \ingroup rpmtag
 * Number of entries in rpmTagTable.
 */
extern const int rpmTagTableSize;

/** \ingroup rpmtag
 */
extern headerTagIndices const rpmTags;

/** \ingroup header
 * Table of query format extensions.
 * @note Chains to headerDefaultFormats[].
 */
extern const struct headerSprintfExtension_s rpmHeaderFormats[];

/** \ingroup header
 * Include calculation for 8 bytes of (magic, 0)?
 */
enum hMagic {
    HEADER_MAGIC_NO		= 0,
    HEADER_MAGIC_YES		= 1
};

/* Return types for header data. Not yet... */
#if 0
/**
 */
typedef union hRET_s {
    const void * ptr;
    const char ** argv;
    const char * str;
    uint32_t * ui32p;
    uint16_t * ui16p;
    int32_t * i32p;
    int16_t * i16p;
    int8_t * i8p;
} * hRET_t;

/**
 */
typedef struct HE_s {
    rpmTag tag;
    rpmTagType * typ;
    union {
	hPTR_t * ptr;
	hRET_t * ret;
    } u;
    rpm_count_t * cnt;
} * HE_t;
#endif

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
Header headerNew(void);

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
Header headerFree( Header h);

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		new header reference
 */
Header headerLink(Header h);

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		new header reference
 */
Header headerUnlink(Header h);

/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
void headerSort(Header h);

/** \ingroup header
 * Restore tags in header to original ordering.
 * @param h		header
 */
void headerUnsort(Header h);

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
unsigned int headerSizeof(Header h, enum hMagic magicp);

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header blob (i.e. with offsets)
 */
void * headerUnload(Header h);

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
Header headerReload(Header h, rpmTag tag);

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
Header headerCopy(Header h);

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
Header headerLoad(void * uh);

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
Header headerCopyLoad(const void * uh);

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
Header headerRead(FD_t fd, enum hMagic magicp);

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @param magicp	prefix write with 8 bytes of (magic, 0)?
 * @return		0 on success, 1 on error
 */
int headerWrite(FD_t fd, Header h, enum hMagic magicp);

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
int headerIsEntry(Header h, rpmTag tag);

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		pointer to tag value(s)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
void * headerFreeTag(Header h, rpm_data_t data, rpmTagType type);

/** \ingroup header
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval *type	tag value data type (or NULL)
 * @retval *p		pointer to tag value(s) (or NULL)
 * @retval *c		number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
int headerGetEntry(Header h, rpmTag tag,
			rpmTagType * type,
			rpm_data_t * p,
			rpm_count_t * c);

/** \ingroup header
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @retval *type	tag value data type (or NULL)
 * @retval *p		pointer to tag value(s) (or NULL)
 * @retval *c		number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
int headerGetEntryMinMemory(Header h, rpmTag tag,
			rpmTagType * type,
			rpm_data_t * p, 
			rpm_count_t * c);

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
int headerAddEntry(Header h, rpmTag tag, rpmTagType type, rpm_constdata_t p, rpm_count_t c);

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
int headerAppendEntry(Header h, rpmTag tag, rpmTagType type,
		rpm_constdata_t p, rpm_count_t c);

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
int headerAddOrAppendEntry(Header h, rpmTag tag, rpmTagType type,
		rpm_constdata_t p, rpm_count_t c);

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
int headerAddI18NString(Header h, rpmTag tag, const char * string,
		const char * lang);

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
int headerModifyEntry(Header h, rpmTag tag, rpmTagType type,
			rpm_constdata_t p, rpm_count_t c);

/** \ingroup header
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
int headerRemoveEntry(Header h, rpmTag tag);

/** \ingroup header
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 *
 * @param h		header
 * @param fmt		format to use
 * @param tbltags	array of tag name/value pairs
 * @param extensions	chained table of formatting extensions.
 * @retval errmsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
char * headerSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tbltags,
		     const struct headerSprintfExtension_s * extensions,
		     errmsg_t * errmsg);

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
void headerCopyTags(Header headerFrom, Header headerTo, 
		    const rpmTag * tagstocopy);

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
HeaderIterator headerFreeIterator(HeaderIterator hi);

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
HeaderIterator headerInitIterator(Header h);

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval *tag		tag
 * @retval *type	tag value data type
 * @retval *p		pointer to tag value(s)
 * @retval *c		number of values
 * @return		1 on success, 0 on failure
 */
int headerNextIterator(HeaderIterator hi,
		rpmTag * tag,
		rpmTagType * type,
		rpm_data_t * p,
		rpm_count_t * c);



/** \ingroup header
 * Free data allocated when retrieved from header.
 * @deprecated Use headerFreeTag() instead.
 * @todo Remove from API.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or RPM_FORCEFREE_TYPE to force free)
 * @return		NULL always
 */
void * headerFreeData(rpm_data_t data, rpmTagType type);

/** \ingroup header
 * Return name, version, release strings from header.
 * @param h		header
 * @retval *np		name pointer (or NULL)
 * @retval *vp		version pointer (or NULL)
 * @retval *rp		release pointer (or NULL)
 * @return		0 always
 */
int headerNVR(Header h,
		const char ** np,
		const char ** vp,
		const char ** rp);

/** \ingroup header
 * Return name, epoch, version, release, arch strings from header.
 * @param h		header
 * @retval *np		name pointer (or NULL)
 * @retval *ep		epoch pointer (or NULL)
 * @retval *vp		version pointer (or NULL)
 * @retval *rp		release pointer (or NULL)
 * @retval *ap		arch pointer (or NULL)
 * @return		0 always
 */
int headerNEVRA(Header h,
		const char ** np,
		int32_t ** ep,
		const char ** vp,
		const char ** rp,
		const char ** ap);

/** \ingroup header
 * Return (malloc'd) header name-version-release string.
 * @param h		header
 * @retval np		name tag value
 * @return		name-version-release string
 */
char * headerGetNEVR(Header h, const char ** np );

/** \ingroup header
 * Return (malloc'd) header name-version-release.arch string.
 * @param h		header
 * @retval np		name tag value
 * @return		name-version-release string
 */
char * headerGetNEVRA(Header h, const char ** np );

/* \ingroup header
 * Return (malloc'd) header (epoch:)version-release string.
 * @param h		header
 * @retval np		name tag value (or NULL)
 * @return             (epoch:)version-release string
 */
char * headerGetEVR(Header h, const char **np);

/** \ingroup header
 * Return header color.
 * @param h		header
 * @return		header color
 */
rpm_color_t headerGetColor(Header h);

/** \ingroup header
 * Check if header is a source or binary package header
 * @param h		header
 * @return		0 == binary, 1 == source
 */
int headerIsSource(Header h);

/* ==================================================================== */
/** \name RPMTS */
/**
 * Prototype for headerFreeData() vector.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or  to force free)
 * @return		NULL always
 */
typedef
    void * (*HFD_t) (rpm_data_t data, rpmTagType type);

/**
 * Prototype for headerGetEntry() vector.
 *
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
typedef int (*HGE_t) (Header h, rpmTag tag,
			rpmTagType * type,
			rpm_data_t * p,
			rpm_count_t * c);

/**
 * Prototype for headerAddEntry() vector.
 *
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h             header
 * @param tag           tag
 * @param type          tag value data type
 * @param p             pointer to tag value(s)
 * @param c             number of values
 * @return              1 on success, 0 on failure
 */
typedef int (*HAE_t) (Header h, rpmTag tag, rpmTagType type,
			rpm_constdata_t p, rpm_count_t c);

/**
 * Prototype for headerModifyEntry() vector.
 * If there are multiple entries with this tag, the first one gets replaced.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef int (*HME_t) (Header h, rpmTag tag, rpmTagType type,
			rpm_constdata_t p, rpm_count_t c);

/**
 * Prototype for headerRemoveEntry() vector.
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
typedef int (*HRE_t) (Header h, rpmTag tag);

#ifdef __cplusplus
}
#endif

#endif	/* H_HEADER */
