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

typedef struct hdrblob_s * hdrblob;
struct hdrblob_s {
    int32_t *ei;
    size_t uc;
    int32_t il;
    int32_t dl;
    entryInfo pe;
    int32_t pvlen;
    uint8_t *dataStart;
    uint8_t *dataEnd;

    rpmTagVal regionTag;
    int32_t ril;
    int32_t rdl;
};

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
rpmRC hdrblobInit(const void *uh, size_t uc,
		rpmTagVal regionTag, int exact_size,
		struct hdrblob_s *blob, char **emsg);

RPM_GNUC_INTERNAL
rpmRC hdrblobRead(FD_t fd, int magic, int exact_size, rpmTagVal regionTag, hdrblob blob, char **emsg);

RPM_GNUC_INTERNAL
rpmRC hdrblobImport(hdrblob blob, int fast, Header *hdrp, char **emsg);

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
