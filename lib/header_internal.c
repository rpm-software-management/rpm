/** \ingroup header
 * \file lib/header_internal.c
 */

#include "system.h"

#include <rpm/rpmtypes.h>
#include "lib/header_internal.h"

#include "debug.h"

uint64_t htonll( uint64_t n ) {
    uint32_t *i = (uint32_t*)&n;
    uint32_t b = i[0];
    i[0] = htonl(i[1]);
    i[1] = htonl(b);
    return n;
}

/*
 * Backwards compatibility wrappers for legacy interfaces.
 * Remove these some day...
 */
#define _RPM_4_4_COMPAT
#include <rpm/rpmlegacy.h>

/* dumb macro to avoid 50 copies of this code while converting... */
#define TDWRAP() \
    if (type) \
	*type = td.type; \
    if (p) \
	*p = td.data; \
    else \
	rpmtdFreeData(&td); \
    if (c) \
	*c = td.count

int headerRemoveEntry(Header h, rpmTag tag)
{
    return headerDel(h, tag);
}

static void *_headerFreeData(rpm_data_t data, rpmTagType type)
{
    if (data) {
	if (type == RPM_FORCEFREE_TYPE ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		free(data);
    }
    return NULL;
}

void * headerFreeData(rpm_data_t data, rpmTagType type)
{
    return _headerFreeData(data, type);
}

void * headerFreeTag(Header h, rpm_data_t data, rpmTagType type)
{
    return _headerFreeData(data, type);
}

static int headerGetWrap(Header h, rpmTag tag,
		rpmTagType * type,
		rpm_data_t * p,
		rpm_count_t * c,
		headerGetFlags flags)
{
    struct rpmtd_s td;
    int rc;

    rc = headerGet(h, tag, &td, flags);
    TDWRAP();
    return rc;
}

int headerGetEntry(Header h, rpmTag tag,
			rpmTagType * type,
			rpm_data_t * p,
			rpm_count_t * c)
{
    return headerGetWrap(h, tag, type, p, c, HEADERGET_DEFAULT);
}

int headerGetEntryMinMemory(Header h, rpmTag tag,
			rpmTagType * type,
			rpm_data_t * p,
			rpm_count_t * c)
{
    return headerGetWrap(h, tag, type, (rpm_data_t) p, c, HEADERGET_MINMEM);
}

int headerGetRawEntry(Header h, rpmTag tag, rpmTagType * type, rpm_data_t * p,
		rpm_count_t * c)
{
    if (p == NULL) 
	return headerIsEntry(h, tag);

    return headerGetWrap(h, tag, type, p, c, HEADERGET_RAW);
}

int headerNextIterator(HeaderIterator hi,
		rpmTag * tag,
		rpmTagType * type,
		rpm_data_t * p,
		rpm_count_t * c)
{
    struct rpmtd_s td;
    int rc;

    rc = headerNext(hi, &td);
    if (tag)
	*tag = td.tag;
    TDWRAP();
    return rc;
}

int headerModifyEntry(Header h, rpmTag tag, rpmTagType type,
			rpm_constdata_t p, rpm_count_t c)
{
    struct rpmtd_s td = {
	.tag = tag,
	.type = type,
	.data = (void *) p,
	.count = c,
    };
    return headerMod(h, &td);
}

static int headerPutWrap(Header h, rpmTag tag, rpmTagType type,
		rpm_constdata_t p, rpm_count_t c, headerPutFlags flags)
{
    struct rpmtd_s td = {
	.tag = tag,
	.type = type,
	.data = (void *) p,
	.count = c,
    };
    return headerPut(h, &td, flags);
}

int headerAddOrAppendEntry(Header h, rpmTag tag, rpmTagType type,
		rpm_constdata_t p, rpm_count_t c)
{
    return headerPutWrap(h, tag, type, p, c, HEADERPUT_APPEND);
}

int headerAppendEntry(Header h, rpmTag tag, rpmTagType type,
		rpm_constdata_t p, rpm_count_t c)
{
    return headerPutWrap(h, tag, type, p, c, HEADERPUT_APPEND);
}

int headerAddEntry(Header h, rpmTag tag, rpmTagType type,
		rpm_constdata_t p, rpm_count_t c)
{
    return headerPutWrap(h, tag, type, p, c, HEADERPUT_DEFAULT);
}
#undef _RPM_4_4_COMPAT
