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

char ** headerGetLangs(Header h)
{
    char **s, *e, **table;
    rpmTagType type;
    rpm_count_t i, count;

    if (!headerGetRawEntry(h, HEADER_I18NTABLE, &type, (rpm_data_t)&s, &count))
	return NULL;

    /* XXX xcalloc never returns NULL. */
    if ((table = (char **)xcalloc((count+1), sizeof(char *))) == NULL)
	return NULL;

    for (i = 0, e = *s; i < count; i++, e += strlen(e)+1)
	table[i] = e;
    table[count] = NULL;

    return table;	/* LCL: double indirection? */
}

void headerDump(Header h, FILE *f, int flags)
{
    int i;
    indexEntry p;
    const char * tag;
    const char * type;

    /* First write out the length of the index (count of index entries) */
    fprintf(f, "Entry count: %d\n", h->indexUsed);

    /* Now write the index */
    p = h->index;
    fprintf(f, "\n             CT  TAG                  TYPE         "
		"      OFSET      COUNT\n");
    for (i = 0; i < h->indexUsed; i++) {
	switch (p->info.type) {
	case RPM_NULL_TYPE:
	    type = "NULL";
	    break;
	case RPM_CHAR_TYPE:
	    type = "CHAR";
	    break;
	case RPM_BIN_TYPE:
	    type = "BIN";
	    break;
	case RPM_INT8_TYPE:
	    type = "INT8";
	    break;
	case RPM_INT16_TYPE:
	    type = "INT16";
	    break;
	case RPM_INT32_TYPE:
	    type = "INT32";
	    break;
	case RPM_INT64_TYPE:
	    type = "INT64";
	    break;
	case RPM_STRING_TYPE:
	    type = "STRING";
	    break;
	case RPM_STRING_ARRAY_TYPE:
	    type = "STRING_ARRAY";
	    break;
	case RPM_I18NSTRING_TYPE:
	    type = "I18N_STRING";
	    break;
	default:
	    type = "(unknown)";
	    break;
	}

	tag = rpmTagGetName(p->info.tag);

	fprintf(f, "Entry      : %3.3d (%d)%-14s %-18s 0x%.8x %.8d\n", i,
		p->info.tag, tag, type, (unsigned) p->info.offset,
		(int) p->info.count);

	if (flags & HEADER_DUMP_INLINE) {
	    char *dp = p->data;
	    int c = p->info.count;
	    int ct = 0;

	    /* Print the data inline */
	    switch (p->info.type) {
	    case RPM_INT32_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%08x (%d)\n", ct++,
			    (unsigned) *((int32_t *) dp),
			    (int) *((int32_t *) dp));
		    dp += sizeof(int32_t);
		}
		break;

	    case RPM_INT16_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%04x (%d)\n", ct++,
			    (unsigned) (*((int16_t *) dp) & 0xffff),
			    (int) *((int16_t *) dp));
		    dp += sizeof(int16_t);
		}
		break;
	    case RPM_INT8_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%02x (%d)\n", ct++,
			    (unsigned) (*((int8_t *) dp) & 0xff),
			    (int) *((int8_t *) dp));
		    dp += sizeof(int8_t);
		}
		break;
	    case RPM_BIN_TYPE:
		while (c > 0) {
		    fprintf(f, "       Data: %.3d ", ct);
		    while (c--) {
			fprintf(f, "%02x ", (unsigned) (*(int8_t *)dp & 0xff));
			ct++;
			dp += sizeof(int8_t);
			if (! (ct % 8)) {
			    break;
			}
		    }
		    fprintf(f, "\n");
		}
		break;
	    case RPM_CHAR_TYPE:
		while (c--) {
		    char ch = (char) *((char *) dp);
		    fprintf(f, "       Data: %.3d 0x%2x %c (%d)\n", ct++,
			    (unsigned)(ch & 0xff),
			    (isprint(ch) ? ch : ' '),
			    (int) *((char *) dp));
		    dp += sizeof(char);
		}
		break;
	    case RPM_STRING_TYPE:
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d %s\n", ct++, (char *) dp);
		    dp = strchr(dp, 0);
		    dp++;
		}
		break;
	    default:
		fprintf(stderr, _("Data type %d not supported\n"), 
			(int) p->info.type);
		break;
	    }
	}
	p++;
    }
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
