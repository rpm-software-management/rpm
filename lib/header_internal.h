#ifndef H_HEADER_INTERNAL
#define H_HEADER_INTERNAL

/** \ingroup header
 * \file lib/header_internal.h
 */

#include <rpm/header.h>
#include <netinet/in.h>

/** \ingroup header
 * Description of tag data.
 */
typedef struct entryInfo_s * entryInfo;
struct entryInfo_s {
    rpm_tag_t tag;		/*!< Tag identifier. */
    rpm_tagtype_t type;		/*!< Tag data type. */
    int32_t offset;		/*!< Offset into data segment (ondisk only). */
    rpm_count_t count;		/*!< Number of tag elements. */
};

#define	REGION_TAG_TYPE		RPM_BIN_TYPE
#define	REGION_TAG_COUNT	sizeof(struct entryInfo_s)

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
 * This check imposes a limit of 256 MB -- file signatures
 * may require a lot of space in the header.
 */
#define HEADER_DATA_MAX 0x0fffffff
#define hdrchkData(_nbytes) ((_nbytes) & (~HEADER_DATA_MAX))

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

/* convert entry info to host endianess */
static inline void ei2h(const struct entryInfo_s *pe, struct entryInfo_s *info)
{
    info->tag = ntohl(pe->tag);
    info->type = ntohl(pe->type);
    info->offset = ntohl(pe->offset);
    info->count = ntohl(pe->count);
}

RPM_GNUC_INTERNAL
rpmRC headerVerifyRegion(rpmTagVal regionTag,
                        int il, int dl, entryInfo pe, unsigned char *dataStart,
                        int exact_size, int *ril, int *rdl, char **buf);


/** \ingroup header
 * Perform simple sanity and range checks on header tag(s).
 * @param il		no. of tags in header
 * @param dl		no. of bytes in header data.
 * @param pe		1st element in tag array, big-endian
 * @param dataStart	start of data area
 * @retvar emsg		possible error message (malloced) or NULL to disable
 * @return		0 on success, otherwise ordinal of failed tag
 */
RPM_GNUC_INTERNAL
int headerVerifyInfo(int il, int dl,
		     const struct entryInfo_s * pe, const void *dataStart,
		     char **emsg);


/** \ingroup header
 * Set header instance (rpmdb record number)
 * @param h		header
 * @param instance	record number
 */
RPM_GNUC_INTERNAL
void headerSetInstance(Header h, unsigned int instance);

/* Package IO helper to consolidate partial read and error handling */
RPM_GNUC_INTERNAL
ssize_t Freadall(FD_t fd, void * buf, ssize_t size);
#ifdef __cplusplus
}   
#endif

#endif  /* H_HEADER_INTERNAL */
