#ifndef H_HDRINLINE
#define H_HDRINLINE

/** \ingroup header
 * \file lib/hdrinline.h
 */

/*@-exportlocal@*/
extern int _hdrinline_debug;
/*@=exportlocal@*/

#ifdef __cplusplus
extern "C" {
#endif
/*@+voidabstract -nullpass -abstract -mustmod -compdef -shadow -predboolothers @*/

#define	HV(_h)	((HV_t *)(_h))

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
/*@unused@*/ static inline
/*@null@*/ Header headerFree( /*@null@*/ /*@killref@*/ Header h)
	/*@modifies h @*/
{
    if (h == NULL) return NULL;
    return (HV(h)->free) (h);
}

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
/*@unused@*/ static inline
Header headerLink(Header h)
	/*@modifies h @*/
{
    return (HV(h)->link) (h);
}

/*@-exportlocal@*/
/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
/*@unused@*/ static inline
void headerSort(Header h)
	/*@modifies h @*/
{
    return (HV(h)->sort) (h);
}

/** \ingroup header
 * Restore tags in header to original ordering.
 * @param h		header
 */
/*@unused@*/ static inline
void headerUnsort(Header h)
	/*@modifies h @*/
{
    return (HV(h)->unsort) (h);
}
/*@=exportlocal@*/

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
/*@unused@*/ static inline
unsigned int headerSizeof(/*@null@*/ Header h, enum hMagic magicp)
	/*@modifies h @*/
{
    if (h == NULL) return 0;
    return (HV(h)->size) (h, magicp);
}

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header blob (i.e. with offsets)
 */
/*@unused@*/ static inline
/*@only@*/ /*@null@*/ void * headerUnload(Header h)
	/*@modifies h @*/
{
    return (HV(h)->unload) (h);
}

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
/*@unused@*/ static inline
/*@null@*/ Header headerReload(/*@only@*/ Header h, int tag)
	/*@modifies h @*/
{
    return (HV(h)->reload) (h, tag);
}

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
/*@unused@*/ static inline
/*@null@*/ Header headerCopy(Header h)
	/*@modifies h @*/
{
    return (HV(h)->copy) (h);
}

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerIsEntry(/*@null@*/Header h, int_32 tag)
	/*@modifies h @*/
{
    if (h == NULL) return 0;
    return (HV(h)->isentry) (h, tag);
}

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
/*@unused@*/ static inline
int headerGetEntry(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ void ** p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
{
    return (HV(h)->get) (h, tag, type, p, c);
}

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
/*@unused@*/ static inline
int headerGetEntryMinMemory(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hPTR_t * p, 
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
{
    return (HV(h)->getmin) (h, tag, type, p, c);
}

/** \ingroup header
 * Retrieve tag value with type match.
 * If *type is RPM_NULL_TYPE any type will match, otherwise only *type will
 * match.
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
/*@-exportlocal@*/
/*@unused@*/ static inline
int headerGetRawEntry(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hPTR_t * p, 
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
{
    return (HV(h)->getraw) (h, tag, type, p, c);
}
/*@=exportlocal@*/

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
/*@unused@*/ static inline
int headerAddEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c)
	/*@modifies h @*/
{
    return (HV(h)->add) (h, tag, type, p, c);
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
/*@unused@*/ static inline
int headerAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
	/*@modifies h @*/
{
    return (HV(h)->append) (h, tag, type, p, c);
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
/*@unused@*/ static inline
int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
	/*@modifies h @*/
{
    return (HV(h)->addorappend) (h, tag, type, p, c);
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
/*@unused@*/ static inline
int headerAddI18NString(Header h, int_32 tag, const char * string,
		const char * lang)
	/*@modifies h @*/
{
    return (HV(h)->addi18n) (h, tag, string, lang);
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
/*@unused@*/ static inline
int headerModifyEntry(Header h, int_32 tag, int_32 type,
			const void * p, int_32 c)
	/*@modifies h @*/
{
    return (HV(h)->modify) (h, tag, type, p, c);
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
/*@unused@*/ static inline
int headerRemoveEntry(Header h, int_32 tag)
	/*@modifies h @*/
{
    return (HV(h)->remove) (h, tag);
}

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
/*@unused@*/ static inline
/*@only@*/ char * headerSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry * tags,
		     const struct headerSprintfExtension * extensions,
		     /*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies *errmsg @*/
{
    return (HV(h)->sprintf) (h, fmt, tags, extensions, errmsg);
}
/*@=voidabstract =nullpass =abstract =mustmod =compdef =shadow =predboolothers @*/

#ifdef __cplusplus
}
#endif

#endif	/* H_HDRINLINE */
