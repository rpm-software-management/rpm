#ifndef H_HEADER_INTERNAL
#define H_HEADER_INTERNAL

/** \ingroup header
 * \file rpmdb/header_internal.h
 */

#include <header.h>
#include "header_method.h"

#include <netinet/in.h>

#define	INDEX_MALLOC_SIZE	8

/*
 * Teach header.c about legacy tags.
 */
#define	HEADER_OLDFILENAMES	1027
#define	HEADER_BASENAMES	1117

/** \ingroup header
 * Description of tag data.
 */
typedef struct entryInfo_s * entryInfo;
struct entryInfo_s {
    int32_t tag;			/*!< Tag identifier. */
    int32_t type;		/*!< Tag data type. */
    int32_t offset;		/*!< Offset into data segment (ondisk only). */
    int32_t count;		/*!< Number of tag elements. */
};

#define	REGION_TAG_TYPE		RPM_BIN_TYPE
#define	REGION_TAG_COUNT	sizeof(struct entryInfo_s)

#define	ENTRY_IS_REGION(_e) \
	(((_e)->info.tag >= HEADER_IMAGE) && ((_e)->info.tag < HEADER_REGIONS))
#define	ENTRY_IN_REGION(_e)	((_e)->info.offset < 0)

/** \ingroup header
 * A single tag from a Header.
 */
typedef struct indexEntry_s * indexEntry;
struct indexEntry_s {
    struct entryInfo_s info;	/*!< Description of tag data. */
    void * data; 		/*!< Location of tag data. */
    int length;			/*!< No. bytes of data. */
    int rdlen;			/*!< No. bytes of data in region. */
};

/** \ingroup header
 * The Header data structure.
 */
struct headerToken_s {
    struct HV_s hv;		/*!< Header public methods. */
    void * blob;		/*!< Header region blob. */
    indexEntry index;		/*!< Array of tags. */
    int indexUsed;		/*!< Current size of tag array. */
    int indexAlloced;		/*!< Allocated size of tag array. */
    int flags;
#define	HEADERFLAG_SORTED	(1 << 0) /*!< Are header entries sorted? */
#define	HEADERFLAG_ALLOCATED	(1 << 1) /*!< Is 1st header region allocated? */
#define	HEADERFLAG_LEGACY	(1 << 2) /*!< Header came from legacy source? */
#define HEADERFLAG_DEBUG	(1 << 3) /*!< Debug this header? */
    int nrefs;			/*!< Reference count. */
};

/** \ingroup header
 */
typedef struct sprintfTag_s * sprintfTag;
struct sprintfTag_s {
    headerTagFormatFunction fmt;
    headerTagTagFunction ext;   /*!< NULL if tag element is invalid */
    int extNum;
    int32_t tag;
    int justOne;
    int arrayCount;
    char * format;
    char * type;
    int pad;
};

/** \ingroup header
 * Extension cache.
 */
typedef struct rpmec_s * rpmec;
struct rpmec_s {
    int32_t type;
    int32_t count;
    int avail;
    int freeit;
    const void * data;
};

/** \ingroup header
 */
typedef struct sprintfToken_s * sprintfToken;
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
	    sprintfToken format;
	    int i;
	    int numTokens;
	} array;			/*!< PTOK_ARRAY */
	struct {
	    char * string;
	    int len;
	} string;			/*!< PTOK_STRING */
	struct {
	    sprintfToken ifFormat;
	    int numIfTokens;
	    sprintfToken elseFormat;
	    int numElseTokens;
	    struct sprintfTag_s tag;
	} cond;				/*!< PTOK_COND */
    } u;
};

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup header
 * Return array of locales found in header.
 * The array is terminated with a NULL sentinel.
 * @param h		header
 * @return		array of locales (or NULL on error)
 */
char ** headerGetLangs(Header h);

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
int headerGetRawEntry(Header h, int32_t tag,
			hTYP_t type,
			hPTR_t * p, 
			hCNT_t c);

/** \ingroup header
 * Return header reference count.
 * @param h		header
 * @return		no. of references
 */
/* FIX: cast? */
static inline int headerUsageCount(Header h)  {
    return h->nrefs;
}

/** \ingroup header
 * Dump a header in human readable format (for debugging).
 * @param h		header
 * @param f		file handle
 * @param flags		0 or HEADER_DUMP_INLINE
 * @param tags		array of tag name/value pairs
 */
void headerDump(Header h, FILE *f, int flags,
		const struct headerTagTableEntry_s * tags);
#define HEADER_DUMP_INLINE   1

/** \ingroup header
 * Convert binary blob to printable hex string 
 * @param[in] data		binary data
 * @param[in] count		size of data in bytes
 */
char * bin2hex(const char *data, size_t count);

#ifdef __cplusplus
}   
#endif

#endif  /* H_HEADER_INTERNAL */
