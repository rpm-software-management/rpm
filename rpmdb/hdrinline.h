/*@-type@*/ /* FIX: cast to HV_t bogus */
#ifndef H_HDRINLINE
#define H_HDRINLINE

/** \ingroup header
 * \file lib/hdrinline.h
 */

#ifdef __cplusplus
extern "C" {
#endif
/*@+voidabstract -nullpass -mustmod -compdef -shadow -predboolothers @*/

/** \ingroup header
 * Header methods for rpm headers.
 */
/*@observer@*/ /*@unchecked@*/
extern struct HV_s * hdrVec;

/** \ingroup header
 */
/*@unused@*/ static inline HV_t h2hv(Header h)
	/*@*/
{
    /*@-abstract -castexpose -refcounttrans@*/
    return ((HV_t)h);
    /*@=abstract =castexpose =refcounttrans@*/
}

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
/*@unused@*/ static inline
Header headerNew(void)
	/*@*/
{
    return hdrVec->hdrnew();
}

/** \ingroup header
 * Dereference a header instance.
 * @todo Remove debugging entry from the ABI.
 * @param h		header
 * @param msg
 * @param fn
 * @param ln
 * @return		NULL always
 */
/*@unused@*/ static inline
/*@null@*/ Header XheaderFree( /*@killref@*/ /*@null@*/ Header h,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies h @*/
{
    /*@-abstract@*/
    if (h == NULL) return NULL;
    /*@=abstract@*/
    return (h2hv(h)->Xhdrfree) (h, msg, fn, ln);
}

/** \ingroup header
 * Reference a header instance.
 * @todo Remove debugging entry from the ABI.
 * @param h		header
 * @param msg
 * @param fn
 * @param ln
 * @return		new header reference
 */
/*@unused@*/ static inline
Header XheaderLink(Header h, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
	/*@modifies h @*/
{
    return (h2hv(h)->Xhdrlink) (h, msg, fn, ln);
}

/** \ingroup header
 * Dereference a header instance.
 * @todo Remove debugging entry from the ABI.
 * @param h		header
 * @param msg
 * @param fn
 * @param ln
 * @return		new header reference
 */
/*@unused@*/ static inline
Header XheaderUnlink(/*@killref@*/ /*@null@*/ Header h,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies h @*/
{
    /*@-abstract@*/
    if (h == NULL) return NULL;
    /*@=abstract@*/
    return (h2hv(h)->Xhdrunlink) (h, msg, fn, ln);
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
/*@-noeffectuncon@*/ /* FIX: add rc */
    (h2hv(h)->hdrsort) (h);
/*@=noeffectuncon@*/
    return;
}

/** \ingroup header
 * Restore tags in header to original ordering.
 * @param h		header
 */
/*@unused@*/ static inline
void headerUnsort(Header h)
	/*@modifies h @*/
{
/*@-noeffectuncon@*/ /* FIX: add rc */
    (h2hv(h)->hdrunsort) (h);
/*@=noeffectuncon@*/
    return;
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
    /*@-abstract@*/
    if (h == NULL) return 0;
    /*@=abstract@*/
    return (h2hv(h)->hdrsizeof) (h, magicp);
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
    return (h2hv(h)->hdrunload) (h);
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
    /*@-onlytrans@*/
    return (h2hv(h)->hdrreload) (h, tag);
    /*@=onlytrans@*/
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
    return (h2hv(h)->hdrcopy) (h);
}

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
/*@unused@*/ static inline
/*@null@*/ Header headerLoad(/*@kept@*/ void * uh)
	/*@modifies uh @*/
{
    return hdrVec->hdrload(uh);
}

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
/*@unused@*/ static inline
/*@null@*/ Header headerCopyLoad(const void * uh)
	/*@*/
{
    return hdrVec->hdrcopyload(uh);
}

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
/*@unused@*/ static inline
/*@null@*/ Header headerRead(FD_t fd, enum hMagic magicp)
	/*@modifies fd @*/
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
/*@unused@*/ static inline
int headerWrite(FD_t fd, /*@null@*/ Header h, enum hMagic magicp)
	/*@modifies fd, h @*/
{
    /*@-abstract@*/
    if (h == NULL) return 0;
    /*@=abstract@*/
    return (h2hv(h)->hdrwrite) (fd, h, magicp);
}

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerIsEntry(/*@null@*/ Header h, int_32 tag)
	/*@modifies h @*/
{
    /*@-abstract@*/
    if (h == NULL) return 0;
    /*@=abstract@*/
    return (h2hv(h)->hdrisentry) (h, tag);
}

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
/*@unused@*/ static inline
/*@null@*/ void * headerFreeTag(Header h,
		/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/
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
    return (h2hv(h)->hdrget) (h, tag, type, p, c);
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
/*@mayexit@*/
/*@unused@*/ static inline
int headerAddEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c)
	/*@modifies h @*/
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
/*@unused@*/ static inline
int headerAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
	/*@modifies h @*/
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
/*@unused@*/ static inline
int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
	/*@modifies h @*/
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
/*@unused@*/ static inline
int headerAddI18NString(Header h, int_32 tag, const char * string,
		const char * lang)
	/*@modifies h @*/
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
/*@unused@*/ static inline
int headerModifyEntry(Header h, int_32 tag, int_32 type,
			const void * p, int_32 c)
	/*@modifies h @*/
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
/*@unused@*/ static inline
int headerRemoveEntry(Header h, int_32 tag)
	/*@modifies h @*/
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
/*@unused@*/ static inline
/*@only@*/ char * headerSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tbltags,
		     const struct headerSprintfExtension_s * extensions,
		     /*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies *errmsg @*/
{
    return (h2hv(h)->hdrsprintf) (h, fmt, tbltags, extensions, errmsg);
}

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
/*@unused@*/ static inline
void headerCopyTags(Header headerFrom, Header headerTo, hTAG_t tagstocopy)
	/*@modifies headerFrom, headerTo @*/
{
/*@-noeffectuncon@*/ /* FIX: add rc */
    hdrVec->hdrcopytags(headerFrom, headerTo, tagstocopy);
/*@=noeffectuncon@*/
    return;
}

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
/*@unused@*/ static inline
HeaderIterator headerFreeIterator(/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/
{
    return hdrVec->hdrfreeiter(hi);
}

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
/*@unused@*/ static inline
HeaderIterator headerInitIterator(Header h)
	/*@modifies h */
{
    return hdrVec->hdrinititer(h);
}

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval tag		address of tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
/*@unused@*/ static inline
int headerNextIterator(HeaderIterator hi,
		/*@null@*/ /*@out@*/ hTAG_t tag,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies hi, *tag, *type, *p, *c @*/
{
    return hdrVec->hdrnextiter(hi, tag, type, p, c);
}

/*@=voidabstract =nullpass =mustmod =compdef =shadow =predboolothers @*/

#ifdef __cplusplus
}
#endif

#endif	/* H_HDRINLINE */
/*@=type@*/
