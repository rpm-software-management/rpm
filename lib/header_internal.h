#ifndef H_HEADER_INTERNAL
#define H_HEADER_INTERNAL

/** \ingroup header
 * \file lib/header_internal.h
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
typedef /*@abstract@*/ struct entryInfo * entryInfo;
struct entryInfo {
    int_32 tag;			/*!< Tag identifier. */
    int_32 type;		/*!< Tag data type. */
    int_32 offset;		/*!< Offset into data segment (ondisk only). */
    int_32 count;		/*!< Number of tag elements. */
};

#define	REGION_TAG_TYPE		RPM_BIN_TYPE
#define	REGION_TAG_COUNT	sizeof(struct entryInfo)

#define	ENTRY_IS_REGION(_e)     ((_e)->info.tag < HEADER_I18NTABLE)
#define	ENTRY_IN_REGION(_e)	((_e)->info.offset < 0)

/** \ingroup header
 * A single tag from a Header.
 */
typedef /*@abstract@*/ struct indexEntry * indexEntry;
struct indexEntry {
    struct entryInfo info;	/*!< Description of tag data. */
/*@owned@*/ void * data; 	/*!< Location of tag data. */
    int length;			/*!< No. bytes of data. */
    int rdlen;			/*!< No. bytes of data in region. */
};

/** \ingroup header
 * The Header data structure.
 */
struct headerToken {
/*@unused@*/ HV_t hv;		/*!< Header public methods. */
/*@owned@*/ indexEntry index;	/*!< Array of tags. */
    int indexUsed;		/*!< Current size of tag array. */
    int indexAlloced;		/*!< Allocated size of tag array. */
    int region_allocated;	/*!< Is 1st header region allocated? */
    int sorted; 		/*!< Are header entries sorted? */
    int legacy;			/*!< Header came from legacy source? */
/*@refs@*/ int nrefs;	/*!< Reference count. */
};

/** \ingroup header
 */
typedef /*@abstract@*/ struct sprintfTag * sprintfTag;
struct sprintfTag {
/*@null@*/ headerTagTagFunction ext;   /*!< if NULL tag element is invalid */
    int extNum;
    int_32 tag;
    int justOne;
    int arrayCount;
/*@kept@*/ char * format;
/*@kept@*/ /*@null@*/ char * type;
    int pad;
};

/** \ingroup header
 */
typedef /*@abstract@*/ struct extensionCache * extensionCache;
struct extensionCache {
    int_32 type;
    int_32 count;
    int avail;
    int freeit;
/*@owned@*/ const void * data;
};

/** \ingroup header
 */
/*@-fielduse@*/
typedef /*@abstract@*/ struct sprintfToken * sprintfToken;
struct sprintfToken {
    enum {
	PTOK_NONE = 0,
	PTOK_TAG,
	PTOK_ARRAY,
	PTOK_STRING,
	PTOK_COND
    } type;
    union {
	struct {
	/*@only@*/ sprintfToken format;
	    int numTokens;
	} array;
	struct sprintfTag tag;
	struct {
	/*@dependent@*/ char * string;
	    int len;
	} string;
	struct {
	/*@only@*/ /*@null@*/ sprintfToken ifFormat;
	    int numIfTokens;
	/*@only@*/ /*@null@*/ sprintfToken elseFormat;
	    int numElseTokens;
	    struct sprintfTag tag;
	} cond;
    } u;
};
/*@=fielduse@*/

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup header
 */
extern unsigned char header_magic[8];

/** \ingroup header
 */
extern HV_t hv;

/** \ingroup header
 * Swap int_32 and int_16 arrays within header region.
 *
 * This code is way more twisty than I would like.
 *
 * A bug with RPM_I18NSTRING_TYPE in rpm-2.5.x (fixed in August 1998)
 * causes the offset and length of elements in a header region to disagree
 * regarding the total length of the region data.
 *
 * The "fix" is to compute the size using both offset and length and
 * return the larger of the two numbers as the size of the region.
 * Kinda like computing left and right Riemann sums of the data elements
 * to determine the size of a data structure, go figger :-).
 *
 * There's one other twist if a header region tag is in the set to be swabbed,
 * as the data for a header region is located after all other tag data.
 *
 * @param entry		header entry
 * @param il		no. of entries
 * @param dl		start no. bytes of data
 * @param pe		header physical entry pointer (swapped)
 * @param dataStart	header data
 * @param regionid	region offset
 * @return		no. bytes of data in region, -1 on error
 */
int regionSwab(/*@null@*/ indexEntry entry, int il, int dl,
		entryInfo pe, char * dataStart, int regionid)
	/*@modifies entry, *dataStart @*/;

/** \ingroup header
 */
/*@only@*/ /*@null@*/ void * doHeaderUnload(Header h, /*@out@*/ int * lengthPtr)
	/*@modifies h, *lengthPtr @*/;

/** \ingroup header
 * Retrieve data from header entry.
 * @todo Permit retrieval of regions other than HEADER_IMUTABLE.
 * @param entry		header entry
 * @retval type		address of type (or NULL)
 * @retval p		address of data (or NULL)
 * @retval c		address of count (or NULL)
 * @param minMem	string pointers refer to header memory?
 * @return		1 on success, otherwise error.
 */
int copyEntry(const indexEntry entry,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c,
		int minMem)
	/*@modifies *type, *p, *c @*/;

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
 * Return header reference count.
 * @param h		header
 * @return		no. of references
 */
/*@unused@*/ static inline int headerUsageCount(Header h) /*@*/ {
    return h->nrefs;
}

/** \ingroup header
 * Dump a header in human readable format (for debugging).
 * @param h		header
 * @param flags		0 or HEADER_DUMP_LINLINE
 * @param tags		array of tag name/value pairs
 */
/*@unused@*/
void headerDump(Header h, FILE *f, int flags,
		const struct headerTagTableEntry * tags)
	/*@modifies f, fileSystem @*/;
#define HEADER_DUMP_INLINE   1

#ifdef __cplusplus
}   
#endif

#endif  /* H_HEADER_INTERNAL */
