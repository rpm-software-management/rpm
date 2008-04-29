/** \ingroup header
 * \file rpmdb/header.c
 */

/* RPM - Copyright (C) 1995-2002 Red Hat Software */

/* Data written to file descriptors is in network byte order.    */
/* Data read from file descriptors is expected to be in          */
/* network byte order and is converted on the fly to host order. */

#include "system.h"

#include <rpm/rpmtag.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmpgp.h>
#include "rpmdb/header_internal.h"

#include "debug.h"

int _hdr_debug = 0;



#define PARSER_BEGIN 	0
#define PARSER_IN_ARRAY 1
#define PARSER_IN_EXPR  2

/** \ingroup header
 */
static unsigned char const header_magic[8] = {
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
    -1,	/*!< RPM_INT64_TYPE */
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

/** \ingroup header
 * Maximum no. of bytes permitted in a header.
 */
static const size_t headerMaxbytes = (32*1024*1024);

/**
 * Sanity check on no. of tags.
 * This check imposes a limit of 65K tags, more than enough.
 */ 
#define hdrchkTags(_ntags)	((_ntags) & 0xffff0000)

/**
 * Sanity check on type values.
 */
#define hdrchkType(_type) ((_type) < RPM_MIN_TYPE || (_type) > RPM_MAX_TYPE)

/**
 * Sanity check on data size and/or offset and/or count.
 * This check imposes a limit of 16Mb, more than enough.
 */ 
#define hdrchkData(_nbytes)	((_nbytes) & 0xff000000)

/**
 * Sanity check on alignment for data type.
 */
#define hdrchkAlign(_type, _off)	((_off) & (typeAlign[_type]-1))

/**
 * Sanity check on range of data offset.
 */
#define hdrchkRange(_dl, _off)		((_off) < 0 || (_off) > (_dl))

Header headerLink(Header h)
{
    if (h == NULL) return NULL;

    h->nrefs++;
if (_hdr_debug)
fprintf(stderr, "--> h  %p ++ %d at %s:%u\n", h, h->nrefs, __FILE__, __LINE__);

    return h;
}

Header headerUnlink(Header h)
{
    if (h == NULL) return NULL;
if (_hdr_debug)
fprintf(stderr, "--> h  %p -- %d at %s:%u\n", h, h->nrefs, __FILE__, __LINE__);
    h->nrefs--;
    return NULL;
}

Header headerFree(Header h)
{
    (void) headerUnlink(h);

    if (h == NULL || h->nrefs > 0)
	return NULL;	/* XXX return previous header? */

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
    return h;
}

Header headerNew(void)
{
    Header h = xcalloc(1, sizeof(*h));

    h->blob = NULL;
    h->indexAlloced = INDEX_MALLOC_SIZE;
    h->indexUsed = 0;
    h->flags |= HEADERFLAG_SORTED;

    h->index = (h->indexAlloced
	? xcalloc(h->indexAlloced, sizeof(*h->index))
	: NULL);

    h->nrefs = 0;
    return headerLink(h);
}

/**
 */
static int indexCmp(const void * avp, const void * bvp)
{
    indexEntry ap = (indexEntry) avp, bp = (indexEntry) bvp;
    return (ap->info.tag - bp->info.tag);
}

void headerSort(Header h)
{
    if (!(h->flags & HEADERFLAG_SORTED)) {
	qsort(h->index, h->indexUsed, sizeof(*h->index), indexCmp);
	h->flags |= HEADERFLAG_SORTED;
    }
}

/**
 */
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

/** \ingroup header
 * Restore tags in header to original ordering.
 * @param h		header
 */
void headerUnsort(Header h)
{
    qsort(h->index, h->indexUsed, sizeof(*h->index), offsetCmp);
}

unsigned headerSizeof(Header h, enum hMagic magicp)
{
    indexEntry entry;
    unsigned int size = 0;
    unsigned int pad = 0;
    int i;

    if (h == NULL)
	return size;

    headerSort(h);

    switch (magicp) {
    case HEADER_MAGIC_YES:
	size += sizeof(header_magic);
	break;
    case HEADER_MAGIC_NO:
	break;
    }

    size += 2 * sizeof(int32_t);	/* count of index entries */

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	unsigned diff;
	rpmTagType type;

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
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    diff = typeSizes[type] - (size % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		size += diff;
		pad += diff;
	    }
	}

	size += sizeof(struct entryInfo_s) + entry->length;
    }

    return size;
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
static int dataLength(rpmTagType type, rpm_constdata_t p, rpm_count_t count,
			 int onDisk, rpm_constdata_t pend)
{
    const unsigned char * s = p;
    const unsigned char * se = pend;
    int length = 0;

    switch (type) {
    case RPM_STRING_TYPE:
	if (count != 1)
	    return -1;
	while (*s++) {
	    if (se && s > se)
		return -1;
	    length++;
	}
	length++;	/* count nul terminator too. */
	break;

    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
	/* These are like RPM_STRING_TYPE, except they're *always* an array */
	/* Compute sum of length of all strings, including nul terminators */

	if (onDisk) {
	    while (count--) {
		length++;       /* count nul terminator too */
               while (*s++) {
		    if (se && s > se)
			return -1;
		    length++;
		}
	    }
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
 * @param dataStart	header data start
 * @param dataEnd	header data end
 * @param regionid	region offset
 * @return		no. bytes of data in region, -1 on error
 */
static int regionSwab(indexEntry entry, int il, int dl,
		entryInfo pe,
		unsigned char * dataStart,
		const unsigned char * dataEnd,
		int regionid)
{
    unsigned char * tprev = NULL;
    unsigned char * t = NULL;
    int tdel = 0;
    int tl = dl;
    struct indexEntry_s ieprev;

    memset(&ieprev, 0, sizeof(ieprev));
    for (; il > 0; il--, pe++) {
	struct indexEntry_s ie;
	rpmTagType type;

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

	ie.data = t = dataStart + ie.info.offset;
	if (dataEnd && t >= dataEnd)
	    return -1;

	ie.length = dataLength(ie.info.type, ie.data, ie.info.count, 1, dataEnd);
	if (ie.length < 0 || hdrchkData(ie.length))
	    return -1;

	ie.rdlen = 0;

	if (entry) {
	    ie.info.offset = regionid;
	    *entry = ie;	/* structure assignment */
	    entry++;
	}

	/* Alignment */
	type = ie.info.type;
	if (typeSizes[type] > 1) {
	    unsigned diff;
	    diff = typeSizes[type] - (dl % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		dl += diff;
		if (ieprev.info.type == RPM_I18NSTRING_TYPE)
		    ieprev.length += diff;
	    }
	}
	tdel = (tprev ? (t - tprev) : 0);
	if (ieprev.info.type == RPM_I18NSTRING_TYPE)
	    tdel = ieprev.length;

	if (ie.info.tag >= HEADER_I18NTABLE) {
	    tprev = t;
	} else {
	    tprev = dataStart;
	    /* XXX HEADER_IMAGE tags don't include region sub-tag. */
	    if (ie.info.tag == HEADER_IMAGE)
		tprev -= REGION_TAG_COUNT;
	}

	/* Perform endian conversions */
	switch (ntohl(pe->type)) {
	case RPM_INT32_TYPE:
	{   int32_t * it = (int32_t *)t;
	    for (; ie.info.count > 0; ie.info.count--, it += 1) {
		if (dataEnd && ((unsigned char *)it) >= dataEnd)
		    return -1;
		*it = htonl(*it);
	    }
	    t = (unsigned char *) it;
	}   break;
	case RPM_INT16_TYPE:
	{   int16_t * it = (int16_t *) t;
	    for (; ie.info.count > 0; ie.info.count--, it += 1) {
		if (dataEnd && ((unsigned char *)it) >= dataEnd)
		    return -1;
		*it = htons(*it);
	    }
	    t = (unsigned char *) it;
	}   break;
	default:
	    t += ie.length;
	    break;
	}

	dl += ie.length;
	tl += tdel;
	ieprev = ie;	/* structure assignment */

    }
    tdel = (tprev ? (t - tprev) : 0);
    tl += tdel;

    /* XXX
     * There are two hacks here:
     *	1) tl is 16b (i.e. REGION_TAG_COUNT) short while doing headerReload().
     *	2) the 8/98 rpm bug with inserting i18n tags needs to use tl, not dl.
     */
    if (tl+REGION_TAG_COUNT == dl)
	tl += REGION_TAG_COUNT;

    return dl;
}

/** \ingroup header
 * doHeaderUnload.
 * @param h		header
 * @retval *lengthPtr	no. bytes in unloaded header blob
 * @return		unloaded header blob (NULL on error)
 */
static void * doHeaderUnload(Header h,
		size_t * lengthPtr)
{
    int32_t * ei = NULL;
    entryInfo pe;
    char * dataStart;
    char * te;
    unsigned pad;
    unsigned len;
    int32_t il = 0;
    int32_t dl = 0;
    indexEntry entry; 
    rpmTagType type;
    int i;
    int drlen, ndribbles;
    int driplen, ndrips;
    int legacy = 0;

    /* Sort entries by (offset,tag). */
    headerUnsort(h);

    /* Compute (il,dl) for all tags, including those deleted in region. */
    pad = 0;
    drlen = ndribbles = driplen = ndrips = 0;
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
		type = entry->info.type;
		if (typeSizes[type] > 1) {
		    unsigned diff;
		    diff = typeSizes[type] - (dl % typeSizes[type]);
		    if (diff != typeSizes[type]) {
			drlen += diff;
			pad += diff;
			dl += diff;
		    }
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
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    unsigned diff;
	    diff = typeSizes[type] - (dl % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		driplen += diff;
		pad += diff;
		dl += diff;
	    } else
		diff = 0;
	}

	ndrips++;
	il++;
	driplen += entry->length;
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

    pad = 0;
    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	const char * src;
	unsigned char *t;
	int count;
	int rdlen;

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

		legacy = 1;
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

		count = regionSwab(NULL, ril, 0, pe, t, NULL, 0);
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

		count = regionSwab(NULL, ril, 0, pe, t, NULL, 0);
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
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    unsigned diff;
	    diff = typeSizes[type] - ((te - dataStart) % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		memset(te, 0, diff);
		te += diff;
		pad += diff;
	    }
	}

	pe->offset = htonl(te - dataStart);

	/* copy data w/ endian conversions */
	switch (entry->info.type) {
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

    if (lengthPtr)
	*lengthPtr = len;

    h->flags &= ~HEADERFLAG_SORTED;
    headerSort(h);

    return (void *) ei;

errxit:
    ei = _free(ei);
    return (void *) ei;
}

void * headerUnload(Header h)
{
    size_t length;
    void * uh = doHeaderUnload(h, &length);
    return uh;
}

/**
 * Find matching (tag,type) entry in header.
 * @param h		header
 * @param tag		entry tag
 * @param type		entry type
 * @return 		header entry
 */
static
indexEntry findEntry(Header h, rpmTag tag, rpmTagType type)
{
    indexEntry entry, entry2, last;
    struct indexEntry_s key;

    if (h == NULL) return NULL;
    if (!(h->flags & HEADERFLAG_SORTED)) headerSort(h);

    key.info.tag = tag;

    entry2 = entry = 
	bsearch(&key, h->index, h->indexUsed, sizeof(*h->index), indexCmp);
    if (entry == NULL)
	return NULL;

    if (type == RPM_NULL_TYPE)
	return entry;

    /* look backwards */
    while (entry->info.tag == tag && entry->info.type != type &&
	   entry > h->index) entry--;

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    last = h->index + h->indexUsed;
    /* FIX: entry2 = entry. Code looks bogus as well. */
    while (entry2->info.tag == tag && entry2->info.type != type &&
	   entry2 < last) entry2++;

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    return NULL;
}

int headerRemoveEntry(Header h, rpmTag tag)
{
    indexEntry last = h->index + h->indexUsed;
    indexEntry entry, first;
    int ne;

    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) return 1;

    /* Make sure entry points to the first occurence of this tag. */
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
	data = _free(data);
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

Header headerLoad(void * uh)
{
    int32_t * ei = (int32_t *) uh;
    int32_t il = ntohl(ei[0]);		/* index length */
    int32_t dl = ntohl(ei[1]);		/* data length */
    size_t pvlen = sizeof(il) + sizeof(dl) +
               (il * sizeof(struct entryInfo_s)) + dl;
    void * pv = uh;
    Header h = NULL;
    entryInfo pe;
    unsigned char * dataStart;
    unsigned char * dataEnd;
    indexEntry entry; 
    int rdlen;
    int i;

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl))
	goto errxit;

    ei = (int32_t *) pv;
    pe = (entryInfo) &ei[2];
    dataStart = (unsigned char *) (pe + il);
    dataEnd = dataStart + dl;

    h = xcalloc(1, sizeof(*h));
    h->blob = uh;
    h->indexAlloced = il + 1;
    h->indexUsed = il;
    h->index = xcalloc(h->indexAlloced, sizeof(*h->index));
    h->flags |= HEADERFLAG_SORTED;
    h->nrefs = 0;
    h = headerLink(h);

    entry = h->index;
    i = 0;
    if (!(htonl(pe->tag) < HEADER_I18NTABLE)) {
	h->flags |= HEADERFLAG_LEGACY;
	entry->info.type = REGION_TAG_TYPE;
	entry->info.tag = HEADER_IMAGE;
	entry->info.count = REGION_TAG_COUNT;
	entry->info.offset = ((unsigned char *)pe - dataStart); /* negative offset */

	entry->data = pe;
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, il, 0, pe, dataStart, dataEnd, entry->info.offset);
#if 0	/* XXX don't check, the 8/98 i18n bug fails here. */
	if (rdlen != dl)
	    goto errxit;
#endif
	entry->rdlen = rdlen;
	entry++;
	h->indexUsed++;
    } else {
	int32_t rdl;
	int32_t ril;

	h->flags &= ~HEADERFLAG_LEGACY;

	entry->info.type = htonl(pe->type);
	entry->info.count = htonl(pe->count);

	if (hdrchkType(entry->info.type))
	    goto errxit;
	if (hdrchkTags(entry->info.count))
	    goto errxit;

	{   int off = ntohl(pe->offset);

	    if (hdrchkData(off))
		goto errxit;
	    if (off) {
		size_t nb = REGION_TAG_COUNT;
		int32_t stei[nb];
		/* XXX Hmm, why the copy? */
		memcpy(&stei, dataStart + off, nb);
		rdl = -ntohl(stei[2]);	/* negative offset */
		ril = rdl/sizeof(*pe);
		if (hdrchkTags(ril) || hdrchkData(rdl))
		    goto errxit;
		entry->info.tag = htonl(pe->tag);
	    } else {
		ril = il;
		rdl = (ril * sizeof(struct entryInfo_s));
		entry->info.tag = HEADER_IMAGE;
	    }
	}
	entry->info.offset = -rdl;	/* negative offset */

	entry->data = pe;
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, ril-1, 0, pe+1, dataStart, dataEnd, entry->info.offset);
	if (rdlen < 0)
	    goto errxit;
	entry->rdlen = rdlen;

	if (ril < h->indexUsed) {
	    indexEntry newEntry = entry + ril;
	    int ne = (h->indexUsed - ril);
	    int rid = entry->info.offset+1;
	    int rc;

	    /* Load dribble entries from region. */
	    rc = regionSwab(newEntry, ne, 0, pe+ril, dataStart, dataEnd, rid);
	    if (rc < 0)
		goto errxit;
	    rdlen += rc;

	  { indexEntry firstEntry = newEntry;
	    int save = h->indexUsed;
	    int j;

	    /* Dribble entries replace duplicate region entries. */
	    h->indexUsed -= ne;
	    for (j = 0; j < ne; j++, newEntry++) {
		(void) headerRemoveEntry(h, newEntry->info.tag);
		if (newEntry->info.tag == HEADER_BASENAMES)
		    (void) headerRemoveEntry(h, HEADER_OLDFILENAMES);
	    }

	    /* If any duplicate entries were replaced, move new entries down. */
	    if (h->indexUsed < (save - ne)) {
		memmove(h->index + h->indexUsed, firstEntry,
			(ne * sizeof(*entry)));
	    }
	    h->indexUsed += ne;
	  }
	}
    }

    h->flags &= ~HEADERFLAG_SORTED;
    headerSort(h);

    return h;

errxit:
    if (h) {
	h->index = _free(h->index);
	h = _free(h);
    }
    return h;
}

Header headerReload(Header h, rpmTag tag)
{
    Header nh;
    size_t length;
    void * uh = doHeaderUnload(h, &length);

    h = headerFree(h);
    if (uh == NULL)
	return NULL;
    nh = headerLoad(uh);
    if (nh == NULL) {
	uh = _free(uh);
	return NULL;
    }
    if (nh->flags & HEADERFLAG_ALLOCATED)
	uh = _free(uh);
    nh->flags |= HEADERFLAG_ALLOCATED;
    if (ENTRY_IS_REGION(nh->index)) {
	if (tag == HEADER_SIGNATURES || tag == HEADER_IMMUTABLE)
	    nh->index[0].info.tag = tag;
    }
    return nh;
}

Header headerCopyLoad(const void * uh)
{
    int32_t * ei = (int32_t *) uh;
    int32_t il = ntohl(ei[0]);		/* index length */
    int32_t dl = ntohl(ei[1]);		/* data length */
    size_t pvlen = sizeof(il) + sizeof(dl) +
			(il * sizeof(struct entryInfo_s)) + dl;
    void * nuh = NULL;
    Header h = NULL;

    /* Sanity checks on header intro. */
    if (!(hdrchkTags(il) || hdrchkData(dl)) && pvlen < headerMaxbytes) {
	nuh = memcpy(xmalloc(pvlen), uh, pvlen);
	if ((h = headerLoad(nuh)) != NULL)
	    h->flags |= HEADERFLAG_ALLOCATED;
    }
    if (h == NULL)
	nuh = _free(nuh);
    return h;
}

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
Header headerRead(FD_t fd, enum hMagic magicp)
{
    int32_t block[4];
    int32_t reserved;
    int32_t * ei = NULL;
    int32_t il;
    int32_t dl;
    int32_t magic;
    Header h = NULL;
    size_t len;
    int i;

    memset(block, 0, sizeof(block));
    i = 2;
    if (magicp == HEADER_MAGIC_YES)
	i += 2;

    /* FIX: cast? */
    if (timedRead(fd, (char *)block, i*sizeof(*block)) != (i * sizeof(*block)))
	goto exit;

    i = 0;

    if (magicp == HEADER_MAGIC_YES) {
	magic = block[i++];
	if (memcmp(&magic, header_magic, sizeof(magic)))
	    goto exit;
	reserved = block[i++];
    }
    
    il = ntohl(block[i]);	i++;
    dl = ntohl(block[i]);	i++;

    len = sizeof(il) + sizeof(dl) + (il * sizeof(struct entryInfo_s)) + dl;

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl) || len > headerMaxbytes)
	goto exit;

    ei = xmalloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);
    len -= sizeof(il) + sizeof(dl);

    /* FIX: cast? */
    if (timedRead(fd, (char *)&ei[2], len) != len)
	goto exit;
    
    h = headerLoad(ei);

exit:
    if (h) {
	if (h->flags & HEADERFLAG_ALLOCATED)
	    ei = _free(ei);
	h->flags |= HEADERFLAG_ALLOCATED;
    } else if (ei)
	ei = _free(ei);
   	/* FIX: timedRead macro obscures annotation */
    return h;
}

int headerWrite(FD_t fd, Header h, enum hMagic magicp)
{
    ssize_t nb;
    size_t length;
    void * uh;

    if (h == NULL)
	return 1;
    uh = doHeaderUnload(h, &length);
    if (uh == NULL)
	return 1;
    switch (magicp) {
    case HEADER_MAGIC_YES:
	nb = Fwrite(header_magic, sizeof(char), sizeof(header_magic), fd);
	if (nb != sizeof(header_magic))
	    goto exit;
	break;
    case HEADER_MAGIC_NO:
	break;
    }

    nb = Fwrite(uh, sizeof(char), length, fd);

exit:
    uh = _free(uh);
    return (nb == length ? 0 : 1);
}

int headerIsEntry(Header h, rpmTag tag)
{
   		/* FIX: h modified by sort. */
    return (findEntry(h, tag, RPM_NULL_TYPE) ? 1 : 0);
   	
}

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
static int copyEntry(const indexEntry entry,
		rpmTagType * type,
		rpm_data_t * p,
		rpm_count_t * c,
		int minMem)
{
    rpm_count_t count = entry->info.count;
    int rc = 1;		/* XXX 1 on success. */

    if (p)
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
	    if (entry->info.tag == HEADER_IMAGE) {
		ril -= 1;
		pe += 1;
	    } else {
		count += REGION_TAG_COUNT;
		rdl += REGION_TAG_COUNT;
	    }

	    *p = xmalloc(count);
	    ei = (int32_t *) *p;
	    ei[0] = htonl(ril);
	    ei[1] = htonl(rdl);

	    pe = (entryInfo) memcpy(ei + 2, pe, (ril * sizeof(*pe)));

	    dataStart = (unsigned char *) memcpy(pe + ril, dataStart, rdl);

	    rc = regionSwab(NULL, ril, 0, pe, dataStart, dataStart + rdl, 0);
	    /* XXX 1 on success. */
	    rc = (rc < 0) ? 0 : 1;
	} else {
	    count = entry->length;
	    *p = (!minMem
		? memcpy(xmalloc(count), entry->data, count)
		: entry->data);
	}
	break;
    case RPM_STRING_TYPE:
	if (count == 1) {
	    *p = entry->data;
	    break;
	}
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	const char ** ptrEntry;
	int tableSize = count * sizeof(char *);
	char * t;
	int i;

	if (minMem) {
	    *p = xmalloc(tableSize);
	    ptrEntry = (const char **) *p;
	    t = entry->data;
	} else {
	    t = xmalloc(tableSize + entry->length);
	    *p = (void *)t;
	    ptrEntry = (const char **) *p;
	    t += tableSize;
	    memcpy(t, entry->data, entry->length);
	}
	for (i = 0; i < count; i++) {
	    *ptrEntry++ = t;
	    t = strchr(t, 0);
	    t++;
	}
    }	break;

    default:
	*p = entry->data;
	break;
    }
    if (type) *type = entry->info.type;
    if (c) *c = count;
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
    if (strlen(td) == (le-l) && !strncmp(td, l, (le - l)))
	return 1;

    /* Next, try stripping optional dialect and matching.  */
    for (fe = l; fe < le && *fe != '@'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    /* Next, try stripping optional codeset and matching.  */
    for (fe = l; fe < le && *fe != '.'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    /* Finally, try stripping optional country code and matching. */
    for (fe = l; fe < le && *fe != '_'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 2;

    return 0;
}

/**
 * Return i18n string from header that matches locale.
 * @param h		header
 * @param entry		i18n string data
 * @return		matching i18n string (or 1st string if no match)
 */
static char *
headerFindI18NString(Header h, indexEntry entry)
{
    const char *lang, *l, *le;
    indexEntry table;

    /* XXX Drepper sez' this is the order. */
    if ((lang = getenv("LANGUAGE")) == NULL &&
	(lang = getenv("LC_ALL")) == NULL &&
	(lang = getenv("LC_MESSAGES")) == NULL &&
	(lang = getenv("LANG")) == NULL)
	    return entry->data;
    
    if ((table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE)) == NULL)
	return entry->data;

    for (l = lang; *l != '\0'; l = le) {
	const char *td;
	char *ed, *ed_weak = NULL;
	int langNum;

	while (*l && *l == ':')			/* skip leading colons */
	    l++;
	if (*l == '\0')
	    break;
	for (le = l; *le && *le != ':'; le++)	/* find end of this locale */
	    {};

	/* For each entry in the header ... */
	for (langNum = 0, td = table->data, ed = entry->data;
	     langNum < entry->info.count;
	     langNum++, td += strlen(td) + 1, ed += strlen(ed) + 1) {

	           int match = headerMatchLocale(td, l, le);
		   if (match == 1) return ed;
		   else if (match == 2) ed_weak = ed;

	}
	if (ed_weak) return ed_weak;
    }

    return entry->data;
}

/**
 * Retrieve tag data from header.
 * @param h		header
 * @param tag		tag to retrieve
 * @retval type		address of type (or NULL)
 * @retval p		address of data (or NULL)
 * @retval c		address of count (or NULL)
 * @param minMem	string pointers reference header memory?
 * @return		1 on success, 0 on not found
 */
static int intGetEntry(Header h, rpmTag tag,
		rpmTagType * type,
		rpm_data_t * p,
		rpm_count_t * c,
		int minMem)
{
    indexEntry entry;
    int rc;

    /* First find the tag */
   		/* FIX: h modified by sort. */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (entry == NULL) {
	if (type) type = 0;
	if (p) *p = NULL;
	if (c) *c = 0;
	return 0;
    }

    switch (entry->info.type) {
    case RPM_I18NSTRING_TYPE:
	rc = 1;
	if (type) *type = RPM_STRING_TYPE;
	if (c) *c = 1;
	if (p) *p = headerFindI18NString(h, entry);
	break;
    default:
	rc = copyEntry(entry, type, p, c, minMem);
	break;
    }

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

void * headerFreeTag(Header h, rpm_data_t data, rpmTagType type)
{
    if (data) {
	if (type == RPM_FORCEFREE_TYPE ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		data = _free(data);
    }
    return NULL;
}

int headerGetEntry(Header h, rpmTag tag,
			rpmTagType * type,
			rpm_data_t * p,
			rpm_count_t * c)
{
    return intGetEntry(h, tag, type, p, c, 0);
}

int headerGetEntryMinMemory(Header h, rpmTag tag,
			rpmTagType * type,
			rpm_data_t * p,
			rpm_count_t * c)
{
    return intGetEntry(h, tag, type, (rpm_data_t) p, c, 1);
}

int headerGetRawEntry(Header h, rpmTag tag, rpmTagType * type, rpm_data_t * p,
		rpm_count_t * c)
{
    indexEntry entry;
    int rc;

    if (p == NULL) return headerIsEntry(h, tag);

    /* First find the tag */
   		/* FIX: h modified by sort. */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) {
	if (p) *p = NULL;
	if (c) *c = 0;
	return 0;
    }

    rc = copyEntry(entry, type, p, c, 0);

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

/**
 */
static void copyData(rpmTagType type, rpm_data_t dstPtr, 
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
grabData(rpmTagType type, rpm_constdata_t p, rpm_count_t c, int * lengthPtr)
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

int headerAddEntry(Header h, rpmTag tag, rpmTagType type, 
			rpm_constdata_t p, rpm_count_t c)
{
    indexEntry entry;
    rpm_data_t data;
    int length;

    /* Count must always be >= 1 for headerAddEntry. */
    if (c <= 0)
	return 0;

    if (hdrchkType(type))
	return 0;
    if (hdrchkData(c))
	return 0;

    length = 0;
    data = grabData(type, p, c, &length);
    if (data == NULL || length <= 0)
	return 0;

    /* Allocate more index space if necessary */
    if (h->indexUsed == h->indexAlloced) {
	h->indexAlloced += INDEX_MALLOC_SIZE;
	h->index = xrealloc(h->index, h->indexAlloced * sizeof(*h->index));
    }

    /* Fill in the index */
    entry = h->index + h->indexUsed;
    entry->info.tag = tag;
    entry->info.type = type;
    entry->info.count = c;
    entry->info.offset = 0;
    entry->data = data;
    entry->length = length;

    if (h->indexUsed > 0 && tag < h->index[h->indexUsed-1].info.tag)
	h->flags &= ~HEADERFLAG_SORTED;
    h->indexUsed++;

    return 1;
}

int headerAppendEntry(Header h, rpmTag tag, rpmTagType type,
		rpm_constdata_t p, rpm_count_t c)
{
    indexEntry entry;
    int length;

    if (type == RPM_STRING_TYPE || type == RPM_I18NSTRING_TYPE) {
	/* we can't do this */
	return 0;
    }

    /* Find the tag entry in the header. */
    entry = findEntry(h, tag, type);
    if (!entry)
	return 0;

    length = dataLength(type, p, c, 0, NULL);
    if (length < 0)
	return 0;

    if (ENTRY_IN_REGION(entry)) {
	char * t = xmalloc(entry->length + length);
	memcpy(t, entry->data, entry->length);
	entry->data = t;
	entry->info.offset = 0;
    } else
	entry->data = xrealloc(entry->data, entry->length + length);

    copyData(type, ((char *) entry->data) + entry->length, p, c, length);

    entry->length += length;

    entry->info.count += c;

    return 1;
}

int headerAddOrAppendEntry(Header h, rpmTag tag, rpmTagType type,
		rpm_constdata_t p, rpm_count_t c)
{
    return (findEntry(h, tag, type)
	? headerAppendEntry(h, tag, type, p, c)
	: headerAddEntry(h, tag, type, p, c));
}

int headerAddI18NString(Header h, rpmTag tag, const char * string,
		const char * lang)
{
    indexEntry table, entry;
    const char ** strArray;
    int length;
    int ghosts;
    rpm_count_t i, langNum;
    char * buf;

    table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE);
    entry = findEntry(h, tag, RPM_I18NSTRING_TYPE);

    if (!table && entry)
	return 0;		/* this shouldn't ever happen!! */

    if (!table && !entry) {
	const char * charArray[2];
	rpm_count_t count = 0;
	if (!lang || (lang[0] == 'C' && lang[1] == '\0')) {
	    charArray[count++] = "C";
	} else {
	    charArray[count++] = "C";
	    charArray[count++] = lang;
	}
	if (!headerAddEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE, 
			&charArray, count))
	    return 0;
	table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE);
    }

    if (!table)
	return 0;
    if (!lang) lang = "C";

    {	const char * l = table->data;
	for (langNum = 0; langNum < table->info.count; langNum++) {
	    if (!strcmp(l, lang)) break;
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
	strArray = xmalloc(sizeof(*strArray) * (langNum + 1));
	for (i = 0; i < langNum; i++)
	    strArray[i] = "";
	strArray[langNum] = string;
	rc = headerAddEntry(h, tag, RPM_I18NSTRING_TYPE, strArray, langNum + 1);
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

int headerModifyEntry(Header h, rpmTag tag, rpmTagType type,
			rpm_constdata_t p, rpm_count_t c)
{
    indexEntry entry;
    rpm_data_t oldData;
    rpm_data_t data;
    int length;

    /* First find the tag */
    entry = findEntry(h, tag, type);
    if (!entry)
	return 0;

    length = 0;
    data = grabData(type, p, c, &length);
    if (data == NULL || length <= 0)
	return 0;

    /* make sure entry points to the first occurence of this tag */
    while (entry > h->index && (entry - 1)->info.tag == tag)  
	entry--;

    /* free after we've grabbed the new data in case the two are intertwined;
       that's a bad idea but at least we won't break */
    oldData = entry->data;

    entry->info.count = c;
    entry->info.type = type;
    entry->data = data;
    entry->length = length;

    if (ENTRY_IN_REGION(entry)) {
	entry->info.offset = 0;
    } else
	oldData = _free(oldData);

    return 1;
}

/**
 */
static char escapedChar(const char ch)	
{
    switch (ch) {
    case 'a': 	return '\a';
    case 'b': 	return '\b';
    case 'f': 	return '\f';
    case 'n': 	return '\n';
    case 'r': 	return '\r';
    case 't': 	return '\t';
    case 'v': 	return '\v';
    default:	return ch;
    }
}

/**
 * Destroy headerSprintf format array.
 * @param format	sprintf format array
 * @param num		number of elements
 * @return		NULL always
 */
static sprintfToken
freeFormat( sprintfToken format, int num)
{
    int i;

    if (format == NULL) return NULL;

    for (i = 0; i < num; i++) {
	switch (format[i].type) {
	case PTOK_ARRAY:
	    format[i].u.array.format =
		freeFormat(format[i].u.array.format,
			format[i].u.array.numTokens);
	    break;
	case PTOK_COND:
	    format[i].u.cond.ifFormat =
		freeFormat(format[i].u.cond.ifFormat, 
			format[i].u.cond.numIfTokens);
	    format[i].u.cond.elseFormat =
		freeFormat(format[i].u.cond.elseFormat, 
			format[i].u.cond.numElseTokens);
	    break;
	case PTOK_NONE:
	case PTOK_TAG:
	case PTOK_STRING:
	default:
	    break;
	}
    }
    format = _free(format);
    return NULL;
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
    return hi;
}

HeaderIterator headerInitIterator(Header h)
{
    HeaderIterator hi = xmalloc(sizeof(*hi));

    headerSort(h);

    hi->h = headerLink(h);
    hi->next_index = 0;
    return hi;
}

int headerNextIterator(HeaderIterator hi,
		rpmTag * tag,
		rpmTagType * type,
		rpm_data_t * p,
		rpm_count_t * c)
{
    Header h = hi->h;
    int slot = hi->next_index;
    indexEntry entry = NULL;
    int rc;

    for (slot = hi->next_index; slot < h->indexUsed; slot++) {
	entry = h->index + slot;
	if (!ENTRY_IS_REGION(entry))
	    break;
    }
    hi->next_index = slot;
    if (entry == NULL || slot >= h->indexUsed)
	return 0;

   	/* LCL: no clue */
    hi->next_index++;

    if (tag)
	*tag = entry->info.tag;

    rc = copyEntry(entry, type, p, c, 0);

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
Header headerCopy(Header h)
{
    Header nh = headerNew();
    HeaderIterator hi;
    rpmTagType type;
    rpmTag tag;
    rpm_count_t count;
    rpm_data_t ptr;
   
    for (hi = headerInitIterator(h);
	headerNextIterator(hi, &tag, &type, &ptr, &count);
	ptr = headerFreeData(ptr, type))
    {
	if (ptr) (void) headerAddEntry(nh, tag, type, ptr, count);
    }
    hi = headerFreeIterator(hi);

    return headerReload(nh, HEADER_IMAGE);
}

/**
 */
typedef struct headerSprintfArgs_s {
    Header h;
    char * fmt;
    headerTagTableEntry tags;
    headerSprintfExtension exts;
    const char * errmsg;
    rpmec ec;
    sprintfToken format;
    HeaderIterator hi;
    char * val;
    size_t vallen;
    size_t alloced;
    int numTokens;
    int i;
} * headerSprintfArgs;

/**
 * Initialize an hsa iteration.
 * @param hsa		headerSprintf args
 */
static void hsaInit(headerSprintfArgs hsa)
{
    sprintfTag tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));

    hsa->i = 0;
    if (tag != NULL && tag->tag == -2)
	hsa->hi = headerInitIterator(hsa->h);
}

/**
 * Return next hsa iteration item.
 * @param hsa		headerSprintf args
 * @return		next sprintfToken (or NULL)
 */
static sprintfToken hsaNext(headerSprintfArgs hsa)
{
    sprintfToken fmt = NULL;
    sprintfTag tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));

    if (hsa->i >= 0 && hsa->i < hsa->numTokens) {
	fmt = hsa->format + hsa->i;
	if (hsa->hi == NULL) {
	    hsa->i++;
	} else {
	    rpmTag tagno;
	    rpmTagType type;
	    rpm_count_t count;

	    if (!headerNextIterator(hsa->hi, &tagno, &type, NULL, &count))
		fmt = NULL;
	    tag->tag = tagno;
	}
    }

    return fmt;
}

/**
 * Finish an hsa iteration.
 * @param hsa		headerSprintf args
 */
static void hsaFini(headerSprintfArgs hsa)
{
    hsa->hi = headerFreeIterator(hsa->hi);
    hsa->i = 0;
}

/**
 * Reserve sufficient buffer space for next output value.
 * @param hsa		headerSprintf args
 * @param need		no. of bytes to reserve
 * @return		pointer to reserved space
 */
static char * hsaReserve(headerSprintfArgs hsa, size_t need)
{
    if ((hsa->vallen + need) >= hsa->alloced) {
	if (hsa->alloced <= need)
	    hsa->alloced += need;
	hsa->alloced <<= 1;
	hsa->val = xrealloc(hsa->val, hsa->alloced+1);	
    }
    return hsa->val + hsa->vallen;
}

/**
 * Return tag name from value.
 * @todo bsearch on sorted value table.
 * @param tbl		tag table
 * @param val		tag value to find
 * @return		tag name, NULL on not found
 */
static const char * myTagName(headerTagTableEntry tbl, int val)
{
    static char name[128];
    const char * s;
    char *t;

    for (; tbl->name != NULL; tbl++) {
	if (tbl->val == val)
	    break;
    }
    if ((s = tbl->name) == NULL)
	return NULL;
    s += sizeof("RPMTAG_") - 1;
    t = name;
    *t++ = *s++;
    while (*s != '\0')
	*t++ = rtolower(*s++);
    *t = '\0';
    return name;
}

/**
 * Return tag value from name.
 * @todo bsearch on sorted name table.
 * @param tbl		tag table
 * @param name		tag name to find
 * @return		tag value, 0 on not found
 */
static int myTagValue(headerTagTableEntry tbl, const char * name)
{
    for (; tbl->name != NULL; tbl++) {
	if (!rstrcasecmp(tbl->name + sizeof("RPMTAG"), name))
	    return tbl->val;
    }
    return 0;
}

/**
 * Search extensions and tags for a name.
 * @param hsa		headerSprintf args
 * @param token		parsed fields
 * @param name		name to find
 * @return		0 on success, 1 on not found
 */
static int findTag(headerSprintfArgs hsa, sprintfToken token, const char * name)
{
    const char *tagname = name;
    headerSprintfExtension ext;
    sprintfTag stag = (token->type == PTOK_COND
	? &token->u.cond.tag : &token->u.tag);

    stag->fmt = NULL;
    stag->ext = NULL;
    stag->extNum = 0;
    stag->tag = -1;

    if (!strcmp(tagname, "*")) {
	stag->tag = -2;
	goto bingo;
    }

    if (strncmp("RPMTAG_", tagname, sizeof("RPMTAG_")-1) == 0) {
	tagname += sizeof("RPMTAG");
    }

    /* Search extensions for specific tag override. */
    for (ext = hsa->exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_TAG)
	    continue;
	if (!rstrcasecmp(ext->name + sizeof("RPMTAG"), tagname)) {
	    stag->ext = ext->u.tagFunction;
	    stag->extNum = ext - hsa->exts;
	    goto bingo;
	}
    }

    /* Search tag names. */
    stag->tag = myTagValue(hsa->tags, tagname);
    if (stag->tag != 0)
	goto bingo;

    return 1;

bingo:
    /* Search extensions for specific format. */
    if (stag->type != NULL)
    for (ext = hsa->exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	    ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_FORMAT)
	    continue;
	if (!strcmp(ext->name, stag->type)) {
	    stag->fmt = ext->u.formatFunction;
	    break;
	}
    }
    return 0;
}

/* forward ref */
/**
 * Parse an expression.
 * @param hsa		headerSprintf args
 * @param token		token
 * @param str		string
 * @param[out] *endPtr
 * @return		0 on success
 */
static int parseExpression(headerSprintfArgs hsa, sprintfToken token,
		char * str,char ** endPtr);

/**
 * Parse a headerSprintf term.
 * @param hsa		headerSprintf args
 * @param str
 * @retval *formatPtr
 * @retval *numTokensPtr
 * @retval *endPtr
 * @param state
 * @return		0 on success
 */
static int parseFormat(headerSprintfArgs hsa, char * str,
	sprintfToken * formatPtr,int * numTokensPtr,
	char ** endPtr, int state)
{
    char * chptr, * start, * next, * dst;
    sprintfToken format;
    sprintfToken token;
    int numTokens;
    int i;
    int done = 0;

    /* upper limit on number of individual formats */
    numTokens = 0;
    if (str != NULL)
    for (chptr = str; *chptr != '\0'; chptr++)
	if (*chptr == '%') numTokens++;
    numTokens = numTokens * 2 + 1;

    format = xcalloc(numTokens, sizeof(*format));
    if (endPtr) *endPtr = NULL;

    /* LCL: can't detect done termination */
    dst = start = str;
    numTokens = 0;
    token = NULL;
    if (start != NULL)
    while (*start != '\0') {
	switch (*start) {
	case '%':
	    /* handle %% */
	    if (*(start + 1) == '%') {
		if (token == NULL || token->type != PTOK_STRING) {
		    token = format + numTokens++;
		    token->type = PTOK_STRING;
		    dst = token->u.string.string = start;
		}
		start++;
		*dst++ = *start++;
		break;
	    } 

	    token = format + numTokens++;
	    *dst++ = '\0';
	    start++;

	    if (*start == '|') {
		char * newEnd;

		start++;
		if (parseExpression(hsa, token, start, &newEnd))
		{
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		start = newEnd;
		break;
	    }

	    token->u.tag.format = start;
	    token->u.tag.pad = 0;
	    token->u.tag.justOne = 0;
	    token->u.tag.arrayCount = 0;

	    chptr = start;
	    while (*chptr && *chptr != '{' && *chptr != '%') chptr++;
	    if (!*chptr || *chptr == '%') {
		hsa->errmsg = _("missing { after %");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    *chptr++ = '\0';

	    while (start < chptr) {
		if (risdigit(*start)) {
		    i = strtoul(start, &start, 10);
		    token->u.tag.pad += i;
		    start = chptr;
		    break;
		} else {
		    start++;
		}
	    }

	    if (*start == '=') {
		token->u.tag.justOne = 1;
		start++;
	    } else if (*start == '#') {
		token->u.tag.justOne = 1;
		token->u.tag.arrayCount = 1;
		start++;
	    }

	    dst = next = start;
	    while (*next && *next != '}') next++;
	    if (!*next) {
		hsa->errmsg = _("missing } after %{");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *next++ = '\0';

	    chptr = start;
	    while (*chptr && *chptr != ':') chptr++;

	    if (*chptr != '\0') {
		*chptr++ = '\0';
		if (!*chptr) {
		    hsa->errmsg = _("empty tag format");
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		token->u.tag.type = chptr;
	    } else {
		token->u.tag.type = NULL;
	    }
	    
	    if (!*start) {
		hsa->errmsg = _("empty tag name");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    i = 0;
	    token->type = PTOK_TAG;

	    if (findTag(hsa, token, start)) {
		hsa->errmsg = _("unknown tag");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    start = next;
	    break;

	case '[':
	    *dst++ = '\0';
	    *start++ = '\0';
	    token = format + numTokens++;

	    if (parseFormat(hsa, start,
			    &token->u.array.format,
			    &token->u.array.numTokens,
			    &start, PARSER_IN_ARRAY))
	    {
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    if (!start) {
		hsa->errmsg = _("] expected at end of array");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    dst = start;

	    token->type = PTOK_ARRAY;

	    break;

	case ']':
	    if (state != PARSER_IN_ARRAY) {
		hsa->errmsg = _("unexpected ]");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    break;

	case '}':
	    if (state != PARSER_IN_EXPR) {
		hsa->errmsg = _("unexpected }");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    break;

	default:
	    if (token == NULL || token->type != PTOK_STRING) {
		token = format + numTokens++;
		token->type = PTOK_STRING;
		dst = token->u.string.string = start;
	    }

	    if (*start == '\\') {
		start++;
		*dst++ = escapedChar(*start++);
	    } else {
		*dst++ = *start++;
	    }
	    break;
	}
	if (done)
	    break;
    }

    if (dst != NULL)
        *dst = '\0';

    for (i = 0; i < numTokens; i++) {
	token = format + i;
	if (token->type == PTOK_STRING)
	    token->u.string.len = strlen(token->u.string.string);
    }

    *numTokensPtr = numTokens;
    *formatPtr = format;

    return 0;
}

static int parseExpression(headerSprintfArgs hsa, sprintfToken token,
		char * str, char ** endPtr)
{
    char * chptr;
    char * end;

    hsa->errmsg = NULL;
    chptr = str;
    while (*chptr && *chptr != '?') chptr++;

    if (*chptr != '?') {
	hsa->errmsg = _("? expected in expression");
	return 1;
    }

    *chptr++ = '\0';;

    if (*chptr != '{') {
	hsa->errmsg = _("{ expected after ? in expression");
	return 1;
    }

    chptr++;

    if (parseFormat(hsa, chptr, &token->u.cond.ifFormat, 
		    &token->u.cond.numIfTokens, &end, PARSER_IN_EXPR)) 
	return 1;

    /* XXX fix segfault on "rpm -q rpm --qf='%|NAME?{%}:{NAME}|\n'"*/
    if (!(end && *end)) {
	hsa->errmsg = _("} expected in expression");
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    chptr = end;
    if (*chptr != ':' && *chptr != '|') {
	hsa->errmsg = _(": expected following ? subexpression");
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    if (*chptr == '|') {
	if (parseFormat(hsa, NULL, &token->u.cond.elseFormat, 
		&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR))
	{
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}
    } else {
	chptr++;

	if (*chptr != '{') {
	    hsa->errmsg = _("{ expected after : in expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr++;

	if (parseFormat(hsa, chptr, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR)) 
	    return 1;

	/* XXX fix segfault on "rpm -q rpm --qf='%|NAME?{a}:{%}|{NAME}\n'" */
	if (!(end && *end)) {
	    hsa->errmsg = _("} expected in expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr = end;
	if (*chptr != '|') {
	    hsa->errmsg = _("| expected at end of expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.elseFormat =
		freeFormat(token->u.cond.elseFormat, token->u.cond.numElseTokens);
	    return 1;
	}
    }
	
    chptr++;

    *endPtr = chptr;

    token->type = PTOK_COND;

    (void) findTag(hsa, token, str);

    return 0;
}

/**
 * Call a header extension only once, saving results.
 * @param hsa		headerSprintf args
 * @param fn
 * @retval *typeptr
 * @retval *data
 * @retval *countptr
 * @retval ec		extension cache
 * @return		0 on success, 1 on failure
 */
static int getExtension(headerSprintfArgs hsa, headerTagTagFunction fn,
		rpmTagType * typeptr,
		rpm_data_t * data,
		rpm_count_t * countptr,
		rpmec ec)
{
    if (!ec->avail) {
	if (fn(hsa->h, &ec->type, &ec->data, &ec->count, &ec->freeit))
	    return 1;
	ec->avail = 1;
    }

    if (typeptr) *typeptr = ec->type;
    if (data) *data = ec->data;
    if (countptr) *countptr = ec->count;

    return 0;
}

/**
 * formatValue
 * @param hsa		headerSprintf args
 * @param tag
 * @param element
 * @return		end of formatted string (NULL on error)
 */
static char * formatValue(headerSprintfArgs hsa, sprintfTag tag, int element)
{
    char * val = NULL;
    size_t need = 0;
    char * t, * te;
    char buf[20];
    rpmTagType type;
    rpm_count_t count;
    rpm_data_t data;
    unsigned int intVal;
    const char ** strarray;
    int datafree = 0;
    int countBuf;

    memset(buf, 0, sizeof(buf));
    if (tag->ext) {
	if (getExtension(hsa, tag->ext, &type, &data, &count, hsa->ec + tag->extNum))
	{
	    count = 1;
	    type = RPM_STRING_TYPE;	
	    data = "(none)";
	}
    } else {
	if (!headerGetEntry(hsa->h, tag->tag, &type, &data, &count)) {
	    count = 1;
	    type = RPM_STRING_TYPE;	
	    data = "(none)";
	}

	/* XXX this test is unnecessary, array sizes are checked */
	switch (type) {
	default:
	    if (element >= count) {
		data = headerFreeData(data, type);

		hsa->errmsg = _("(index out of range)");
		return NULL;
	    }
	    break;
	case RPM_BIN_TYPE:
	case RPM_STRING_TYPE:
	    break;
	}
	datafree = 1;
    }

    if (tag->arrayCount) {
	if (datafree)
	    data = headerFreeData(data, type);

	countBuf = count;
	data = &countBuf;
	count = 1;
	type = RPM_INT32_TYPE;
    }

    (void) stpcpy( stpcpy(buf, "%"), tag->format);

    if (data)
    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
	strarray = (const char **)data;

	if (tag->fmt)
	    val = tag->fmt(RPM_STRING_TYPE, strarray[element], buf, tag->pad, 
			   (rpm_count_t) element);

	if (val) {
	    need = strlen(val);
	} else {
	    need = strlen(strarray[element]) + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "s");
	    sprintf(val, buf, strarray[element]);
	}

	break;

    case RPM_STRING_TYPE:
	if (tag->fmt)
	    val = tag->fmt(RPM_STRING_TYPE, data, buf, tag->pad,  0);

	if (val) {
	    need = strlen(val);
	} else {
	    need = strlen(data) + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "s");
	    sprintf(val, buf, data);
	}
	break;

    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
	switch (type) {
	case RPM_CHAR_TYPE:	
	case RPM_INT8_TYPE:
	    intVal = *(((int8_t *) data) + element);
	    break;
	case RPM_INT16_TYPE:
	    intVal = *(((uint16_t *) data) + element);
	    break;
	default:		/* keep -Wall quiet */
	case RPM_INT32_TYPE:
	    intVal = *(((int32_t *) data) + element);
	    break;
	}

	if (tag->fmt)
	    val = tag->fmt(RPM_INT32_TYPE, &intVal, buf, tag->pad, 
			   (rpm_count_t) element);

	if (val) {
	    need = strlen(val);
	} else {
	    need = 10 + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "d");
	    sprintf(val, buf, intVal);
	}
	break;

    case RPM_BIN_TYPE:
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	if (tag->fmt)
	    val = tag->fmt(RPM_BIN_TYPE, data, buf, tag->pad, (rpm_count_t) count);

	if (val) {
	    need = strlen(val);
	} else {
	    val = pgpHexStr(data, count);
	    need = strlen(val) + tag->pad;
	}
	break;

    default:
	need = sizeof("(unknown type)") - 1;
	val = xstrdup("(unknown type)");
	break;
    }

    if (datafree)
	data = headerFreeData(data, type);

    if (val && need > 0) {
	t = hsaReserve(hsa, need);
	te = stpcpy(t, val);
	hsa->vallen += (te - t);
	val = _free(val);
    }

    return (hsa->val + hsa->vallen);
}

/**
 * Format a single headerSprintf item.
 * @param hsa		headerSprintf args
 * @param token
 * @param element
 * @return		end of formatted string (NULL on error)
 */
static char * singleSprintf(headerSprintfArgs hsa, sprintfToken token,
		int element)
{
    char * t, * te;
    int i, j, found;
    rpmTagType type;
    rpm_count_t count, numElements;
    sprintfToken spft;
    int condNumFormats;
    size_t need;

    /* we assume the token and header have been validated already! */

    switch (token->type) {
    case PTOK_NONE:
	break;

    case PTOK_STRING:
	need = token->u.string.len;
	if (need == 0) break;
	t = hsaReserve(hsa, need);
	te = stpcpy(t, token->u.string.string);
	hsa->vallen += (te - t);
	break;

    case PTOK_TAG:
	t = hsa->val + hsa->vallen;
	te = formatValue(hsa, &token->u.tag,
			(token->u.tag.justOne ? 0 : element));
	if (te == NULL)
	    return NULL;
	break;

    case PTOK_COND:
	if (token->u.cond.tag.ext || headerIsEntry(hsa->h, token->u.cond.tag.tag)) {
	    spft = token->u.cond.ifFormat;
	    condNumFormats = token->u.cond.numIfTokens;
	} else {
	    spft = token->u.cond.elseFormat;
	    condNumFormats = token->u.cond.numElseTokens;
	}

	need = condNumFormats * 20;
	if (spft == NULL || need == 0) break;

	t = hsaReserve(hsa, need);
	for (i = 0; i < condNumFormats; i++, spft++) {
	    te = singleSprintf(hsa, spft, element);
	    if (te == NULL)
		return NULL;
	}
	break;

    case PTOK_ARRAY:
	numElements = 0;
	found = 0;
	spft = token->u.array.format;
	for (i = 0; i < token->u.array.numTokens; i++, spft++)
	{
	    if (spft->type != PTOK_TAG ||
		spft->u.tag.arrayCount ||
		spft->u.tag.justOne) continue;

	    if (spft->u.tag.ext) {
		if (getExtension(hsa, spft->u.tag.ext, &type, NULL, &count, 
				 hsa->ec + spft->u.tag.extNum))
		     continue;
	    } else {
		if (!headerGetEntry(hsa->h, spft->u.tag.tag, &type, NULL, &count))
		    continue;
	    } 

	    found = 1;

	    if (type == RPM_BIN_TYPE)
		count = 1;	/* XXX count abused as no. of bytes. */

	    if (numElements > 1 && count != numElements)
	    switch (type) {
	    default:
		hsa->errmsg =
			_("array iterator used with different sized arrays");
		return NULL;
		break;
	    case RPM_BIN_TYPE:
	    case RPM_STRING_TYPE:
		break;
	    }
	    if (count > numElements)
		numElements = count;
	}

	if (! found) {
	    need = sizeof("(none)") - 1;
	    t = hsaReserve(hsa, need);
	    te = stpcpy(t, "(none)");
	    hsa->vallen += (te - t);
	} else {
	    int isxml;

	    need = numElements * token->u.array.numTokens * 10;
	    if (need == 0) break;

	    spft = token->u.array.format;
	    isxml = (spft->type == PTOK_TAG && spft->u.tag.type != NULL &&
		!strcmp(spft->u.tag.type, "xml"));

	    if (isxml) {
		const char * tagN = myTagName(hsa->tags, spft->u.tag.tag);

		need = sizeof("  <rpmTag name=\"\">\n") - 1;
		if (tagN != NULL)
		    need += strlen(tagN);
		t = hsaReserve(hsa, need);
		te = stpcpy(t, "  <rpmTag name=\"");
		if (tagN != NULL)
		    te = stpcpy(te, tagN);
		te = stpcpy(te, "\">\n");
		hsa->vallen += (te - t);
	    }

	    t = hsaReserve(hsa, need);
	    for (j = 0; j < numElements; j++) {
		spft = token->u.array.format;
		for (i = 0; i < token->u.array.numTokens; i++, spft++) {
		    te = singleSprintf(hsa, spft, j);
		    if (te == NULL)
			return NULL;
		}
	    }

	    if (isxml) {
		need = sizeof("  </rpmTag>\n") - 1;
		t = hsaReserve(hsa, need);
		te = stpcpy(t, "  </rpmTag>\n");
		hsa->vallen += (te - t);
	    }

	}
	break;
    }

    return (hsa->val + hsa->vallen);
}

/**
 * Create an extension cache.
 * @param exts		headerSprintf extensions
 * @return		new extension cache
 */
static rpmec
rpmecNew(const headerSprintfExtension exts)
{
    headerSprintfExtension ext;
    rpmec ec;
    int i = 0;

    for (ext = exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	i++;
    }

    ec = xcalloc(i, sizeof(*ec));
    return ec;
}

/**
 * Destroy an extension cache.
 * @param exts		headerSprintf extensions
 * @param ec		extension cache
 * @return		NULL always
 */
static rpmec
rpmecFree(const headerSprintfExtension exts, rpmec ec)
{
    headerSprintfExtension ext;
    int i = 0;

    for (ext = exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	if (ec[i].freeit) ec[i].data = _free(ec[i].data);
	i++;
    }

    ec = _free(ec);
    return NULL;
}

char * headerSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tbltags,
		     const struct headerSprintfExtension_s * extensions,
		     errmsg_t * errmsg)
{
    struct headerSprintfArgs_s hsa;
    sprintfToken nextfmt;
    sprintfTag tag;
    char * t, * te;
    int isxml;
    size_t need;
 
    memset(&hsa, 0, sizeof(hsa));
    hsa.h = headerLink(h);
    hsa.fmt = xstrdup(fmt);
    hsa.exts = (headerSprintfExtension) extensions;
    hsa.tags = (headerTagTableEntry) tbltags;
    hsa.errmsg = NULL;

    if (parseFormat(&hsa, hsa.fmt, &hsa.format, &hsa.numTokens, NULL, PARSER_BEGIN))
	goto exit;

    hsa.ec = rpmecNew(hsa.exts);
    hsa.val = xstrdup("");

    tag =
	(hsa.format->type == PTOK_TAG
	    ? &hsa.format->u.tag :
	(hsa.format->type == PTOK_ARRAY
	    ? &hsa.format->u.array.format->u.tag :
	NULL));
    isxml = (tag != NULL && tag->tag == -2 && tag->type != NULL && !strcmp(tag->type, "xml"));

    if (isxml) {
	need = sizeof("<rpmHeader>\n") - 1;
	t = hsaReserve(&hsa, need);
	te = stpcpy(t, "<rpmHeader>\n");
	hsa.vallen += (te - t);
    }

    hsaInit(&hsa);
    while ((nextfmt = hsaNext(&hsa)) != NULL) {
	te = singleSprintf(&hsa, nextfmt, 0);
	if (te == NULL) {
	    hsa.val = _free(hsa.val);
	    break;
	}
    }
    hsaFini(&hsa);

    if (isxml) {
	need = sizeof("</rpmHeader>\n") - 1;
	t = hsaReserve(&hsa, need);
	te = stpcpy(t, "</rpmHeader>\n");
	hsa.vallen += (te - t);
    }

    if (hsa.val != NULL && hsa.vallen < hsa.alloced)
	hsa.val = xrealloc(hsa.val, hsa.vallen+1);	

    hsa.ec = rpmecFree(hsa.exts, hsa.ec);
    hsa.format = freeFormat(hsa.format, hsa.numTokens);

exit:
    if (errmsg)
	*errmsg = hsa.errmsg;
    hsa.h = headerFree(hsa.h);
    hsa.fmt = _free(hsa.fmt);
    return hsa.val;
}

/**
 * octalFormat.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @param element	(unused)
 * @return		formatted string
 */
static char * octalFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding,int element)
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(20 + padding);
	strcat(formatPrefix, "o");
	sprintf(val, formatPrefix, *((const int32_t *) data));
    }

    return val;
}

/**
 * hexFormat.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @param element	(unused)
 * @return		formatted string
 */
static char * hexFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding,int element)
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(20 + padding);
	strcat(formatPrefix, "x");
	sprintf(val, formatPrefix, *((const int32_t *) data));
    }

    return val;
}

/**
 */
static char * realDateFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding,int element,
		const char * strftimeFormat)
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	struct tm * tstruct;
	char buf[50];

	val = xmalloc(50 + padding);
	strcat(formatPrefix, "s");

	/* this is important if sizeof(rpm_time_t) ! sizeof(time_t) */
	{   time_t dateint = *((const rpm_time_t *) data);
	    tstruct = localtime(&dateint);
	}
	buf[0] = '\0';
	if (tstruct)
	    (void) strftime(buf, sizeof(buf) - 1, strftimeFormat, tstruct);
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

/**
 * Format a date.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @param element	(unused)
 * @return		formatted string
 */
static char * dateFormat(rpmTagType type, rpm_constdata_t data, 
		         char * formatPrefix, size_t padding, int element)
{
    return realDateFormat(type, data, formatPrefix, padding, element,
			_("%c"));
}

/**
 * Format a day.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @param element	(unused)
 * @return		formatted string
 */
static char * dayFormat(rpmTagType type, rpm_constdata_t data, 
		         char * formatPrefix, size_t padding, int element)
{
    return realDateFormat(type, data, formatPrefix, padding, element, 
			  _("%a %b %d %Y"));
}

/**
 * Return shell escape formatted data.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @param element	(unused)
 * @return		formatted string
 */
static char * shescapeFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding,int element)
{
    char * result, * dst, * src;

    if (type == RPM_INT32_TYPE) {
	result = xmalloc(padding + 20);
	strcat(formatPrefix, "d");
	sprintf(result, formatPrefix, *((const int32_t *) data));
    } else {
	char *buf = NULL;
	strcat(formatPrefix, "s");
	rasprintf(&buf, formatPrefix, data);

	result = dst = xmalloc(strlen(buf) * 4 + 3);
	*dst++ = '\'';
	for (src = buf; *src != '\0'; src++) {
	    if (*src == '\'') {
		*dst++ = '\'';
		*dst++ = '\\';
		*dst++ = '\'';
		*dst++ = '\'';
	    } else {
		*dst++ = *src;
	    }
	}
	*dst++ = '\'';
	*dst = '\0';
	free(buf);
    }

    return result;
}

/* FIX: cast? */
const struct headerSprintfExtension_s headerDefaultFormats[] = {
    { HEADER_EXT_FORMAT, "octal", { octalFormat } },
    { HEADER_EXT_FORMAT, "hex", { hexFormat } },
    { HEADER_EXT_FORMAT, "date", { dateFormat } },
    { HEADER_EXT_FORMAT, "day", { dayFormat } },
    { HEADER_EXT_FORMAT, "shescape", { shescapeFormat } },
    { HEADER_EXT_LAST, NULL, { NULL } }
};

void headerCopyTags(Header headerFrom, Header headerTo, 
		    const rpmTag * tagstocopy)
{
    const rpmTag * p;

    if (headerFrom == headerTo)
	return;

    for (p = tagstocopy; *p != 0; p++) {
	rpm_data_t s;
	rpmTagType type;
	rpm_count_t count;
	if (headerIsEntry(headerTo, *p))
	    continue;
	if (!headerGetEntryMinMemory(headerFrom, *p, &type, &s, &count))
	    continue;
	(void) headerAddEntry(headerTo, *p, type, s, count);
	/* XXXX freeing up *any* data from headerGetEntryMinMemory ?! */
	s = headerFreeData(s, type);
    }
}

void * headerFreeData(rpm_data_t data, rpmTagType type)
{
    if (data) {
	if (type == RPM_FORCEFREE_TYPE ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		free(data); /* XXX _free() */
    }
    return NULL;
}
