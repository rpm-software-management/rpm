#ifndef H_HEADER_INTERNAL
#define H_HEADER_INTERNAL

/** \ingroup header
 * \file rpmdb/header_internal.h
 */

#include <header.h>

#if !defined(__LCLINT__)
#include <netinet/in.h>
#endif  /* __LCLINT__ */

#define	INDEX_MALLOC_SIZE	8

/*
 * Teach header.c about legacy tags.
 */
#define	HEADER_OLDFILENAMES	1027
#define	HEADER_BASENAMES	1117

/** \ingroup header
 * Description of tag data.
 */
typedef /*@abstract@*/ struct entryInfo_s * entryInfo;
struct entryInfo_s {
    int_32 tag;			/*!< Tag identifier. */
    int_32 type;		/*!< Tag data type. */
    int_32 offset;		/*!< Offset into data segment (ondisk only). */
    int_32 count;		/*!< Number of tag elements. */
};

#define	REGION_TAG_TYPE		RPM_BIN_TYPE
#define	REGION_TAG_COUNT	sizeof(struct entryInfo_s)

#define	ENTRY_IS_REGION(_e) \
	(((_e)->info.tag >= HEADER_IMAGE) && ((_e)->info.tag < HEADER_REGIONS))
#define	ENTRY_IN_REGION(_e)	((_e)->info.offset < 0)

/** \ingroup header
 * A single tag from a Header.
 */
typedef /*@abstract@*/ struct indexEntry_s * indexEntry;
struct indexEntry_s {
    struct entryInfo_s info;	/*!< Description of tag data. */
/*@owned@*/
    void * data; 		/*!< Location of tag data. */
    int length;			/*!< No. bytes of data. */
    int rdlen;			/*!< No. bytes of data in region. */
};

/** \ingroup header
 * The Header data structure.
 */
struct headerToken_s {
/*@unused@*/
    struct HV_s hv;		/*!< Header public methods. */
/*@only@*/ /*@null@*/
    void * blob;		/*!< Header region blob. */
/*@owned@*/
    indexEntry index;		/*!< Array of tags. */
    int indexUsed;		/*!< Current size of tag array. */
    int indexAlloced;		/*!< Allocated size of tag array. */
    int flags;
#define	HEADERFLAG_SORTED	(1 << 0) /*!< Are header entries sorted? */
#define	HEADERFLAG_ALLOCATED	(1 << 1) /*!< Is 1st header region allocated? */
#define	HEADERFLAG_LEGACY	(1 << 2) /*!< Header came from legacy source? */
#define HEADERFLAG_DEBUG	(1 << 3) /*!< Debug this header? */
/*@refs@*/
    int nrefs;			/*!< Reference count. */
};

/** \ingroup header
 */
typedef /*@abstract@*/ struct sprintfTag_s * sprintfTag;
struct sprintfTag_s {
/*@null@*/
    headerTagTagFunction ext;   /*!< NULL if tag element is invalid */
    int extNum;
    int_32 tag;
    int justOne;
    int arrayCount;
/*@kept@*/
    char * format;
/*@kept@*/ /*@null@*/
    char * type;
    int pad;
};

/** \ingroup header
 * Extension cache.
 */
typedef /*@abstract@*/ struct rpmec_s * rpmec;
struct rpmec_s {
    int_32 type;
    int_32 count;
    int avail;
    int freeit;
/*@owned@*/
    const void * data;
};

/** \ingroup header
 */
typedef /*@abstract@*/ struct sprintfToken_s * sprintfToken;
/*@-fielduse@*/
struct sprintfToken_s {
    enum {
	PTOK_NONE = 0,
	PTOK_TAG,
	PTOK_ARRAY,
	PTOK_STRING,
	PTOK_COND
    } type;
    union {
	struct sprintfTag_s tag;	/*!< PTOK_TAG */
	struct {
	/*@only@*/
	    sprintfToken format;
	    int numTokens;
	} array;			/*!< PTOK_ARRAY */
	struct {
	/*@dependent@*/
	    char * string;
	    int len;
	} string;			/*!< PTOK_STRING */
	struct {
	/*@only@*/ /*@null@*/
	    sprintfToken ifFormat;
	    int numIfTokens;
	/*@only@*/ /*@null@*/
	    sprintfToken elseFormat;
	    int numElseTokens;
	    struct sprintfTag_s tag;
	} cond;				/*!< PTOK_COND */
    } u;
};
/*@=fielduse@*/

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup header
 * Return array of locales found in header.
 * The array is terminated with a NULL sentinel.
 * @param h		header
 * @return		array of locales (or NULL on error)
 */
/*@unused@*/
/*@only@*/ /*@null@*/ char ** headerGetLangs(Header h)
	/*@*/;

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
/*@-incondefs@*/
int headerGetRawEntry(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hPTR_t * p, 
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
	/*@requires maxSet(type) >= 0 /\ maxSet(p) >= 0 /\ maxSet(c) >= 0 @*/;
/*@=incondefs@*/
/*@=exportlocal@*/

/** \ingroup header
 * Return header reference count.
 * @param h		header
 * @return		no. of references
 */
/*@-type@*/ /* FIX: cast? */
/*@unused@*/ static inline int headerUsageCount(Header h) /*@*/ {
    return h->nrefs;
}
/*@=type@*/

/** \ingroup header
 * Dump a header in human readable format (for debugging).
 * @param h		header
 * @param f		file handle
 * @param flags		0 or HEADER_DUMP_INLINE
 * @param tags		array of tag name/value pairs
 */
/*@unused@*/
void headerDump(Header h, FILE *f, int flags,
		const struct headerTagTableEntry_s * tags)
	/*@globals fileSystem @*/
	/*@modifies f, fileSystem @*/;
#define HEADER_DUMP_INLINE   1

#ifdef __cplusplus
}   
#endif

#endif  /* H_HEADER_INTERNAL */
