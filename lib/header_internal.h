#ifndef H_HEADER_INTERNAL
#define H_HEADER_INTERNAL

/** \ingroup header
 * \file lib/header_internal.h
 */

#include <netinet/in.h>

#include <rpm/header.h>

#define	INDEX_MALLOC_SIZE	8

/** \ingroup header
 * Description of tag data.
 */
typedef struct entryInfo_s * entryInfo;
struct entryInfo_s {
    rpmTag tag;			/*!< Tag identifier. */
    rpmTagType type;		/*!< Tag data type. */
    int32_t offset;		/*!< Offset into data segment (ondisk only). */
    rpm_count_t count;		/*!< Number of tag elements. */
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
    rpm_data_t data; 		/*!< Location of tag data. */
    int length;			/*!< No. bytes of data. */
    int rdlen;			/*!< No. bytes of data in region. */
};

typedef enum headerFlags_e {
    HEADERFLAG_SORTED    = (1 << 0), /*!< Are header entries sorted? */
    HEADERFLAG_ALLOCATED = (1 << 1), /*!< Is 1st header region allocated? */
    HEADERFLAG_LEGACY    = (1 << 2), /*!< Header came from legacy source? */
    HEADERFLAG_DEBUG     = (1 << 3), /*!< Debug this header? */
} headerFlags;

/** \ingroup header
 * The Header data structure.
 */
struct headerToken_s {
    void * blob;		/*!< Header region blob. */
    indexEntry index;		/*!< Array of tags. */
    int indexUsed;		/*!< Current size of tag array. */
    int indexAlloced;		/*!< Allocated size of tag array. */
    unsigned int instance;	/*!< Rpmdb instance (offset) */
    headerFlags flags;
    int nrefs;			/*!< Reference count. */
};

/**
 * Sanity check on no. of tags.
 * This check imposes a limit of 65K tags, more than enough.
 */
#define hdrchkTags(_ntags)      ((_ntags) & 0xffff0000)

/**
 * Sanity check on type values.
 */
#define hdrchkType(_type) ((_type) < RPM_MIN_TYPE || (_type) > RPM_MAX_TYPE)

/**
 * Sanity check on data size and/or offset and/or count.
 * This check imposes a limit of 16 MB, more than enough.
 */
#define hdrchkData(_nbytes) ((_nbytes) & 0xff000000)

/**
 * Sanity check on data alignment for data type.
 */
#define hdrchkAlign(_type, _off)	((_off) & (typeAlign[_type]-1))

/**
 * Sanity check on range of data offset.
 */
#define hdrchkRange(_dl, _off)		((_off) < 0 || (_off) > (_dl))

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup header
 * Conver a 64bit value to network byte order.
 * @param n		a number
 * @return		number in network byte order
 */
RPM_GNUC_INTERNAL
uint64_t htonll( uint64_t n );

/** \ingroup header
 * Set header instance (rpmdb record number)
 * @param h		header
 * @param instance	record number
 */
RPM_GNUC_INTERNAL
void headerSetInstance(Header h, unsigned int instance);

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
int headerGetRawEntry(Header h, rpmTag tag,
			rpmTagType * type,
			rpm_data_t * p, 
			rpm_count_t * c);

#ifdef __cplusplus
}   
#endif

#endif  /* H_HEADER_INTERNAL */
