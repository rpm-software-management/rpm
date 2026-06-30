#ifndef H_HEADER_INTERNAL
#define H_HEADER_INTERNAL

/** \ingroup header
 * \file header_internal.hh
 */

#include <rpm/header.h>
#include <rpm/rpmcrypto.h>

typedef struct entryInfo_s * entryInfo;
typedef struct hdrblob_s * hdrblob;
struct hdrblob_s {
    uint32_t *ei;
    uint32_t il;
    uint32_t dl;
    entryInfo pe;
    uint32_t pvlen;
    uint8_t *dataStart;
    uint8_t *dataEnd;

    rpmTagVal regionTag;
    uint32_t ril;
    uint32_t rdl;
};

hdrblob hdrblobCreate(void);

hdrblob hdrblobFree(hdrblob blob);

rpmRC hdrblobInit(const void *uh, size_t uc,
		rpmTagVal regionTag, int exact_size,
		struct hdrblob_s *blob, char **emsg);

rpmRC hdrblobRead(FD_t fd, int magic, int exact_size, rpmTagVal regionTag, hdrblob blob, char **emsg);

rpmRC hdrblobImport(hdrblob blob, int fast, Header *hdrp, char **emsg);

int hdrblobIsEntry(hdrblob blob, uint32_t tag);

rpmRC hdrblobGet(hdrblob blob, uint32_t tag, rpmtd td);

void hdrblobDigestUpdate(rpmDigestBundle bundle, struct hdrblob_s *blob);

/** \ingroup header
 * Set header instance (rpmdb record number)
 * @param h		header
 * @param instance	record number
 */
void headerSetInstance(Header h, unsigned int instance);

/* Package IO helper to consolidate partial read and error handling */
ssize_t Freadall(FD_t fd, void * buf, ssize_t size);

int headerIsSourceHeuristic(Header h);

#endif  /* H_HEADER_INTERNAL */
