#ifndef H_HEADER_INTERNAL
#define H_HEADER_INTERNAL

/** \ingroup header
 * \file lib/header_internal.h
 */

#include <header.h>

#if !defined(__LCLINT__)
#include <netinet/in.h>
#endif  /* __LCLINT__ */


/**
 * Description of tag data.
 */
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

/**
 * A single tag from a Header.
 */
struct indexEntry {
    struct entryInfo info;	/*!< Description of tag data. */
/*@owned@*/ void * data; 	/*!< Location of tag data. */
    int length;			/*!< No. bytes of data. */
    int rdlen;			/*!< No. bytes of data in region. */
};

/**
 * The Header data structure.
 */
struct headerToken {
/*@owned@*/ struct indexEntry * index;	/*!< Array of tags. */
    int indexUsed;		/*!< Current size of tag array. */
    int indexAlloced;		/*!< Allocated size of tag array. */
    int region_allocated;	/*!< Is 1st header region allocated? */
    int sorted; 		/*!< Are header entries sorted? */
    int legacy;			/*!< Header came from legacy source? */
/*@refs@*/ int nrefs;	/*!< Reference count. */
};

/**
 */
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

/**
 */
struct extensionCache {
    int_32 type;
    int_32 count;
    int avail;
    int freeit;
/*@owned@*/ const void * data;
};

/**
 */
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
	    /*@only@*/ struct sprintfToken * format;
	    int numTokens;
	} array;
	struct sprintfTag tag;
	struct {
	    /*@dependent@*/ char * string;
	    int len;
	} string;
	struct {
	/*@only@*/ /*@null@*/ struct sprintfToken * ifFormat;
	    int numIfTokens;
	/*@only@*/ /*@null@*/ struct sprintfToken * elseFormat;
	    int numElseTokens;
	    struct sprintfTag tag;
	} cond;
    } u;
};

#ifdef __cplusplus
extern "C" {
#endif


/** \ingroup header
 * Dump a header in human readable format (for debugging).
 * @param h		header
 * @param flags		0 or HEADER_DUMP_LINLINE
 * @param tags		array of tag name/value pairs
 */
/*@unused@*/
void headerDump(Header h, FILE *f, int flags,
		const struct headerTagTableEntry * tags);
#define HEADER_DUMP_INLINE   1

#ifdef __cplusplus
}   
#endif

#endif  /* H_HEADER_INTERNAL */
