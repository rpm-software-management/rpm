/** \ingroup header
 * \file lib/header.c
 */

/* RPM - Copyright (C) 1995-2002 Red Hat Software */

/* Data written to file descriptors is in network byte order.    */
/* Data read from file descriptors is expected to be in          */
/* network byte order and is converted on the fly to host order. */

#include "system.h"
#include <netdb.h>
#include <errno.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmstring.h>
#include "lib/header_internal.h"
#include "lib/misc.h"			/* tag function proto */

#include "debug.h"

/** \ingroup header
 */
const unsigned char rpm_header_magic[8] = {
	0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/** \ingroup header
 * Alignment needed for header data types.
 */
static const int typeAlign[16] =  {
    1,	/*!< RPM_NULL_TYPE */
    1,	/*!< RPM_CHAR_TYPE */
    1,	/*!< RPM_INT8_TYPE */
    2,	/*!< RPM_INT16_TYPE */
    4,	/*!< RPM_INT32_TYPE */
    8,	/*!< RPM_INT64_TYPE */
    1,	/*!< RPM_STRING_TYPE */
    1,	/*!< RPM_BIN_TYPE */
    1,	/*!< RPM_STRING_ARRAY_TYPE */
    1,	/*!< RPM_I18NSTRING_TYPE */
    0,
    0,
    0,
    0,
    0,
    0
};

/** \ingroup header
 * Size of header data types.
 */
static const int typeSizes[16] =  { 
    0,	/*!< RPM_NULL_TYPE */
    1,	/*!< RPM_CHAR_TYPE */
    1,	/*!< RPM_INT8_TYPE */
    2,	/*!< RPM_INT16_TYPE */
    4,	/*!< RPM_INT32_TYPE */
    8,	/*!< RPM_INT64_TYPE */
    -1,	/*!< RPM_STRING_TYPE */
    1,	/*!< RPM_BIN_TYPE */
    -1,	/*!< RPM_STRING_ARRAY_TYPE */
    -1,	/*!< RPM_I18NSTRING_TYPE */
    0,
    0,
    0,
    0,
    0,
    0
};

enum headerSorted_e {
    HEADERSORT_NONE	= 0,	/* Not sorted */
    HEADERSORT_OFFSET	= 1,	/* Sorted by offset (on-disk format) */
    HEADERSORT_INDEX	= 2,	/* Sorted by index  */
};

enum headerFlags_e {
    HEADERFLAG_ALLOCATED = (1 << 1), /*!< Is 1st header region allocated? */
    HEADERFLAG_LEGACY    = (1 << 2), /*!< Header came from legacy source? */
    HEADERFLAG_DEBUG     = (1 << 3), /*!< Debug this header? */
};

typedef rpmFlags headerFlags;

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
    int sorted;			/*!< Current sort method */
    int nrefs;			/*!< Reference count. */
};

/** \ingroup header
 * Maximum no. of bytes permitted in a header.
 */
static const size_t headerMaxbytes = (32*1024*1024);

#define	INDEX_MALLOC_SIZE	8

#define	ENTRY_IS_REGION(_e) \
	(((_e)->info.tag >= RPMTAG_HEADERIMAGE) && ((_e)->info.tag < RPMTAG_HEADERREGIONS))
#define	ENTRY_IN_REGION(_e)	((_e)->info.offset < 0)

/* Convert a 64bit value to network byte order. */
RPM_GNUC_CONST
static uint64_t htonll(uint64_t n)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint32_t *i = (uint32_t*)&n;
    uint32_t b = i[0];
    i[0] = htonl(i[1]);
    i[1] = htonl(b);
#endif
    return n;
}

Header headerLink(Header h)
{
    if (h != NULL)
	h->nrefs++;
    return h;
}

static Header headerUnlink(Header h)
{
    if (h != NULL)
	h->nrefs--;
    return NULL;
}

Header headerFree(Header h)
{
    (void) headerUnlink(h);

    if (h == NULL || h->nrefs > 0)
	return NULL;

    if (h->index) {
	indexEntry entry = h->index;
	int i;
	for (i = 0; i < h->indexUsed; i++, entry++) {
	    if ((h->flags & HEADERFLAG_ALLOCATED) && ENTRY_IS_REGION(entry)) {
		if (entry->length > 0) {
		    int32_t * ei = entry->data;
		    if ((ei - 2) == h->blob) h->blob = _free(h->blob);
		    entry->data = NULL;
		}
	    } else if (!ENTRY_IN_REGION(entry)) {
		entry->data = _free(entry->data);
	    }
	    entry->data = NULL;
	}
	h->index = _free(h->index);
    }

    h = _free(h);
    return NULL;
}

static Header headerCreate(void *blob, unsigned int pvlen, int32_t indexLen)
{
    Header h = xcalloc(1, sizeof(*h));
    if (blob) {
	h->blob = (pvlen > 0) ? memcpy(xmalloc(pvlen), blob, pvlen) : blob;
	h->indexAlloced = indexLen + 1;
	h->indexUsed = indexLen;
    } else {
	h->indexAlloced = INDEX_MALLOC_SIZE;
	h->indexUsed = 0;
    }
    h->instance = 0;
    h->sorted = HEADERSORT_NONE;

    h->index = (h->indexAlloced
	? xcalloc(h->indexAlloced, sizeof(*h->index))
	: NULL);

    h->nrefs = 0;
    return headerLink(h);
}

Header headerNew(void)
{
    return headerCreate(NULL, 0, 0);
}

int headerVerifyInfo(int il, int dl, const void * pev, void * iv, int negate)
{
    entryInfo pe = (entryInfo) pev;
    entryInfo info = iv;
    int i;

    for (i = 0; i < il; i++) {
	info->tag = ntohl(pe[i].tag);
	info->type = ntohl(pe[i].type);
	info->offset = ntohl(pe[i].offset);
	if (negate)
	    info->offset = -info->offset;
	info->count = ntohl(pe[i].count);

	if (hdrchkType(info->type))
	    return i;
	if (hdrchkAlign(info->type, info->offset))
	    return i;
	if (hdrchkRange(dl, info->offset))
	    return i;
	if (hdrchkData(info->count))
	    return i;

    }
    return -1;
}

static int indexCmp(const void * avp, const void * bvp)
{
    indexEntry ap = (indexEntry) avp, bp = (indexEntry) bvp;
    return (ap->info.tag - bp->info.tag);
}

void headerSort(Header h)
{
    if (h->sorted != HEADERSORT_INDEX) {
	qsort(h->index, h->indexUsed, sizeof(*h->index), indexCmp);
	h->sorted = HEADERSORT_INDEX;
    }
}

static int offsetCmp(const void * avp, const void * bvp) 
{
    indexEntry ap = (indexEntry) avp, bp = (indexEntry) bvp;
    int rc = (ap->info.offset - bp->info.offset);

    if (rc == 0) {
	/* Within a region, entries sort by address. Added drips sort by tag. */
	if (ap->info.offset < 0)
	    rc = (((char *)ap->data) - ((char *)bp->data));
	else
	    rc = (ap->info.tag - bp->info.tag);
    }
    return rc;
}

void headerUnsort(Header h)
{
    if (h->sorted != HEADERSORT_OFFSET) {
	qsort(h->index, h->indexUsed, sizeof(*h->index), offsetCmp);
	h->sorted = HEADERSORT_OFFSET;
    }
}

static inline unsigned int alignDiff(rpm_tagtype_t type, unsigned int alignsize)
{
    int typesize = typeSizes[type];

    if (typesize > 1) {
	unsigned int diff = typesize - (alignsize % typesize);
	if (diff != typesize)
	    return diff;
    }
    return 0;
}

unsigned headerSizeof(Header h, int magicp)
{
    indexEntry entry;
    unsigned int size = 0;
    int i;

    if (h == NULL)
	return size;

    headerSort(h);

    switch (magicp) {
    case HEADER_MAGIC_YES:
	size += sizeof(rpm_header_magic);
	break;
    case HEADER_MAGIC_NO:
	break;
    }

    size += 2 * sizeof(int32_t);	/* count of index entries */

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	/* Regions go in as is ... */
        if (ENTRY_IS_REGION(entry)) {
	    size += entry->length;
	    /* XXX Legacy regions do not include the region tag and data. */
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY))
		size += sizeof(struct entryInfo_s) + entry->info.count;
	    continue;
        }

	/* ... and region elements are skipped. */
	if (entry->info.offset < 0)
	    continue;

	/* Alignment */
	size += alignDiff(entry->info.type, size);

	size += sizeof(struct entryInfo_s) + entry->length;
    }

    return size;
}

/*
 * Header string (array) size calculation, bounded if end is non-NULL.
 * Return length (including \0 termination) on success, -1 on error.
 */
static inline int strtaglen(const char *str, rpm_count_t c, const char *end)
{
    const char *start = str;
    const char *s;

    if (end) {
	while ((s = memchr(start, '\0', end-start))) {
	    if (--c == 0 || s > end)
		break;
	    start = s + 1;
	}
    } else {
	while ((s = strchr(start, '\0'))) {
	    if (--c == 0)
		break;
	    start = s + 1;
	}
    }
    return (c > 0) ? -1 : (s - str + 1);
}

/**
 * Return length of entry data.
 * @param type		entry data type
 * @param p		entry data
 * @param count		entry item count
 * @param onDisk	data is concatenated strings (with NUL's))?
 * @param pend		pointer to end of data (or NULL)
 * @return		no. bytes in data, -1 on failure
 */
static int dataLength(rpm_tagtype_t type, rpm_constdata_t p, rpm_count_t count,
			 int onDisk, rpm_constdata_t pend)
{
    const char * s = p;
    const char * se = pend;
    int length = 0;

    switch (type) {
    case RPM_STRING_TYPE:
	if (count != 1)
	    return -1;
	length = strtaglen(s, 1, se);
	break;

    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
	/* These are like RPM_STRING_TYPE, except they're *always* an array */
	/* Compute sum of length of all strings, including nul terminators */

	if (onDisk) {
	    length = strtaglen(s, count, se);
	} else {
	    const char ** av = (const char **)p;
	    while (count--) {
		/* add one for null termination */
		length += strlen(*av++) + 1;
	    }
	}
	break;

    default:
	if (typeSizes[type] == -1)
	    return -1;
	length = typeSizes[(type & 0xf)] * count;
	if (length < 0 || (se && (s + length) > se))
	    return -1;
	break;
    }

    return length;
}

/** \ingroup header
 * Swap int32_t and int16_t arrays within header region.
 *
 * If a header region tag is in the set to be swabbed, as the data for a
 * a header region is located after all other tag data.
 *
 * @param entry		header entry
 * @param il		no. of entries
 * @param dl		start no. bytes of data
 * @param pe		header physical entry pointer (swapped)
 * @param dataStart	header data start
 * @param dataEnd	header data end
 * @param regionid	region offset
 * @param fast		use offsets for data sizes if possible
 * @return		no. bytes of data in region, -1 on error
 */
static int regionSwab(indexEntry entry, int il, int dl,
		entryInfo pe,
		unsigned char * dataStart,
		const unsigned char * dataEnd,
		int regionid, int fast)
{
    if ((entry != NULL && regionid >= 0) || (entry == NULL && regionid != 0))
	return -1;

    for (; il > 0; il--, pe++) {
	struct indexEntry_s ie;

	ie.info.tag = ntohl(pe->tag);
	ie.info.type = ntohl(pe->type);
	ie.info.count = ntohl(pe->count);
	ie.info.offset = ntohl(pe->offset);

	if (hdrchkType(ie.info.type))
	    return -1;
	if (hdrchkData(ie.info.count))
	    return -1;
	if (hdrchkData(ie.info.offset))
	    return -1;
	if (hdrchkAlign(ie.info.type, ie.info.offset))
	    return -1;

	ie.data = dataStart + ie.info.offset;
	if (dataEnd && (unsigned char *)ie.data >= dataEnd)
	    return -1;

	if (fast && il > 1) {
	    ie.length = ntohl(pe[1].offset) - ie.info.offset;
	} else {
	    ie.length = dataLength(ie.info.type, ie.data, ie.info.count,
				   1, dataEnd);
	}
	if (ie.length < 0 || hdrchkData(ie.length))
	    return -1;

	ie.rdlen = 0;

	if (entry) {
	    ie.info.offset = regionid;
	    *entry = ie;	/* structure assignment */
	    entry++;
	}

	/* Alignment */
	dl += alignDiff(ie.info.type, dl);

	/* Perform endian conversions */
	switch (ntohl(pe->type)) {
	case RPM_INT64_TYPE:
	{   uint64_t * it = ie.data;
	    for (; ie.info.count > 0; ie.info.count--, it += 1) {
		if (dataEnd && ((unsigned char *)it) >= dataEnd)
		    return -1;
		*it = htonll(*it);
	    }
	}   break;
	case RPM_INT32_TYPE:
	{   int32_t * it = ie.data;
	    for (; ie.info.count > 0; ie.info.count--, it += 1) {
		if (dataEnd && ((unsigned char *)it) >= dataEnd)
		    return -1;
		*it = htonl(*it);
	    }
	}   break;
	case RPM_INT16_TYPE:
	{   int16_t * it = ie.data;
	    for (; ie.info.count > 0; ie.info.count--, it += 1) {
		if (dataEnd && ((unsigned char *)it) >= dataEnd)
		    return -1;
		*it = htons(*it);
	    }
	}   break;
	}

	dl += ie.length;
    }

    return dl;
}

void * headerExport(Header h, unsigned int *bsize)
{
    int32_t * ei = NULL;
    entryInfo pe;
    char * dataStart;
    char * te;
    unsigned len, diff;
    int32_t il = 0;
    int32_t dl = 0;
    indexEntry entry; 
    int i;
    int drlen, ndribbles;

    if (h == NULL) return NULL;

    /* Sort entries by (offset,tag). */
    headerUnsort(h);

    /* Compute (il,dl) for all tags, including those deleted in region. */
    drlen = ndribbles = 0;
    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	if (ENTRY_IS_REGION(entry)) {
	    int32_t rdl = -entry->info.offset;	/* negative offset */
	    int32_t ril = rdl/sizeof(*pe);
	    int rid = entry->info.offset;

	    il += ril;
	    dl += entry->rdlen + entry->info.count;
	    /* XXX Legacy regions do not include the region tag and data. */
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY))
		il += 1;

	    /* Skip rest of entries in region, but account for dribbles. */
	    for (; i < h->indexUsed && entry->info.offset <= rid+1; i++, entry++) {
		if (entry->info.offset <= rid)
		    continue;

		/* Alignment */
		diff = alignDiff(entry->info.type, dl);
		if (diff) {
		    drlen += diff;
		    dl += diff;    
		}

		ndribbles++;
		il++;
		drlen += entry->length;
		dl += entry->length;
	    }
	    i--;
	    entry--;
	    continue;
	}

	/* Ignore deleted drips. */
	if (entry->data == NULL || entry->length <= 0)
	    continue;

	/* Alignment */
	dl += alignDiff(entry->info.type, dl);

	il++;
	dl += entry->length;
    }

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl))
	goto errxit;

    len = sizeof(il) + sizeof(dl) + (il * sizeof(*pe)) + dl;

    ei = xmalloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);

    pe = (entryInfo) &ei[2];
    dataStart = te = (char *) (pe + il);

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	const char * src;
	unsigned char *t;
	int count;
	int rdlen;
	unsigned int diff;

	if (entry->data == NULL || entry->length <= 0)
	    continue;

	t = (unsigned char*)te;
	pe->tag = htonl(entry->info.tag);
	pe->type = htonl(entry->info.type);
	pe->count = htonl(entry->info.count);

	if (ENTRY_IS_REGION(entry)) {
	    int32_t rdl = -entry->info.offset;	/* negative offset */
	    int32_t ril = rdl/sizeof(*pe) + ndribbles;
	    int rid = entry->info.offset;

	    src = (char *)entry->data;
	    rdlen = entry->rdlen;

	    /* XXX Legacy regions do not include the region tag and data. */
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY)) {
		int32_t stei[4];

		memcpy(pe+1, src, rdl);
		memcpy(te, src + rdl, rdlen);
		te += rdlen;

		pe->offset = htonl(te - dataStart);
		stei[0] = pe->tag;
		stei[1] = pe->type;
		stei[2] = htonl(-rdl-entry->info.count);
		stei[3] = pe->count;
		memcpy(te, stei, entry->info.count);
		te += entry->info.count;
		ril++;
		rdlen += entry->info.count;

		count = regionSwab(NULL, ril, 0, pe, t, NULL, 0, 0);
		if (count != rdlen)
		    goto errxit;

	    } else {

		memcpy(pe+1, src + sizeof(*pe), ((ril-1) * sizeof(*pe)));
		memcpy(te, src + (ril * sizeof(*pe)), rdlen+entry->info.count+drlen);
		te += rdlen;
		{  
		    entryInfo se = (entryInfo)src;
		    int off = ntohl(se->offset);
		    pe->offset = (off) ? htonl(te - dataStart) : htonl(off);
		}
		te += entry->info.count + drlen;

		count = regionSwab(NULL, ril, 0, pe, t, NULL, 0, 0);
		if (count != (rdlen + entry->info.count + drlen))
		    goto errxit;
	    }

	    /* Skip rest of entries in region. */
	    while (i < h->indexUsed && entry->info.offset <= rid+1) {
		i++;
		entry++;
	    }
	    i--;
	    entry--;
	    pe += ril;
	    continue;
	}

	/* Ignore deleted drips. */
	if (entry->data == NULL || entry->length <= 0)
	    continue;

	/* Alignment */
	diff = alignDiff(entry->info.type, (te - dataStart));
	if (diff) {
	    memset(te, 0, diff);
	    te += diff;
	}

	pe->offset = htonl(te - dataStart);

	/* copy data w/ endian conversions */
	switch (entry->info.type) {
	case RPM_INT64_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((uint64_t *)te) = htonll(*((uint64_t *)src));
		te += sizeof(uint64_t);
		src += sizeof(uint64_t);
	    }
	    break;

	case RPM_INT32_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int32_t *)te) = htonl(*((int32_t *)src));
		te += sizeof(int32_t);
		src += sizeof(int32_t);
	    }
	    break;

	case RPM_INT16_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int16_t *)te) = htons(*((int16_t *)src));
		te += sizeof(int16_t);
		src += sizeof(int16_t);
	    }
	    break;

	default:
	    memcpy(te, entry->data, entry->length);
	    te += entry->length;
	    break;
	}
	pe++;
    }
   
    /* Insure that there are no memcpy underruns/overruns. */
    if (((char *)pe) != dataStart)
	goto errxit;
    if ((((char *)ei)+len) != te)
	goto errxit;

    if (bsize)
	*bsize = len;

    headerSort(h);

    return (void *) ei;

errxit:
    free(ei);
    return NULL;
}

void * headerUnload(Header h)
{
    return headerExport(h, NULL);
}

/**
 * Find matching (tag,type) entry in header.
 * @param h		header
 * @param tag		entry tag
 * @param type		entry type
 * @return 		header entry
 */
static
indexEntry findEntry(Header h, rpmTagVal tag, rpm_tagtype_t type)
{
    indexEntry entry;
    struct indexEntry_s key;

    if (h == NULL) return NULL;
    if (h->sorted != HEADERSORT_INDEX)
	headerSort(h);

    key.info.tag = tag;

    entry = bsearch(&key, h->index, h->indexUsed, sizeof(*h->index), indexCmp);
    if (entry == NULL)
	return NULL;

    if (type == RPM_NULL_TYPE)
	return entry;

    /* look backwards */
    while (entry->info.tag == tag && entry->info.type != type &&
	   entry > h->index) entry--;

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    return NULL;
}

int headerDel(Header h, rpmTagVal tag)
{
    indexEntry last = h->index + h->indexUsed;
    indexEntry entry, first;
    int ne;

    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) return 1;

    /* Make sure entry points to the first occurrence of this tag. */
    while (entry > h->index && (entry - 1)->info.tag == tag)  
	entry--;

    /* Free data for tags being removed. */
    for (first = entry; first < last; first++) {
	rpm_data_t data;
	if (first->info.tag != tag)
	    break;
	data = first->data;
	first->data = NULL;
	first->length = 0;
	if (ENTRY_IN_REGION(first))
	    continue;
	free(data);
    }

    ne = (first - entry);
    if (ne > 0) {
	h->indexUsed -= ne;
	ne = last - first;
	if (ne > 0)
	    memmove(entry, first, (ne * sizeof(*entry)));
    }

    return 0;
}

Header headerImport(void * blob, unsigned int bsize, headerImportFlags flags)
{
    const int32_t * ei = (int32_t *) blob;
    int32_t il = ntohl(ei[0]);		/* index length */
    int32_t dl = ntohl(ei[1]);		/* data length */
    unsigned int pvlen = sizeof(il) + sizeof(dl) +
		    (il * sizeof(struct entryInfo_s)) + dl;;
    Header h = NULL;
    entryInfo pe;
    unsigned char * dataStart;
    unsigned char * dataEnd;
    indexEntry entry; 
    int rdlen;
    int fast = (flags & HEADERIMPORT_FAST);

    /* Sanity checks on header intro. */
    if (bsize && bsize != pvlen)
	goto errxit;
    if (hdrchkTags(il) || hdrchkData(dl) || pvlen >= headerMaxbytes)
	goto errxit;

    h = headerCreate(blob, (flags & HEADERIMPORT_COPY) ? pvlen : 0, il);

    ei = h->blob; /* In case we had to copy */
    pe = (entryInfo) &ei[2];
    dataStart = (unsigned char *) (pe + il);
    dataEnd = dataStart + dl;

    entry = h->index;
    if (!(htonl(pe->tag) < RPMTAG_HEADERI18NTABLE)) {
	h->flags |= HEADERFLAG_LEGACY;
	entry->info.type = REGION_TAG_TYPE;
	entry->info.tag = RPMTAG_HEADERIMAGE;
	entry->info.count = REGION_TAG_COUNT;
	entry->info.offset = ((unsigned char *)pe - dataStart); /* negative offset */

	entry->data = pe;
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, il, 0, pe,
			   dataStart, dataEnd, entry->info.offset, fast);
	if (rdlen != dl)
	    goto errxit;
	entry->rdlen = rdlen;
	h->indexUsed++;
    } else {
	int32_t rdl;
	int32_t ril;

	h->flags &= ~HEADERFLAG_LEGACY;

	entry->info.type = htonl(pe->type);
	entry->info.count = htonl(pe->count);
	entry->info.tag = htonl(pe->tag);

	if (!ENTRY_IS_REGION(entry))
	    goto errxit;
	if (entry->info.type != REGION_TAG_TYPE)
	    goto errxit;
	if (entry->info.count != REGION_TAG_COUNT)
	    goto errxit;

	{   int off = ntohl(pe->offset);

	    if (off) {
		size_t nb = REGION_TAG_COUNT;
		int32_t stei[nb];
		if (hdrchkRange(dl, (off + nb)))
		    goto errxit;
		/* XXX Hmm, why the copy? */
		memcpy(&stei, dataStart + off, nb);
		rdl = -ntohl(stei[2]);	/* negative offset */
		ril = rdl/sizeof(*pe);
		if (hdrchkTags(ril) || hdrchkData(rdl))
		    goto errxit;
	    } else {
		ril = il;
		rdl = (ril * sizeof(struct entryInfo_s));
		entry->info.tag = RPMTAG_HEADERIMAGE;
	    }
	}
	entry->info.offset = -rdl;	/* negative offset */

	entry->data = pe;
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, ril-1, 0, pe+1,
			   dataStart, dataEnd, entry->info.offset, fast);
	if (rdlen < 0)
	    goto errxit;
	entry->rdlen = rdlen;

	if (ril < h->indexUsed) {
	    indexEntry newEntry = entry + ril;
	    int ne = (h->indexUsed - ril);
	    int rid = entry->info.offset+1;

	    /* Load dribble entries from region. */
	    rdlen = regionSwab(newEntry, ne, rdlen, pe+ril,
				dataStart, dataEnd, rid, fast);
	    if (rdlen < 0)
		goto errxit;

	  { indexEntry firstEntry = newEntry;
	    int save = h->indexUsed;
	    int j;

	    /* Dribble entries replace duplicate region entries. */
	    h->indexUsed -= ne;
	    for (j = 0; j < ne; j++, newEntry++) {
		(void) headerDel(h, newEntry->info.tag);
		if (newEntry->info.tag == RPMTAG_BASENAMES)
		    (void) headerDel(h, RPMTAG_OLDFILENAMES);
	    }

	    /* If any duplicate entries were replaced, move new entries down. */
	    if (h->indexUsed < (save - ne)) {
		memmove(h->index + h->indexUsed, firstEntry,
			(ne * sizeof(*entry)));
	    }
	    h->indexUsed += ne;
	  }
	}

	rdlen += REGION_TAG_COUNT;

	if (rdlen != dl)
	    goto errxit;
    }

    /* Force sorting, dribble lookups can cause early sort on partial header */
    h->sorted = HEADERSORT_NONE;
    headerSort(h);
    h->flags |= HEADERFLAG_ALLOCATED;

    return h;

errxit:
    if (h) {
	if (flags & HEADERIMPORT_COPY)
	    free(h->blob);
	free(h->index);
	free(h);
    }
    return NULL;
}

Header headerReload(Header h, rpmTagVal tag)
{
    Header nh;
    unsigned int uc = 0;
    void * uh = headerExport(h, &uc);

    h = headerFree(h);
    if (uh == NULL)
	return NULL;
    nh = headerImport(uh, uc, 0);
    if (nh == NULL) {
	uh = _free(uh);
	return NULL;
    }
    if (ENTRY_IS_REGION(nh->index)) {
	if (tag == RPMTAG_HEADERSIGNATURES || tag == RPMTAG_HEADERIMMUTABLE)
	    nh->index[0].info.tag = tag;
    }
    return nh;
}

Header headerLoad(void * uh)
{
    return headerImport(uh, 0, 0);
}

Header headerCopyLoad(const void * uh)
{
    /* Discards const but that's ok as we'll take a copy */
    return headerImport((void *)uh, 0, HEADERIMPORT_COPY);
}

Header headerRead(FD_t fd, int magicp)
{
    int32_t block[4];
    int32_t * ei = NULL;
    int32_t il;
    int32_t dl;
    Header h = NULL;
    unsigned int len, blen;

    if (magicp == HEADER_MAGIC_YES) {
	int32_t magic;

	if (Freadall(fd, block, 4*sizeof(*block)) != 4*sizeof(*block))
	    goto exit;

	magic = block[0];

	if (memcmp(&magic, rpm_header_magic, sizeof(magic)))
	    goto exit;

	il = ntohl(block[2]);
	dl = ntohl(block[3]);
    } else {
	if (Freadall(fd, block, 2*sizeof(*block)) != 2*sizeof(*block))
	    goto exit;

	il = ntohl(block[0]);
	dl = ntohl(block[1]);
    }

    blen = (il * sizeof(struct entryInfo_s)) + dl;
    len = sizeof(il) + sizeof(dl) + blen;

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl) || len > headerMaxbytes)
	goto exit;

    ei = xmalloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);

    if (Freadall(fd, (char *)&ei[2], blen) != blen)
	goto exit;
    
    h = headerImport(ei, len, 0);

exit:
    if (h == NULL && ei != NULL) {
	free(ei);
    }
    return h;
}

int headerWrite(FD_t fd, Header h, int magicp)
{
    ssize_t nb;
    unsigned int length;
    void * uh;

    uh = headerExport(h, &length);
    if (uh == NULL)
	return 1;
    switch (magicp) {
    case HEADER_MAGIC_YES:
	nb = Fwrite(rpm_header_magic, sizeof(uint8_t), sizeof(rpm_header_magic), fd);
	if (nb != sizeof(rpm_header_magic))
	    goto exit;
	break;
    case HEADER_MAGIC_NO:
	break;
    }

    nb = Fwrite(uh, sizeof(char), length, fd);

exit:
    free(uh);
    return (nb == length ? 0 : 1);
}

int headerIsEntry(Header h, rpmTagVal tag)
{
   		/* FIX: h modified by sort. */
    return (findEntry(h, tag, RPM_NULL_TYPE) ? 1 : 0);
   	
}

/** \ingroup header
 * Retrieve data from header entry.
 * Relevant flags (others are ignored), if neither is set allocation
 * behavior depends on data type(!) 
 *     HEADERGET_MINMEM: return pointers to header memory
 *     HEADERGET_ALLOC: always return malloced memory, overrides MINMEM
 * 
 * @todo Permit retrieval of regions other than HEADER_IMUTABLE.
 * @param entry		header entry
 * @param td		tag data container
 * @param flags		flags to control memory allocation
 * @return		1 on success, otherwise error.
 */
static int copyTdEntry(const indexEntry entry, rpmtd td, headerGetFlags flags)
{
    rpm_count_t count = entry->info.count;
    int rc = 1;		/* XXX 1 on success. */
    /* ALLOC overrides MINMEM */
    int allocMem = flags & HEADERGET_ALLOC;
    int minMem = allocMem ? 0 : flags & HEADERGET_MINMEM;
    int argvArray = (flags & HEADERGET_ARGV) ? 1 : 0;

    assert(td != NULL);
    td->flags = RPMTD_IMMUTABLE;
    switch (entry->info.type) {
    case RPM_BIN_TYPE:
	/*
	 * XXX This only works for
	 * XXX 	"sealed" HEADER_IMMUTABLE/HEADER_SIGNATURES/HEADER_IMAGE.
	 * XXX This will *not* work for unsealed legacy HEADER_IMAGE (i.e.
	 * XXX a legacy header freshly read, but not yet unloaded to the rpmdb).
	 */
	if (ENTRY_IS_REGION(entry)) {
	    int32_t * ei = ((int32_t *)entry->data) - 2;
	    entryInfo pe = (entryInfo) (ei + 2);
	    unsigned char * dataStart = (unsigned char *) (pe + ntohl(ei[0]));
	    int32_t rdl = -entry->info.offset;	/* negative offset */
	    int32_t ril = rdl/sizeof(*pe);

	    rdl = entry->rdlen;
	    count = 2 * sizeof(*ei) + (ril * sizeof(*pe)) + rdl;
	    if (entry->info.tag == RPMTAG_HEADERIMAGE) {
		ril -= 1;
		pe += 1;
	    } else {
		count += REGION_TAG_COUNT;
		rdl += REGION_TAG_COUNT;
	    }

	    td->data = xmalloc(count);
	    ei = (int32_t *) td->data;
	    ei[0] = htonl(ril);
	    ei[1] = htonl(rdl);

	    pe = (entryInfo) memcpy(ei + 2, pe, (ril * sizeof(*pe)));

	    dataStart = (unsigned char *) memcpy(pe + ril, dataStart, rdl);

	    rc = regionSwab(NULL, ril, 0, pe, dataStart, dataStart + rdl, 0, 0);
	    /* don't return data on failure */
	    if (rc < 0) {
		td->data = _free(td->data);
	    }
	    /* XXX 1 on success. */
	    rc = (rc < 0) ? 0 : 1;
	} else {
	    count = entry->length;
	    td->data = (!minMem
		? memcpy(xmalloc(count), entry->data, count)
		: entry->data);
	}
	break;
    case RPM_STRING_TYPE:
	/* simple string, but fallthrough if its actually an array */
	if (count == 1 && !argvArray) {
	    td->data = allocMem ? xstrdup(entry->data) : entry->data;
	    break;
	}
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	const char ** ptrEntry;
	int tableSize = (count + argvArray) * sizeof(char *);
	char * t;
	int i;

	if (minMem) {
	    td->data = xmalloc(tableSize);
	    ptrEntry = (const char **) td->data;
	    t = entry->data;
	} else {
	    t = xmalloc(tableSize + entry->length);
	    td->data = (void *)t;
	    ptrEntry = (const char **) td->data;
	    t += tableSize;
	    memcpy(t, entry->data, entry->length);
	}
	for (i = 0; i < count; i++) {
	    *ptrEntry++ = t;
	    t = strchr(t, 0);
	    t++;
	}
	if (argvArray) {
	    *ptrEntry = NULL;
	    td->flags |= RPMTD_ARGV;
	}
    }	break;
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
    case RPM_INT64_TYPE:
	if (allocMem) {
	    td->data = xmalloc(entry->length);
	    memcpy(td->data, entry->data, entry->length);
	} else {
	    td->data = entry->data;
	}
	break;
    default:
	/* WTH? Don't mess with unknown data types... */
	rc = 0;
	td->data = NULL;
	break;
    }
    td->type = entry->info.type;
    td->count = count;
    td->size = entry->length;

    if (td->data && entry->data != td->data) {
	td->flags |= RPMTD_ALLOCED;
    }

    return rc;
}

/**
 * Does locale match entry in header i18n table?
 * 
 * \verbatim
 * The range [l,le) contains the next locale to match:
 *    ll[_CC][.EEEEE][@dddd]
 * where
 *    ll	ISO language code (in lowercase).
 *    CC	(optional) ISO coutnry code (in uppercase).
 *    EEEEE	(optional) encoding (not really standardized).
 *    dddd	(optional) dialect.
 * \endverbatim
 *
 * @param td		header i18n table data, NUL terminated
 * @param l		start of locale	to match
 * @param le		end of locale to match
 * @return		1 on good match, 2 on weak match, 0 on no match
 */
static int headerMatchLocale(const char *td, const char *l, const char *le)
{
    const char *fe;

    /* First try a complete match. */
    if (strlen(td) == (le-l) && rstreqn(td, l, (le - l)))
	return 1;

    /* Next, try stripping optional dialect and matching.  */
    for (fe = l; fe < le && *fe != '@'; fe++)
	{};
    if (fe < le && rstreqn(td, l, (fe - l)))
	return 1;

    /* Next, try stripping optional codeset and matching.  */
    for (fe = l; fe < le && *fe != '.'; fe++)
	{};
    if (fe < le && rstreqn(td, l, (fe - l)))
	return 1;

    /* Finally, try stripping optional country code and matching. */
    for (fe = l; fe < le && *fe != '_'; fe++)
	{};
    if (fe < le && rstreqn(td, l, (fe - l)))
	return 2;

    return 0;
}

/**
 * Return i18n string from header that matches locale.
 * @param h		header
 * @param entry		i18n string data
 * @retval td		tag data container
 * @param flags		flags to control allocation
 * @return		1 always
 */
static int copyI18NEntry(Header h, indexEntry entry, rpmtd td, 
						headerGetFlags flags)
{
    const char *lang, *l, *le;
    indexEntry table;

    td->type = RPM_STRING_TYPE;
    td->count = 1;
    /* if no match, just return the first string */
    td->data = entry->data;

    /* XXX Drepper sez' this is the order. */
    if ((lang = getenv("LANGUAGE")) == NULL &&
	(lang = getenv("LC_ALL")) == NULL &&
	(lang = getenv("LC_MESSAGES")) == NULL &&
	(lang = getenv("LANG")) == NULL)
	    goto exit;
    
    if ((table = findEntry(h, RPMTAG_HEADERI18NTABLE, RPM_STRING_ARRAY_TYPE)) == NULL)
	goto exit;

    for (l = lang; *l != '\0'; l = le) {
	const char *t;
	char *ed, *ed_weak = NULL;
	int langNum;

	while (*l && *l == ':')			/* skip leading colons */
	    l++;
	if (*l == '\0')
	    break;
	for (le = l; *le && *le != ':'; le++)	/* find end of this locale */
	    {};

	/* For each entry in the header ... */
	for (langNum = 0, t = table->data, ed = entry->data;
	     langNum < entry->info.count;
	     langNum++, t += strlen(t) + 1, ed += strlen(ed) + 1) {

	    int match = headerMatchLocale(t, l, le);
	    if (match == 1) {
		td->data = ed;
		goto exit;
	    } else if (match == 2) { 
		ed_weak = ed;
	    }
	}
	if (ed_weak) {
	    td->data = ed_weak;
	    goto exit;
	}
    }

exit:
    if (flags & HEADERGET_ALLOC) {
	td->data = xstrdup(td->data);
	td->flags |= RPMTD_ALLOCED;
    }

    return 1;
}

/**
 * Retrieve tag data from header.
 * @param h		header
 * @retval td		tag data container
 * @param flags		flags to control retrieval
 * @return		1 on success, 0 on not found
 */
static int intGetTdEntry(Header h, rpmtd td, headerGetFlags flags)
{
    indexEntry entry;
    int rc;

    /* First find the tag */
    /* FIX: h modified by sort. */
    entry = findEntry(h, td->tag, RPM_NULL_TYPE);
    if (entry == NULL) {
	/* Td is zeroed above, just return... */
	return 0;
    }

    if (flags & HEADERGET_RAW) {
	rc = copyTdEntry(entry, td, flags);
    } else {
	switch (entry->info.type) {
	case RPM_I18NSTRING_TYPE:
	    rc = copyI18NEntry(h, entry, td, flags);
	    break;
	default:
	    rc = copyTdEntry(entry, td, flags);
	    break;
	}
    }

    if (rc == 0)
	td->flags |= RPMTD_INVALID;

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

int headerGet(Header h, rpmTagVal tag, rpmtd td, headerGetFlags flags)
{
    int rc;
    headerTagTagFunction tagfunc = intGetTdEntry;

    if (td == NULL) return 0;

    rpmtdReset(td);
    td->tag = tag;

    if (flags & HEADERGET_EXT) {
	headerTagTagFunction extfunc = rpmHeaderTagFunc(tag);
	if (extfunc) tagfunc = extfunc;
    }
    rc = tagfunc(h, td, flags);

    assert(tag == td->tag);
    return rc;
}

/**
 */
static void copyData(rpm_tagtype_t type, rpm_data_t dstPtr, 
		rpm_constdata_t srcPtr, rpm_count_t cnt, int dataLength)
{
    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	const char ** av = (const char **) srcPtr;
	char * t = dstPtr;

	while (cnt-- > 0 && dataLength > 0) {
	    const char * s;
	    if ((s = *av++) == NULL)
		continue;
	    do {
		*t++ = *s++;
	    } while (s[-1] && --dataLength > 0);
	}
    }	break;

    default:
	memmove(dstPtr, srcPtr, dataLength);
	break;
    }
}

/**
 * Return (malloc'ed) copy of entry data.
 * @param type		entry data type
 * @param p		entry data
 * @param c		entry item count
 * @retval lengthPtr	no. bytes in returned data
 * @return 		(malloc'ed) copy of entry data, NULL on error
 */
static void *
grabData(rpm_tagtype_t type, rpm_constdata_t p, rpm_count_t c, int * lengthPtr)
{
    rpm_data_t data = NULL;
    int length;

    length = dataLength(type, p, c, 0, NULL);
    if (length > 0) {
	data = xmalloc(length);
	copyData(type, data, p, c, length);
    }

    if (lengthPtr)
	*lengthPtr = length;
    return data;
}

static int intAddEntry(Header h, rpmtd td)
{
    indexEntry entry;
    rpm_data_t data;
    int length = 0;

    /* Count must always be >= 1 for headerAddEntry. */
    if (td->count <= 0)
	return 0;

    if (hdrchkType(td->type))
	return 0;
    if (hdrchkData(td->count))
	return 0;

    data = grabData(td->type, td->data, td->count, &length);
    if (data == NULL)
	return 0;

    /* Allocate more index space if necessary */
    if (h->indexUsed == h->indexAlloced) {
	h->indexAlloced += INDEX_MALLOC_SIZE;
	h->index = xrealloc(h->index, h->indexAlloced * sizeof(*h->index));
    }

    /* Fill in the index */
    entry = h->index + h->indexUsed;
    entry->info.tag = td->tag;
    entry->info.type = td->type;
    entry->info.count = td->count;
    entry->info.offset = 0;
    entry->data = data;
    entry->length = length;

    if (h->indexUsed > 0 && td->tag < h->index[h->indexUsed-1].info.tag)
	h->sorted = HEADERSORT_NONE;
    h->indexUsed++;

    return 1;
}

static int intAppendEntry(Header h, rpmtd td)
{
    indexEntry entry;
    int length;

    if (td->type == RPM_STRING_TYPE || td->type == RPM_I18NSTRING_TYPE) {
	/* we can't do this */
	return 0;
    }

    /* Find the tag entry in the header. */
    entry = findEntry(h, td->tag, td->type);
    if (!entry)
	return 0;

    length = dataLength(td->type, td->data, td->count, 0, NULL);
    if (length < 0)
	return 0;

    if (ENTRY_IN_REGION(entry)) {
	char * t = xmalloc(entry->length + length);
	memcpy(t, entry->data, entry->length);
	entry->data = t;
	entry->info.offset = 0;
    } else
	entry->data = xrealloc(entry->data, entry->length + length);

    copyData(td->type, ((char *) entry->data) + entry->length, 
	     td->data, td->count, length);

    entry->length += length;

    entry->info.count += td->count;

    return 1;
}

int headerPut(Header h, rpmtd td, headerPutFlags flags)
{
    int rc;
    
    assert(td != NULL);
    if (flags & HEADERPUT_APPEND) {
	rc = findEntry(h, td->tag, td->type) ?
		intAppendEntry(h, td) :
		intAddEntry(h, td);
    } else {
	rc = intAddEntry(h, td);
    }
    return rc;
}

int headerAddI18NString(Header h, rpmTagVal tag, const char * string,
		const char * lang)
{
    indexEntry table, entry;
    const char ** strArray;
    int length;
    int ghosts;
    rpm_count_t i, langNum;
    char * buf;

    table = findEntry(h, RPMTAG_HEADERI18NTABLE, RPM_STRING_ARRAY_TYPE);
    entry = findEntry(h, tag, RPM_I18NSTRING_TYPE);

    if (!table && entry)
	return 0;		/* this shouldn't ever happen!! */

    if (!table && !entry) {
	const char * charArray[2];
	rpm_count_t count = 0;
	struct rpmtd_s td;
	if (!lang || (lang[0] == 'C' && lang[1] == '\0')) {
	    charArray[count++] = "C";
	} else {
	    charArray[count++] = "C";
	    charArray[count++] = lang;
	}
	
	rpmtdReset(&td);
	td.tag = RPMTAG_HEADERI18NTABLE;
	td.type = RPM_STRING_ARRAY_TYPE;
	td.data = (void *) charArray;
	td.count = count;
	if (!headerPut(h, &td, HEADERPUT_DEFAULT))
	    return 0;
	table = findEntry(h, RPMTAG_HEADERI18NTABLE, RPM_STRING_ARRAY_TYPE);
    }

    if (!table)
	return 0;
    if (!lang) lang = "C";

    {	const char * l = table->data;
	for (langNum = 0; langNum < table->info.count; langNum++) {
	    if (rstreq(l, lang)) break;
	    l += strlen(l) + 1;
	}
    }

    if (langNum >= table->info.count) {
	length = strlen(lang) + 1;
	if (ENTRY_IN_REGION(table)) {
	    char * t = xmalloc(table->length + length);
	    memcpy(t, table->data, table->length);
	    table->data = t;
	    table->info.offset = 0;
	} else
	    table->data = xrealloc(table->data, table->length + length);
	memmove(((char *)table->data) + table->length, lang, length);
	table->length += length;
	table->info.count++;
    }

    if (!entry) {
	int rc;
	struct rpmtd_s td;
	strArray = xmalloc(sizeof(*strArray) * (langNum + 1));
	for (i = 0; i < langNum; i++)
	    strArray[i] = "";
	strArray[langNum] = string;

	rpmtdReset(&td);
	td.tag = tag;
	td.type = RPM_I18NSTRING_TYPE;
	td.data = strArray;
	td.count = langNum + 1;
	rc = headerPut(h, &td, HEADERPUT_DEFAULT);
	free(strArray);
	return rc;
    } else if (langNum >= entry->info.count) {
	ghosts = langNum - entry->info.count;
	
	length = strlen(string) + 1 + ghosts;
	if (ENTRY_IN_REGION(entry)) {
	    char * t = xmalloc(entry->length + length);
	    memcpy(t, entry->data, entry->length);
	    entry->data = t;
	    entry->info.offset = 0;
	} else
	    entry->data = xrealloc(entry->data, entry->length + length);

	memset(((char *)entry->data) + entry->length, '\0', ghosts);
	memmove(((char *)entry->data) + entry->length + ghosts, string, strlen(string)+1);

	entry->length += length;
	entry->info.count = langNum + 1;
    } else {
	char *b, *be, *e, *ee, *t;
	size_t bn, sn, en;

	/* Set beginning/end pointers to previous data */
	b = be = e = ee = entry->data;
	for (i = 0; i < table->info.count; i++) {
	    if (i == langNum)
		be = ee;
	    ee += strlen(ee) + 1;
	    if (i == langNum)
		e  = ee;
	}

	/* Get storage for new buffer */
	bn = (be-b);
	sn = strlen(string) + 1;
	en = (ee-e);
	length = bn + sn + en;
	t = buf = xmalloc(length);

	/* Copy values into new storage */
	memcpy(t, b, bn);
	t += bn;
	memcpy(t, string, sn);
	t += sn;
	memcpy(t, e, en);
	t += en;

	/* Replace i18N string array */
	entry->length -= strlen(be) + 1;
	entry->length += sn;
	
	if (ENTRY_IN_REGION(entry)) {
	    entry->info.offset = 0;
	} else
	    entry->data = _free(entry->data);
	entry->data = buf;
    }

    return 0;
}

int headerMod(Header h, rpmtd td)
{
    indexEntry entry;
    rpm_data_t oldData;
    rpm_data_t data;
    int length = 0;

    /* First find the tag */
    entry = findEntry(h, td->tag, td->type);
    if (!entry)
	return 0;

    data = grabData(td->type, td->data, td->count, &length);
    if (data == NULL)
	return 0;

    /* make sure entry points to the first occurrence of this tag */
    while (entry > h->index && (entry - 1)->info.tag == td->tag)  
	entry--;

    /* free after we've grabbed the new data in case the two are intertwined;
       that's a bad idea but at least we won't break */
    oldData = entry->data;

    entry->info.count = td->count;
    entry->info.type = td->type;
    entry->data = data;
    entry->length = length;

    if (ENTRY_IN_REGION(entry)) {
	entry->info.offset = 0;
    } else
	free(oldData);

    return 1;
}

/**
 * Header tag iterator data structure.
 */
struct headerIterator_s {
    Header h;		/*!< Header being iterated. */
    int next_index;	/*!< Next tag index. */
};

HeaderIterator headerFreeIterator(HeaderIterator hi)
{
    if (hi != NULL) {
	hi->h = headerFree(hi->h);
	hi = _free(hi);
    }
    return NULL;
}

HeaderIterator headerInitIterator(Header h)
{
    HeaderIterator hi = xmalloc(sizeof(*hi));

    headerSort(h);

    hi->h = headerLink(h);
    hi->next_index = 0;
    return hi;
}

static indexEntry nextIndex(HeaderIterator hi)
{
    Header h = hi->h;
    int slot;
    indexEntry entry = NULL;

    for (slot = hi->next_index; slot < h->indexUsed; slot++) {
	entry = h->index + slot;
	if (!ENTRY_IS_REGION(entry))
	    break;
    }
    hi->next_index = slot;
    if (entry == NULL || slot >= h->indexUsed)
	return NULL;

    hi->next_index++;
    return entry;
}

rpmTagVal headerNextTag(HeaderIterator hi)
{
    indexEntry entry = nextIndex(hi);
    return entry ? entry->info.tag : RPMTAG_NOT_FOUND;
}

int headerNext(HeaderIterator hi, rpmtd td)
{
    indexEntry entry = nextIndex(hi);
    int rc = 0;

    rpmtdReset(td);
    if (entry) {
	td->tag = entry->info.tag;
	rc = copyTdEntry(entry, td, HEADERGET_DEFAULT);
    }
    return ((rc == 1) ? 1 : 0);
}

unsigned int headerGetInstance(Header h)
{
    return h ? h->instance : 0;
}

void headerSetInstance(Header h, unsigned int instance)
{
    h->instance = instance;
}    

#define RETRY_ERROR(_err) \
    ((_err) == EINTR || (_err) == EAGAIN || (_err) == EWOULDBLOCK)

ssize_t Freadall(FD_t fd, void * buf, ssize_t size)
{
    ssize_t total = 0;
    ssize_t nb = 0;
    char * bufp = buf;

    while (total < size) {
	nb = Fread(bufp, 1, size - total, fd);

	if (nb == 0 || (nb < 0 && !RETRY_ERROR(errno))) {
	    total = nb;
	    break;
	}

	if (nb > 0) {
	    bufp += nb;
	    total += nb;
	}
    }

    return total;
}

