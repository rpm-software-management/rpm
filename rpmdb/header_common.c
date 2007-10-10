/** \ingroup header
 * \file rpmdb/header_common.c
 */

#include "header.h"
#include "header_method.h"

/** \ingroup header
 * Header methods for rpm headers.
 */
extern struct HV_s * hdrVec;

/** \ingroup header
 */
static inline HV_t h2hv(Header h)
{
    return ((HV_t)h);
}

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
Header headerNew(void)
{
    return hdrVec->hdrnew();
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
Header headerFree( Header h)
{
    if (h == NULL) return NULL;
    return (h2hv(h)->hdrfree) (h);
}

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		new header reference
 */
Header headerLink(Header h)
{
    return (h2hv(h)->hdrlink) (h);
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		new header reference
 */
Header headerUnlink(Header h)
{
    if (h == NULL) return NULL;
    return (h2hv(h)->hdrunlink) (h);
}

/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
void headerSort(Header h)
{
/* FIX: add rc */
    (h2hv(h)->hdrsort) (h);
    return;
}

/** \ingroup header
 * Restore tags in header to original ordering.
 * @param h		header
 */
void headerUnsort(Header h)
{
/* FIX: add rc */
    (h2hv(h)->hdrunsort) (h);
    return;
}

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
unsigned int headerSizeof(Header h, enum hMagic magicp)
{
    if (h == NULL) return 0;
    return (h2hv(h)->hdrsizeof) (h, magicp);
}

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header blob (i.e. with offsets)
 */
void * headerUnload(Header h)
{
    return (h2hv(h)->hdrunload) (h);
}

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
Header headerReload(Header h, int tag)
{
    return (h2hv(h)->hdrreload) (h, tag);
}

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
Header headerCopy(Header h)
{
    return (h2hv(h)->hdrcopy) (h);
}

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
Header headerLoad(void * uh)
{
    return hdrVec->hdrload(uh);
}

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
Header headerCopyLoad(const void * uh)
{
    return hdrVec->hdrcopyload(uh);
}

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
Header headerRead(FD_t fd, enum hMagic magicp)
{
    return hdrVec->hdrread(fd, magicp);
}

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @param magicp	prefix write with 8 bytes of (magic, 0)?
 * @return		0 on success, 1 on error
 */
int headerWrite(FD_t fd, Header h, enum hMagic magicp)
{
    if (h == NULL) return 0;
    return (h2hv(h)->hdrwrite) (fd, h, magicp);
}

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
int headerIsEntry(Header h, int_32 tag)
{
    if (h == NULL) return 0;
    return (h2hv(h)->hdrisentry) (h, tag);
}

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		pointer to tag value(s)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
void * headerFreeTag(Header h,
		const void * data, rpmTagType type)
{
    return (h2hv(h)->hdrfreetag) (h, data, type);
}

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
int headerGetEntry(Header h, int_32 tag,
			hTYP_t type,
			void ** p,
			hCNT_t c)
{
    return (h2hv(h)->hdrget) (h, tag, type, p, c);
}

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
int headerGetEntryMinMemory(Header h, int_32 tag,
			hTYP_t type,
			hPTR_t * p, 
			hCNT_t c)
{
    return (h2hv(h)->hdrgetmin) (h, tag, type, p, c);
}

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
int headerAddEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c)
{
    return (h2hv(h)->hdradd) (h, tag, type, p, c);
}

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
int headerAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
{
    return (h2hv(h)->hdrappend) (h, tag, type, p, c);
}

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
int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
{
    return (h2hv(h)->hdraddorappend) (h, tag, type, p, c);
}

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
{
    return (h2hv(h)->hdraddi18n) (h, tag, string, lang);
}

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
int headerModifyEntry(Header h, int_32 tag, int_32 type,
			const void * p, int_32 c)
{
    return (h2hv(h)->hdrmodify) (h, tag, type, p, c);
}

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
{
    return (h2hv(h)->hdrremove) (h, tag);
}

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
		     errmsg_t * errmsg)
{
    return (h2hv(h)->hdrsprintf) (h, fmt, tbltags, extensions, errmsg);
}

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
void headerCopyTags(Header headerFrom, Header headerTo, hTAG_t tagstocopy)
{
/* FIX: add rc */
    hdrVec->hdrcopytags(headerFrom, headerTo, tagstocopy);
    return;
}

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
HeaderIterator headerFreeIterator(HeaderIterator hi)
{
    return hdrVec->hdrfreeiter(hi);
}

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
HeaderIterator headerInitIterator(Header h)
{
    return hdrVec->hdrinititer(h);
}

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
		hTAG_t tag,
		hTYP_t type,
		hPTR_t * p,
		hCNT_t c)
{
    return hdrVec->hdrnextiter(hi, tag, type, p, c);
}

