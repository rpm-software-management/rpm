#ifndef _RPMLEGACY_H
#define _RPMLEGACY_H

#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>

/* ==================================================================== */
/*         LEGACY INTERFACES AND TYPES, DO NOT USE IN NEW CODE!         */
/* ==================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _RPM_4_4_COMPAT

/* mappings for legacy types */
typedef int32_t		int_32 RPM_GNUC_DEPRECATED;
typedef int16_t		int_16 RPM_GNUC_DEPRECATED;
typedef int8_t		int_8 RPM_GNUC_DEPRECATED;
typedef uint32_t	uint_32 RPM_GNUC_DEPRECATED;
typedef uint16_t	uint_16 RPM_GNUC_DEPRECATED;
typedef uint8_t		uint_8 RPM_GNUC_DEPRECATED;

typedef rpm_tag_t *	hTAG_t RPM_GNUC_DEPRECATED;
typedef rpm_tagtype_t *	hTYP_t RPM_GNUC_DEPRECATED;
typedef const void *	hPTR_t RPM_GNUC_DEPRECATED;
typedef rpm_count_t *	hCNT_t RPM_GNUC_DEPRECATED;

typedef rpmSpec		Spec RPM_GNUC_DEPRECATED;

/* legacy header interfaces */

/** \ingroup header_legacy
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 * @deprecated		Use headerGet() instead
 *
 * @param h		header
 * @param tag		tag
 * @retval *type	tag value data type (or NULL)
 * @retval *p		pointer to tag value(s) (or NULL)
 * @retval *c		number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
int headerGetEntry(Header h, rpm_tag_t tag,
			rpm_tagtype_t * type,
			rpm_data_t * p,
			rpm_count_t * c) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 * @deprecated		Use headerGet() instead
 *
 * @param h		header
 * @param tag		tag
 * @retval *type	tag value data type (or NULL)
 * @retval *p		pointer to tag value(s) (or NULL)
 * @retval *c		number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
int headerGetEntryMinMemory(Header h, rpm_tag_t tag,
			rpm_tagtype_t * type,
			rpm_data_t * p, 
			rpm_count_t * c) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
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
int headerAddEntry(Header h, rpm_tag_t tag, rpm_tagtype_t type, 
		   rpm_constdata_t p, rpm_count_t c) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
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
int headerAppendEntry(Header h, rpm_tag_t tag, rpm_tagtype_t type,
		rpm_constdata_t p, rpm_count_t c) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
 * Add or append element to tag array in header.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
int headerAddOrAppendEntry(Header h, rpm_tag_t tag, rpm_tagtype_t type,
		rpm_constdata_t p, rpm_count_t c) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
 * Modify tag in header.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @deprecated		Use headerMod() instead
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
int headerModifyEntry(Header h, rpm_tag_t tag, rpm_tagtype_t type,
			rpm_constdata_t p, rpm_count_t c) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 * @deprecated		Use headerDel() instead
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
int headerRemoveEntry(Header h, rpm_tag_t tag) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 * @deprecated		Use headerFormat() instead
 *
 * @param _h		header
 * @param _fmt		format to use
 * @param _tbltags	array of tag name/value pairs (unused)
 * @param _exts		chained table of formatting extensions. (unused)
 * @retval _emsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
#define headerSprintf(_h, _fmt, _tbltags, _exts, _emsg) \
	headerFormat((_h), (_fmt), (_emsg))

/** \ingroup header_legacy
 * Return next tag from header.
 * @deprecated Use headerNext() instead.
 *
 * @param hi		header tag iterator
 * @retval *tag		tag
 * @retval *type	tag value data type
 * @retval *p		pointer to tag value(s)
 * @retval *c		number of values
 * @return		1 on success, 0 on failure
 */
int headerNextIterator(HeaderIterator hi,
		rpm_tag_t * tag,
		rpm_tagtype_t * type,
		rpm_data_t * p,
		rpm_count_t * c) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
 * Free data allocated when retrieved from header.
 * @deprecated		Use rpmtdFreeData() instead
 *
 * @param h		header
 * @param data		pointer to tag value(s)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
void * headerFreeTag(Header h, rpm_data_t data, rpm_tagtype_t type) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
 * Free data allocated when retrieved from header.
 * @deprecated Use rpmtdFreeData() instead.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or RPM_FORCEFREE_TYPE to force free)
 * @return		NULL always
 */
void * headerFreeData(rpm_data_t data, rpm_tagtype_t type) RPM_GNUC_DEPRECATED;

/** \ingroup header_legacy
 * Prototypes for headerGetEntry(), headerFreeData() etc vectors.
 * @{
 */
typedef void * (*HFD_t) (rpm_data_t data, rpm_tagtype_t type) RPM_GNUC_DEPRECATED;
typedef int (*HGE_t) (Header h, rpm_tag_t tag, rpm_tagtype_t * type,
			rpm_data_t * p, rpm_count_t * c) RPM_GNUC_DEPRECATED;
typedef int (*HAE_t) (Header h, rpm_tag_t tag, rpm_tagtype_t type,
			rpm_constdata_t p, rpm_count_t c) RPM_GNUC_DEPRECATED;
typedef int (*HME_t) (Header h, rpm_tag_t tag, rpm_tagtype_t type,
			rpm_constdata_t p, rpm_count_t c) RPM_GNUC_DEPRECATED;
typedef int (*HRE_t) (Header h, rpm_tag_t tag) RPM_GNUC_DEPRECATED;
/** @} */

/* other misc renamed / namespaced functions */
/* TODO: arrange deprecation warnings on these too... */
#define isCompressed	rpmFileIsCompressed
#define makeTempFile	rpmMkTempFile
#define whatis		rpmfiWhatis
#define	tagName		rpmTagGetName
#define tagType		rpmTagGetType
#define tagValue	rpmTagGetValue

#define xislower	rislower
#define xisupper	risupper
#define xisalpha	risalpha
#define xisdigit	risdigit
#define xisalnum	risalnum
#define xisblank	risblank
#define xisspace	risspace
#define xtolower	rtolower
#define xtoupper	rtoupper
#define xstrcasecmp	rstrcasecmp
#define xstrncasecmp	rstrncasecmp

#define rpmMessage	rpmlog
#define rpmError	rpmlog

#endif /* _RPM_4_4_COMPAT */

#ifdef __cplusplus
}
#endif

#endif /* _RPMLEGACY_H */
