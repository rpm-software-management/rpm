/** \ingroup header
 * \file lib/header.c
 */

/* RPM - Copyright (C) 1995-2002 Red Hat Software */

/* Data written to file descriptors is in network byte order.    */
/* Data read from file descriptors is expected to be in          */
/* network byte order and is converted on the fly to host order. */

#include "system.h"

#define	__HEADER_PROTOTYPES__

#include <header_internal.h>

#include "debug.h"

/*@-redecl@*/	/* FIX: avoid rpmlib.h, need for debugging. */
/*@observer@*/ const char *const tagName(int tag)	/*@*/;
/*@=redecl@*/

/*@access entryInfo @*/
/*@access indexEntry @*/

/*@access extensionCache @*/
/*@access sprintfTag @*/
/*@access sprintfToken @*/
/*@access HV_t @*/

#define PARSER_BEGIN 	0
#define PARSER_IN_ARRAY 1
#define PARSER_IN_EXPR  2

/*@unchecked@*/
static int _h_debug = 0;

/** \ingroup header
 */
/*@observer@*/ /*@unchecked@*/
static unsigned char header_magic[8] = {
	0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/** \ingroup header
 * Maximum no. of bytes permitted in a header.
 */
/*@unchecked@*/
static size_t headerMaxbytes = (32*1024*1024);

/**
 * Sanity check on no. of tags.
 * This check imposes a limit of 65K tags, more than enough.
 */ 
#define hdrchkTags(_ntags)	((_ntags) & 0xffff0000)

/**
 * Sanity check on data size and/or offset.
 * This check imposes a limit of 16Mb, more than enough.
 */ 
#define hdrchkData(_nbytes)	((_nbytes) & 0xff000000)

/** \ingroup header
 * Alignment needs (and sizeof scalars types) for internal rpm data types.
 */
/*@observer@*/ /*@unchecked@*/
static int typeSizes[] =  { 
	0,	/*!< RPM_NULL_TYPE */
	1,	/*!< RPM_CHAR_TYPE */
	1,	/*!< RPM_INT8_TYPE */
	2,	/*!< RPM_INT16_TYPE */
	4,	/*!< RPM_INT32_TYPE */
	-1,	/*!< RPM_INT64_TYPE */
	-1,	/*!< RPM_STRING_TYPE */
	1,	/*!< RPM_BIN_TYPE */
	-1,	/*!< RPM_STRING_ARRAY_TYPE */
	-1	/*!< RPM_I18NSTRING_TYPE */
};

/*@observer@*/ /*@unchecked@*/
HV_t hdrVec;	/* forward reference */

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p) /*@modifies *p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
static
Header XheaderLink(Header h, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
	/*@modifies h @*/
{
    if (h != NULL) h->nrefs++;
/*@-modfilesystem@*/
if (_h_debug > 0 && msg != NULL)
fprintf(stderr, "--> h  %p ++ %d %s at %s:%u\n", h, (h != NULL ? h->nrefs : 0), msg, fn, ln);
/*@=modfilesystem@*/
    /*@-refcounttrans -nullret @*/
    return h;
    /*@=refcounttrans =nullret @*/
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
static /*@null@*/
Header XheaderUnlink(/*@killref@*/ /*@null@*/ Header h,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies h @*/
{
/*@-modfilesystem@*/
if (_h_debug > 0 && msg != NULL)
fprintf(stderr, "--> h  %p -- %d %s at %s:%u\n", h, (h != NULL ? h->nrefs : 0), msg, fn, ln);
/*@=modfilesystem@*/
    if (h != NULL) h->nrefs--;
    return NULL;
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
static /*@null@*/
Header XheaderFree(/*@killref@*/ /*@null@*/ Header h,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies h @*/
{
    (void) XheaderUnlink(h, msg, fn, ln);

    /*@-usereleased@*/
    if (h == NULL || h->nrefs > 0)
	return NULL;	/* XXX return previous header? */

    if (h->index) {
	indexEntry entry = h->index;
	int i;
	for (i = 0; i < h->indexUsed; i++, entry++) {
	    if ((h->flags & HEADERFLAG_ALLOCATED) && ENTRY_IS_REGION(entry)) {
		if (entry->length > 0) {
		    int_32 * ei = entry->data;
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

    /*@-refcounttrans@*/ h = _free(h); /*@=refcounttrans@*/
    return h;
    /*@=usereleased@*/
}

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
static
Header headerNew(void)
	/*@*/
{
    Header h = xcalloc(1, sizeof(*h));

    /*@-assignexpose@*/
    h->hv = *hdrVec;		/* structure assignment */
    /*@=assignexpose@*/
    h->blob = NULL;
    h->indexAlloced = INDEX_MALLOC_SIZE;
    h->indexUsed = 0;
    h->flags = HEADERFLAG_SORTED;

    h->index = (h->indexAlloced
	? xcalloc(h->indexAlloced, sizeof(*h->index))
	: NULL);

    /*@-globstate -observertrans @*/
    h->nrefs = 0;
    return headerLink(h, "headerNew");
    /*@=globstate =observertrans @*/
}

/**
 */
static int indexCmp(const void * avp, const void * bvp)	/*@*/
{
    /*@-castexpose@*/
    indexEntry ap = (indexEntry) avp, bp = (indexEntry) bvp;
    /*@=castexpose@*/
    return (ap->info.tag - bp->info.tag);
}

/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
static
void headerSort(Header h)
	/*@modifies h @*/
{
    if (!(h->flags & HEADERFLAG_SORTED)) {
	qsort(h->index, h->indexUsed, sizeof(*h->index), indexCmp);
	h->flags |= HEADERFLAG_SORTED;
    }
}

/**
 */
static int offsetCmp(const void * avp, const void * bvp) /*@*/
{
    /*@-castexpose@*/
    indexEntry ap = (indexEntry) avp, bp = (indexEntry) bvp;
    /*@=castexpose@*/
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
static
void headerUnsort(Header h)
	/*@modifies h @*/
{
    qsort(h->index, h->indexUsed, sizeof(*h->index), offsetCmp);
}

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
static
unsigned int headerSizeof(/*@null@*/ Header h, enum hMagic magicp)
	/*@modifies h @*/
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

    /*@-sizeoftype@*/
    size += 2 * sizeof(int_32);	/* count of index entries */
    /*@=sizeoftype@*/

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	unsigned diff;
	int_32 type;

	/* Regions go in as is ... */
        if (ENTRY_IS_REGION(entry)) {
	    size += entry->length;
	    /* XXX Legacy regions do not include the region tag and data. */
	    /*@-sizeoftype@*/
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY))
		size += sizeof(struct entryInfo) + entry->info.count;
	    /*@=sizeoftype@*/
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

	/*@-sizeoftype@*/
	size += sizeof(struct entryInfo) + entry->length;
	/*@=sizeoftype@*/
    }

    return size;
}

/**
 * Return length of entry data.
 * @todo Remove sanity check exit's.
 * @param type		entry data type
 * @param p		entry data
 * @param count		entry item count
 * @param onDisk	data is concatenated strings (with NUL's))?
 * @return		no. bytes in data
 */
/*@mayexit@*/
static int dataLength(int_32 type, hPTR_t p, int_32 count, int onDisk)
	/*@*/
{
    int length = 0;

    switch (type) {
    case RPM_STRING_TYPE:
	if (count == 1) {	/* Special case -- p is just the string */
	    length = strlen(p) + 1;
	    break;
	}
        /* This should not be allowed */
	/*@-modfilesys@*/
	fprintf(stderr, _("dataLength() RPM_STRING_TYPE count must be 1.\n"));
	/*@=modfilesys@*/
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;

    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	int i;

	/* This is like RPM_STRING_TYPE, except it's *always* an array */
	/* Compute sum of length of all strings, including null terminators */
	i = count;

	if (onDisk) {
	    const char * chptr = p;
	    int thisLen;

	    while (i--) {
		thisLen = strlen(chptr) + 1;
		length += thisLen;
		chptr += thisLen;
	    }
	} else {
	    const char ** src = (const char **)p;
	    while (i--) {
		/* add one for null termination */
		length += strlen(*src++) + 1;
	    }
	}
    }	break;

    default:
	if (typeSizes[type] != -1) {
	    length = typeSizes[type] * count;
	    break;
	}
	/*@-modfilesys@*/
	fprintf(stderr, _("Data type %d not supported\n"), (int) type);
	/*@=modfilesys@*/
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;
    }

    return length;
}

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
static int regionSwab(/*@null@*/ indexEntry entry, int il, int dl,
		entryInfo pe, char * dataStart, int regionid)
	/*@modifies *entry, *dataStart @*/
{
    char * tprev = NULL;
    char * t = NULL;
    int tdel, tl = dl;
    struct indexEntry ieprev;

    memset(&ieprev, 0, sizeof(ieprev));
    for (; il > 0; il--, pe++) {
	struct indexEntry ie;
	int_32 type;

	ie.info.tag = ntohl(pe->tag);
	ie.info.type = ntohl(pe->type);
	if (ie.info.type < RPM_MIN_TYPE || ie.info.type > RPM_MAX_TYPE)
	    return -1;
	ie.info.count = ntohl(pe->count);
	ie.info.offset = ntohl(pe->offset);
	ie.data = t = dataStart + ie.info.offset;
	ie.length = dataLength(ie.info.type, ie.data, ie.info.count, 1);
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
	    /*@-sizeoftype@*/
	    if (ie.info.tag == HEADER_IMAGE)
		tprev -= REGION_TAG_COUNT;
	    /*@=sizeoftype@*/
	}

	/* Perform endian conversions */
	switch (ntohl(pe->type)) {
	case RPM_INT32_TYPE:
	{   int_32 * it = (int_32 *)t;
	    for (; ie.info.count > 0; ie.info.count--, it += 1)
		*it = htonl(*it);
	    t = (char *) it;
	}   /*@switchbreak@*/ break;
	case RPM_INT16_TYPE:
	{   int_16 * it = (int_16 *) t;
	    for (; ie.info.count > 0; ie.info.count--, it += 1)
		*it = htons(*it);
	    t = (char *) it;
	}   /*@switchbreak@*/ break;
	default:
	    t += ie.length;
	    /*@switchbreak@*/ break;
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
    /*@-sizeoftype@*/
    if (tl+REGION_TAG_COUNT == dl)
	tl += REGION_TAG_COUNT;
    /*@=sizeoftype@*/

    return dl;
}

/** \ingroup header
 */
static /*@only@*/ /*@null@*/ void * doHeaderUnload(Header h,
		/*@out@*/ int * lengthPtr)
	/*@modifies h, *lengthPtr @*/
{
    int_32 * ei = NULL;
    entryInfo pe;
    char * dataStart;
    char * te;
    unsigned pad;
    unsigned len;
    int_32 il = 0;
    int_32 dl = 0;
    indexEntry entry; 
    int_32 type;
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
	    int_32 rdl = -entry->info.offset;	/* negative offset */
	    int_32 ril = rdl/sizeof(*pe);
	    int rid = entry->info.offset;

	    il += ril;
	    dl += entry->rdlen + entry->info.count;
	    /* XXX Legacy regions do not include the region tag and data. */
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY))
		il += 1;

	    /* Skip rest of entries in region, but account for dribbles. */
	    for (; i < h->indexUsed && entry->info.offset <= rid+1; i++, entry++) {
		if (entry->info.offset <= rid)
		    /*@innercontinue@*/ continue;

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
char *t;
	int count;
	int rdlen;

	if (entry->data == NULL || entry->length <= 0)
	    continue;

t = te;
	pe->tag = htonl(entry->info.tag);
	pe->type = htonl(entry->info.type);
	pe->count = htonl(entry->info.count);

	if (ENTRY_IS_REGION(entry)) {
	    int_32 rdl = -entry->info.offset;	/* negative offset */
	    int_32 ril = rdl/sizeof(*pe) + ndribbles;
	    int rid = entry->info.offset;

	    src = (char *)entry->data;
	    rdlen = entry->rdlen;

	    /* XXX Legacy regions do not include the region tag and data. */
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY)) {
		int_32 stei[4];

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

		count = regionSwab(NULL, ril, 0, pe, t, 0);
		if (count != rdlen)
		    goto errxit;

	    } else {

		memcpy(pe+1, src + sizeof(*pe), ((ril-1) * sizeof(*pe)));
		memcpy(te, src + (ril * sizeof(*pe)), rdlen+entry->info.count+drlen);
		te += rdlen;
		{   /*@-castexpose@*/
		    entryInfo se = (entryInfo)src;
		    /*@=castexpose@*/
		    int off = ntohl(se->offset);
		    pe->offset = (off) ? htonl(te - dataStart) : htonl(off);
		}
		te += entry->info.count + drlen;

		count = regionSwab(NULL, ril, 0, pe, t, 0);
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
		*((int_32 *)te) = htonl(*((int_32 *)src));
		/*@-sizeoftype@*/
		te += sizeof(int_32);
		src += sizeof(int_32);
		/*@=sizeoftype@*/
	    }
	    /*@switchbreak@*/ break;

	case RPM_INT16_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int_16 *)te) = htons(*((int_16 *)src));
		/*@-sizeoftype@*/
		te += sizeof(int_16);
		src += sizeof(int_16);
		/*@=sizeoftype@*/
	    }
	    /*@switchbreak@*/ break;

	default:
	    memcpy(te, entry->data, entry->length);
	    te += entry->length;
	    /*@switchbreak@*/ break;
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
    /*@-usereleased@*/
    ei = _free(ei);
    /*@=usereleased@*/
    return (void *) ei;
}

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header blob (i.e. with offsets)
 */
static /*@only@*/ /*@null@*/
void * headerUnload(Header h)
	/*@modifies h @*/
{
    int length;
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
static /*@null@*/
indexEntry findEntry(/*@null@*/ Header h, int_32 tag, int_32 type)
	/*@modifies h @*/
{
    indexEntry entry, entry2, last;
    struct indexEntry key;

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
    /*@-usereleased@*/ /* FIX: entry2 = entry. Code looks bogus as well. */
    while (entry2->info.tag == tag && entry2->info.type != type &&
	   entry2 < last) entry2++;
    /*@=usereleased@*/

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    return NULL;
}

/** \ingroup header
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
static
int headerRemoveEntry(Header h, int_32 tag)
	/*@modifies h @*/
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
	void * data;
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

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
static /*@null@*/
Header headerLoad(/*@kept@*/ void * uh)
	/*@modifies uh @*/
{
    int_32 * ei = (int_32 *) uh;
    int_32 il = ntohl(ei[0]);		/* index length */
    int_32 dl = ntohl(ei[1]);		/* data length */
    /*@-sizeoftype@*/
    size_t pvlen = sizeof(il) + sizeof(dl) +
               (il * sizeof(struct entryInfo)) + dl;
    /*@=sizeoftype@*/
    void * pv = uh;
    Header h = NULL;
    entryInfo pe;
    char * dataStart;
    indexEntry entry; 
    int rdlen;
    int i;

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl))
	goto errxit;

    ei = (int_32 *) pv;
    /*@-castexpose@*/
    pe = (entryInfo) &ei[2];
    /*@=castexpose@*/
    dataStart = (char *) (pe + il);

    h = xcalloc(1, sizeof(*h));
    /*@-assignexpose@*/
    h->hv = *hdrVec;		/* structure assignment */
    /*@=assignexpose@*/
    /*@-assignexpose -kepttrans@*/
    h->blob = uh;
    /*@=assignexpose =kepttrans@*/
    h->indexAlloced = il + 1;
    h->indexUsed = il;
    h->index = xcalloc(h->indexAlloced, sizeof(*h->index));
    h->flags = HEADERFLAG_SORTED;
    h->nrefs = 0;
    h = headerLink(h, "headerLoad");

    /*
     * XXX XFree86-libs, ash, and pdksh from Red Hat 5.2 have bogus
     * %verifyscript tag that needs to be diddled.
     */
    if (ntohl(pe->tag) == 15 &&
	ntohl(pe->type) == RPM_STRING_TYPE &&
	ntohl(pe->count) == 1)
    {
	pe->tag = htonl(1079);
    }

    entry = h->index;
    i = 0;
    if (!(htonl(pe->tag) < HEADER_I18NTABLE)) {
	h->flags |= HEADERFLAG_LEGACY;
	entry->info.type = REGION_TAG_TYPE;
	entry->info.tag = HEADER_IMAGE;
	/*@-sizeoftype@*/
	entry->info.count = REGION_TAG_COUNT;
	/*@=sizeoftype@*/
	entry->info.offset = ((char *)pe - dataStart); /* negative offset */

	/*@-assignexpose@*/
	entry->data = pe;
	/*@=assignexpose@*/
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, il, 0, pe, dataStart, entry->info.offset);
#if 0	/* XXX don't check, the 8/98 i18n bug fails here. */
	if (rdlen != dl)
	    goto errxit;
#endif
	entry->rdlen = rdlen;
	entry++;
	h->indexUsed++;
    } else {
	int nb = ntohl(pe->count);
	int_32 rdl;
	int_32 ril;

	h->flags &= ~HEADERFLAG_LEGACY;

	entry->info.type = htonl(pe->type);
	if (entry->info.type < RPM_MIN_TYPE || entry->info.type > RPM_MAX_TYPE)
	    goto errxit;
	entry->info.count = htonl(pe->count);

	if (hdrchkTags(entry->info.count))
	    goto errxit;

	{   int off = ntohl(pe->offset);

	    if (hdrchkData(off))
		goto errxit;
	    if (off) {
		int_32 * stei = memcpy(alloca(nb), dataStart + off, nb);
		rdl = -ntohl(stei[2]);	/* negative offset */
		ril = rdl/sizeof(*pe);
		if (hdrchkTags(ril) || hdrchkData(rdl))
		    goto errxit;
		entry->info.tag = htonl(pe->tag);
	    } else {
		ril = il;
		/*@-sizeoftype@*/
		rdl = (ril * sizeof(struct entryInfo));
		/*@=sizeoftype@*/
		entry->info.tag = HEADER_IMAGE;
	    }
	}
	entry->info.offset = -rdl;	/* negative offset */

	/*@-assignexpose@*/
	entry->data = pe;
	/*@=assignexpose@*/
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, ril-1, 0, pe+1, dataStart, entry->info.offset);
	if (rdlen < 0)
	    goto errxit;
	entry->rdlen = rdlen;

	if (ril < h->indexUsed) {
	    indexEntry newEntry = entry + ril;
	    int ne = (h->indexUsed - ril);
	    int rid = entry->info.offset+1;
	    int rc;

	    /* Load dribble entries from region. */
	    rc = regionSwab(newEntry, ne, 0, pe+ril, dataStart, rid);
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

    /*@-globstate -observertrans @*/
    return h;
    /*@=globstate =observertrans @*/

errxit:
    /*@-usereleased@*/
    if (h) {
	h->index = _free(h->index);
	/*@-refcounttrans@*/
	h = _free(h);
	/*@=refcounttrans@*/
    }
    /*@=usereleased@*/
    /*@-refcounttrans -globstate@*/
    return h;
    /*@=refcounttrans =globstate@*/
}

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
static /*@null@*/
Header headerReload(/*@only@*/ Header h, int tag)
	/*@modifies h @*/
{
    Header nh;
    int length;
    /*@-onlytrans@*/
    void * uh = doHeaderUnload(h, &length);

    h = headerFree(h, "headerReload");
    /*@=onlytrans@*/
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

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
static /*@null@*/
Header headerCopyLoad(const void * uh)
	/*@*/
{
    int_32 * ei = (int_32 *) uh;
    int_32 il = ntohl(ei[0]);		/* index length */
    int_32 dl = ntohl(ei[1]);		/* data length */
    /*@-sizeoftype@*/
    size_t pvlen = sizeof(il) + sizeof(dl) +
			(il * sizeof(struct entryInfo)) + dl;
    /*@=sizeoftype@*/
    void * nuh = NULL;
    Header h = NULL;

    /* Sanity checks on header intro. */
    /*@-branchstate@*/
    if (!(hdrchkTags(il) || hdrchkData(dl)) && pvlen < headerMaxbytes) {
	nuh = memcpy(xmalloc(pvlen), uh, pvlen);
	if ((h = headerLoad(nuh)) != NULL)
	    h->flags |= HEADERFLAG_ALLOCATED;
    }
    /*@=branchstate@*/
    /*@-branchstate@*/
    if (h == NULL)
	nuh = _free(nuh);
    /*@=branchstate@*/
    return h;
}

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
static /*@null@*/
Header headerRead(FD_t fd, enum hMagic magicp)
	/*@modifies fd @*/
{
    int_32 block[4];
    int_32 reserved;
    int_32 * ei = NULL;
    int_32 il;
    int_32 dl;
    int_32 magic;
    Header h = NULL;
    size_t len;
    int i;

    memset(block, 0, sizeof(block));
    i = 2;
    if (magicp == HEADER_MAGIC_YES)
	i += 2;

    /*@-type@*/ /* FIX: cast? */
    if (timedRead(fd, (char *)block, i*sizeof(*block)) != (i * sizeof(*block)))
	goto exit;
    /*@=type@*/

    i = 0;

    if (magicp == HEADER_MAGIC_YES) {
	magic = block[i++];
	if (memcmp(&magic, header_magic, sizeof(magic)))
	    goto exit;
	reserved = block[i++];
    }
    
    il = ntohl(block[i]);	i++;
    dl = ntohl(block[i]);	i++;

    /*@-sizeoftype@*/
    len = sizeof(il) + sizeof(dl) + (il * sizeof(struct entryInfo)) + dl;
    /*@=sizeoftype@*/

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl) || len > headerMaxbytes)
	goto exit;

    ei = xmalloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);
    len -= sizeof(il) + sizeof(dl);

    /*@-type@*/ /* FIX: cast? */
    if (timedRead(fd, (char *)&ei[2], len) != len)
	goto exit;
    /*@=type@*/
    
    h = headerLoad(ei);

exit:
    if (h) {
	if (h->flags & HEADERFLAG_ALLOCATED)
	    ei = _free(ei);
	h->flags |= HEADERFLAG_ALLOCATED;
    } else if (ei)
	ei = _free(ei);
    /*@-mustmod@*/	/* FIX: timedRead macro obscures annotation */
    return h;
    /*@-mustmod@*/
}

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @param magicp	prefix write with 8 bytes of (magic, 0)?
 * @return		0 on success, 1 on error
 */
static
int headerWrite(FD_t fd, /*@null@*/ Header h, enum hMagic magicp)
	/*@globals fileSystem @*/
	/*@modifies fd, h, fileSystem @*/
{
    ssize_t nb;
    int length;
    const void * uh;

    if (h == NULL)
	return 1;
    uh = doHeaderUnload(h, &length);
    if (uh == NULL)
	return 1;
    switch (magicp) {
    case HEADER_MAGIC_YES:
	/*@-sizeoftype@*/
	nb = Fwrite(header_magic, sizeof(char), sizeof(header_magic), fd);
	/*@=sizeoftype@*/
	if (nb != sizeof(header_magic))
	    goto exit;
	break;
    case HEADER_MAGIC_NO:
	break;
    }

    /*@-sizeoftype@*/
    nb = Fwrite(uh, sizeof(char), length, fd);
    /*@=sizeoftype@*/

exit:
    uh = _free(uh);
    return (nb == length ? 0 : 1);
}

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
static
int headerIsEntry(/*@null@*/Header h, int_32 tag)
	/*@*/
{
    /*@-mods@*/		/*@ FIX: h modified by sort. */
    return (findEntry(h, tag, RPM_NULL_TYPE) ? 1 : 0);
    /*@=mods@*/	
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
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c,
		int minMem)
	/*@modifies *type, *p, *c @*/
{
    int_32 count = entry->info.count;
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
	    int_32 * ei = ((int_32 *)entry->data) - 2;
	    /*@-castexpose@*/
	    entryInfo pe = (entryInfo) (ei + 2);
	    /*@=castexpose@*/
	    char * dataStart = (char *) (pe + ntohl(ei[0]));
	    int_32 rdl = -entry->info.offset;	/* negative offset */
	    int_32 ril = rdl/sizeof(*pe);

	    /*@-sizeoftype@*/
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
	    ei = (int_32 *) *p;
	    ei[0] = htonl(ril);
	    ei[1] = htonl(rdl);

	    /*@-castexpose@*/
	    pe = (entryInfo) memcpy(ei + 2, pe, (ril * sizeof(*pe)));
	    /*@=castexpose@*/

	    dataStart = (char *) memcpy(pe + ril, dataStart, rdl);
	    /*@=sizeoftype@*/

	    rc = regionSwab(NULL, ril, 0, pe, dataStart, 0);
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
	/*@fallthrough@*/
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	const char ** ptrEntry;
	/*@-sizeoftype@*/
	int tableSize = count * sizeof(char *);
	/*@=sizeoftype@*/
	char * t;
	int i;

	/*@-mods@*/
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
	/*@=mods@*/
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
 * @return		1 on match, 0 on no match
 */
static int headerMatchLocale(const char *td, const char *l, const char *le)
	/*@*/
{
    const char *fe;


#if 0
  { const char *s, *ll, *CC, *EE, *dd;
    char *lbuf, *t.

    /* Copy the buffer and parse out components on the fly. */
    lbuf = alloca(le - l + 1);
    for (s = l, ll = t = lbuf; *s; s++, t++) {
	switch (*s) {
	case '_':
	    *t = '\0';
	    CC = t + 1;
	    break;
	case '.':
	    *t = '\0';
	    EE = t + 1;
	    break;
	case '@':
	    *t = '\0';
	    dd = t + 1;
	    break;
	default:
	    *t = *s;
	    break;
	}
    }

    if (ll)	/* ISO language should be lower case */
	for (t = ll; *t; t++)	*t = tolower(*t);
    if (CC)	/* ISO country code should be upper case */
	for (t = CC; *t; t++)	*t = toupper(*t);

    /* There are a total of 16 cases to attempt to match. */
  }
#endif

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
	return 1;

    return 0;
}

/**
 * Return i18n string from header that matches locale.
 * @param h		header
 * @param entry		i18n string data
 * @return		matching i18n string (or 1st string if no match)
 */
/*@dependent@*/ /*@exposed@*/ static char *
headerFindI18NString(Header h, indexEntry entry)
	/*@*/
{
    const char *lang, *l, *le;
    indexEntry table;

    /* XXX Drepper sez' this is the order. */
    if ((lang = getenv("LANGUAGE")) == NULL &&
	(lang = getenv("LC_ALL")) == NULL &&
        (lang = getenv("LC_MESSAGES")) == NULL &&
	(lang = getenv("LANG")) == NULL)
	    return entry->data;
    
    /*@-mods@*/
    if ((table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE)) == NULL)
	return entry->data;
    /*@=mods@*/

    for (l = lang; *l != '\0'; l = le) {
	const char *td;
	char *ed;
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

		if (headerMatchLocale(td, l, le))
		    return ed;

	}
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
static int intGetEntry(Header h, int_32 tag,
		/*@null@*/ /*@out@*/ hTAG_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c,
		int minMem)
	/*@modifies *type, *p, *c @*/
{
    indexEntry entry;
    int rc;

    /* First find the tag */
    /*@-mods@*/		/*@ FIX: h modified by sort. */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    /*@mods@*/
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
	/*@-dependenttrans@*/
	if (p) *p = headerFindI18NString(h, entry);
	/*@=dependenttrans@*/
	break;
    default:
	rc = copyEntry(entry, type, p, c, minMem);
	break;
    }

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
static /*@null@*/ void * headerFreeTag(/*@unused@*/ Header h,
		/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/
{
    if (data) {
	/*@-branchstate@*/
	if (type == -1 ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
		data = _free(data);
	/*@=branchstate@*/
    }
    return NULL;
}

/** \ingroup header
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
static
int headerGetEntry(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ void ** p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
{
    return intGetEntry(h, tag, type, (hPTR_t *)p, c, 0);
}

/** \ingroup header
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
static
int headerGetEntryMinMemory(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hPTR_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
{
    return intGetEntry(h, tag, type, p, c, 1);
}

int headerGetRawEntry(Header h, int_32 tag, int_32 * type, hPTR_t * p,
		int_32 * c)
{
    indexEntry entry;
    int rc;

    if (p == NULL) return headerIsEntry(h, tag);

    /* First find the tag */
    /*@-mods@*/		/*@ FIX: h modified by sort. */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    /*@=mods@*/
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
static void copyData(int_32 type, /*@out@*/ void * dstPtr, const void * srcPtr,
		int_32 c, int dataLength)
	/*@modifies *dstPtr @*/
{
    const char ** src;
    char * dst;
    int i;

    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
	/* Otherwise, p is char** */
	i = c;
	src = (const char **) srcPtr;
	dst = dstPtr;
	while (i--) {
	    if (*src) {
		int len = strlen(*src) + 1;
		memcpy(dst, *src, len);
		dst += len;
	    }
	    src++;
	}
	break;

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
 * @return 		(malloc'ed) copy of entry data
 */
static void * grabData(int_32 type, hPTR_t p, int_32 c,
		/*@out@*/ int * lengthPtr)
	/*@modifies *lengthPtr @*/
{
    int length = dataLength(type, p, c, 0);
    void * data = xmalloc(length);

    copyData(type, data, p, c, length);

    if (lengthPtr)
	*lengthPtr = length;
    return data;
}

/** \ingroup header
 * Add tag to header.
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerAddEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c)
	/*@modifies h @*/
{
    indexEntry entry;

    /* Count must always be >= 1 for headerAddEntry. */
    if (c <= 0)
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
    entry->data = grabData(type, p, c, &entry->length);

    if (h->indexUsed > 0 && tag < h->index[h->indexUsed-1].info.tag)
	h->flags &= ~HEADERFLAG_SORTED;
    h->indexUsed++;

    return 1;
}

/** \ingroup header
 * Append element to tag array in header.
 * Appends item p to entry w/ tag and type as passed. Won't work on
 * RPM_STRING_TYPE. Any pointers into header memory returned from
 * headerGetEntryMinMemory() for this entry are invalid after this
 * call has been made!
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
	/*@modifies h @*/
{
    indexEntry entry;
    int length;

    /* First find the tag */
    entry = findEntry(h, tag, type);
    if (!entry)
	return 0;

    if (type == RPM_STRING_TYPE || type == RPM_I18NSTRING_TYPE) {
	/* we can't do this */
	return 0;
    }

    length = dataLength(type, p, c, 0);

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

/** \ingroup header
 * Add or append element to tag array in header.
 * @todo Arg "p" should have const.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
	/*@modifies h @*/
{
    return (findEntry(h, tag, type)
	? headerAppendEntry(h, tag, type, p, c)
	: headerAddEntry(h, tag, type, p, c));
}

/** \ingroup header
 * Add locale specific tag to header.
 * A NULL lang is interpreted as the C locale. Here are the rules:
 * \verbatim
 *	- If the tag isn't in the header, it's added with the passed string
 *	   as new value.
 *	- If the tag occurs multiple times in entry, which tag is affected
 *	   by the operation is undefined.
 *	- If the tag is in the header w/ this language, the entry is
 *	   *replaced* (like headerModifyEntry()).
 * \endverbatim
 * This function is intended to just "do the right thing". If you need
 * more fine grained control use headerAddEntry() and headerModifyEntry().
 *
 * @param h		header
 * @param tag		tag
 * @param string	tag value
 * @param lang		locale
 * @return		1 on success, 0 on failure
 */
static
int headerAddI18NString(Header h, int_32 tag, const char * string,
		const char * lang)
	/*@modifies h @*/
{
    indexEntry table, entry;
    const char ** strArray;
    int length;
    int ghosts;
    int i, langNum;
    char * buf;

    table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE);
    entry = findEntry(h, tag, RPM_I18NSTRING_TYPE);

    if (!table && entry)
	return 0;		/* this shouldn't ever happen!! */

    if (!table && !entry) {
	const char * charArray[2];
	int count = 0;
	if (!lang || (lang[0] == 'C' && lang[1] == '\0')) {
	    /*@-observertrans -readonlytrans@*/
	    charArray[count++] = "C";
	    /*@=observertrans =readonlytrans@*/
	} else {
	    /*@-observertrans -readonlytrans@*/
	    charArray[count++] = "C";
	    /*@=observertrans =readonlytrans@*/
	    charArray[count++] = lang;
	}
	if (!headerAddEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE, 
			&charArray, count))
	    return 0;
	table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE);
    }

    if (!table)
	return 0;
    /*@-branchstate@*/
    if (!lang) lang = "C";
    /*@=branchstate@*/

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
	strArray = alloca(sizeof(*strArray) * (langNum + 1));
	for (i = 0; i < langNum; i++)
	    strArray[i] = "";
	strArray[langNum] = string;
	return headerAddEntry(h, tag, RPM_I18NSTRING_TYPE, strArray, 
				langNum + 1);
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
/*@-mayaliasunique@*/
	memcpy(t, string, sn);
	t += sn;
	memcpy(t, e, en);
	t += en;
/*@=mayaliasunique@*/

	/* Replace I18N string array */
	entry->length -= strlen(be) + 1;
	entry->length += sn;
	
	if (ENTRY_IN_REGION(entry)) {
	    entry->info.offset = 0;
	} else
	    entry->data = _free(entry->data);
	/*@-dependenttrans@*/
	entry->data = buf;
	/*@=dependenttrans@*/
    }

    return 0;
}

/** \ingroup header
 * Modify tag in header.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerModifyEntry(Header h, int_32 tag, int_32 type,
			const void * p, int_32 c)
	/*@modifies h @*/
{
    indexEntry entry;
    void * oldData;

    /* First find the tag */
    entry = findEntry(h, tag, type);
    if (!entry)
	return 0;

    /* make sure entry points to the first occurence of this tag */
    while (entry > h->index && (entry - 1)->info.tag == tag)  
	entry--;

    /* free after we've grabbed the new data in case the two are intertwined;
       that's a bad idea but at least we won't break */
    oldData = entry->data;

    entry->info.count = c;
    entry->info.type = type;
    entry->data = grabData(type, p, c, &entry->length);

    /*@-branchstate@*/
    if (ENTRY_IN_REGION(entry)) {
	entry->info.offset = 0;
    } else
	oldData = _free(oldData);
    /*@=branchstate@*/

    return 1;
}

/**
 */
static char escapedChar(const char ch)	/*@*/
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
static /*@null@*/ sprintfToken
freeFormat( /*@only@*/ /*@null@*/ sprintfToken format, int num)
	/*@modifies *format @*/
{
    int i;

    if (format == NULL) return NULL;
    for (i = 0; i < num; i++) {
	switch (format[i].type) {
	case PTOK_ARRAY:
	    format[i].u.array.format =
		freeFormat(format[i].u.array.format,
			format[i].u.array.numTokens);
	    /*@switchbreak@*/ break;
	case PTOK_COND:
	    format[i].u.cond.ifFormat =
		freeFormat(format[i].u.cond.ifFormat, 
			format[i].u.cond.numIfTokens);
	    format[i].u.cond.elseFormat =
		freeFormat(format[i].u.cond.elseFormat, 
			format[i].u.cond.numElseTokens);
	    /*@switchbreak@*/ break;
	case PTOK_NONE:
	case PTOK_TAG:
	case PTOK_STRING:
	default:
	    /*@switchbreak@*/ break;
	}
    }
    format = _free(format);
    return NULL;
}

/**
 */
static void findTag(char * name, const headerTagTableEntry tags, 
		    const headerSprintfExtension extensions,
		    /*@out@*/ headerTagTableEntry * tagMatch,
		    /*@out@*/ headerSprintfExtension * extMatch)
	/*@modifies *tagMatch, *extMatch @*/
{
    headerTagTableEntry entry;
    headerSprintfExtension ext;
    const char * tagname;

    *tagMatch = NULL;
    *extMatch = NULL;

    if (strncmp("RPMTAG_", name, sizeof("RPMTAG_")-1)) {
	char * t = alloca(strlen(name) + sizeof("RPMTAG_"));
	(void) stpcpy( stpcpy(t, "RPMTAG_"), name);
	tagname = t;
    } else {
	tagname = name;
    }

    /* Search extensions first to permit overriding header tags. */
    ext = extensions;
    while (ext->type != HEADER_EXT_LAST) {
	if (ext->name != NULL && ext->type == HEADER_EXT_TAG
	&& !xstrcasecmp(ext->name, tagname))
	    break;

	if (ext->type == HEADER_EXT_MORE)
	    ext = ext->u.more;
	else
	    ext++;
    }

    if (ext->type == HEADER_EXT_TAG) {
	*extMatch = ext;
	return;
    }

    /* Search header tags. */
    for (entry = tags; entry->name; entry++)
	if (entry->name && !xstrcasecmp(entry->name, tagname))
	    break;

    if (entry->name) {
	*tagMatch = entry;
	return;
    }
}

/* forward ref */
static int parseExpression(sprintfToken token, char * str, 
		const headerTagTableEntry tags, 
		const headerSprintfExtension extensions,
		/*@out@*/char ** endPtr, /*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies str, *str, *token, *endPtr, *errmsg @*/;

/**
 */
static int parseFormat(char * str, const headerTagTableEntry tags,
		const headerSprintfExtension extensions,
		/*@out@*/sprintfToken * formatPtr, /*@out@*/int * numTokensPtr,
		/*@null@*/ /*@out@*/ char ** endPtr, int state,
		/*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies str, *str, *formatPtr, *numTokensPtr, *endPtr, *errmsg @*/
{
    char * chptr, * start, * next, * dst;
    sprintfToken format;
    int numTokens;
    int currToken;
    headerTagTableEntry tag;
    headerSprintfExtension ext;
    int i;
    int done = 0;

    /* upper limit on number of individual formats */
    numTokens = 0;
    for (chptr = str; *chptr != '\0'; chptr++)
	if (*chptr == '%') numTokens++;
    numTokens = numTokens * 2 + 1;

    format = xcalloc(numTokens, sizeof(*format));
    if (endPtr) *endPtr = NULL;

    /*@-infloops@*/ /* LCL: can't detect done termination */
    dst = start = str;
    currToken = -1;
    while (*start != '\0') {
	switch (*start) {
	case '%':
	    /* handle %% */
	    if (*(start + 1) == '%') {
		if (currToken < 0 || format[currToken].type != PTOK_STRING) {
		    currToken++;
		    format[currToken].type = PTOK_STRING;
		    /*@-temptrans -assignexpose@*/
		    dst = format[currToken].u.string.string = start;
		    /*@=temptrans =assignexpose@*/
		}

		start++;

		*dst++ = *start++;

		/*@switchbreak@*/ break;
	    } 

	    currToken++;
	    *dst++ = '\0';
	    start++;

	    if (*start == '|') {
		char * newEnd;

		start++;
		if (parseExpression(format + currToken, start, tags, 
				    extensions, &newEnd, errmsg))
		{
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		start = newEnd;
		/*@switchbreak@*/ break;
	    }

	    /*@-assignexpose@*/
	    format[currToken].u.tag.format = start;
	    /*@=assignexpose@*/
	    format[currToken].u.tag.pad = 0;
	    format[currToken].u.tag.justOne = 0;
	    format[currToken].u.tag.arrayCount = 0;

	    chptr = start;
	    while (*chptr && *chptr != '{' && *chptr != '%') chptr++;
	    if (!*chptr || *chptr == '%') {
		/*@-observertrans -readonlytrans@*/
		if (errmsg) *errmsg = _("missing { after %");
		/*@=observertrans =readonlytrans@*/
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    *chptr++ = '\0';

	    while (start < chptr) {
		if (xisdigit(*start)) {
		    i = strtoul(start, &start, 10);
		    format[currToken].u.tag.pad += i;
		} else {
		    start++;
		}
	    }

	    if (*start == '=') {
		format[currToken].u.tag.justOne = 1;
		start++;
	    } else if (*start == '#') {
		format[currToken].u.tag.justOne = 1;
		format[currToken].u.tag.arrayCount = 1;
		start++;
	    }

	    next = start;
	    while (*next && *next != '}') next++;
	    if (!*next) {
		/*@-observertrans -readonlytrans@*/
		if (errmsg) *errmsg = _("missing } after %{");
		/*@=observertrans =readonlytrans@*/
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *next++ = '\0';

	    chptr = start;
	    while (*chptr && *chptr != ':') chptr++;

	    if (*chptr != '\0') {
		*chptr++ = '\0';
		if (!*chptr) {
		    /*@-observertrans -readonlytrans@*/
		    if (errmsg) *errmsg = _("empty tag format");
		    /*@=observertrans =readonlytrans@*/
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		/*@-assignexpose@*/
		format[currToken].u.tag.type = chptr;
		/*@=assignexpose@*/
	    } else {
		format[currToken].u.tag.type = NULL;
	    }
	    
	    if (!*start) {
		/*@-observertrans -readonlytrans@*/
		if (errmsg) *errmsg = _("empty tag name");
		/*@=observertrans =readonlytrans@*/
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    i = 0;
	    findTag(start, tags, extensions, &tag, &ext);

	    if (tag) {
		format[currToken].u.tag.ext = NULL;
		format[currToken].u.tag.tag = tag->val;
	    } else if (ext) {
		format[currToken].u.tag.ext = ext->u.tagFunction;
		format[currToken].u.tag.extNum = ext - extensions;
	    } else {
		/*@-observertrans -readonlytrans@*/
		if (errmsg) *errmsg = _("unknown tag");
		/*@=observertrans =readonlytrans@*/
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    format[currToken].type = PTOK_TAG;

	    start = next;

	    /*@switchbreak@*/ break;

	case '[':
	    *dst++ = '\0';
	    *start++ = '\0';
	    currToken++;

	    if (parseFormat(start, tags, extensions, 
			    &format[currToken].u.array.format,
			    &format[currToken].u.array.numTokens,
			    &start, PARSER_IN_ARRAY, errmsg)) {
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    if (!start) {
		/*@-observertrans -readonlytrans@*/
		if (errmsg) *errmsg = _("] expected at end of array");
		/*@=observertrans =readonlytrans@*/
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    dst = start;

	    format[currToken].type = PTOK_ARRAY;

	    /*@switchbreak@*/ break;

	case ']':
	case '}':
	    if ((*start == ']' && state != PARSER_IN_ARRAY) ||
	        (*start == '}' && state != PARSER_IN_EXPR)) {
		if (*start == ']') {
		    /*@-observertrans -readonlytrans@*/
		    if (errmsg) *errmsg = _("unexpected ]");
		    /*@=observertrans =readonlytrans@*/
		} else {
		    /*@-observertrans -readonlytrans@*/
		    if (errmsg) *errmsg = _("unexpected }");
		    /*@=observertrans =readonlytrans@*/
		}
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    /*@switchbreak@*/ break;

	default:
	    if (currToken < 0 || format[currToken].type != PTOK_STRING) {
		currToken++;
		format[currToken].type = PTOK_STRING;
		/*@-temptrans -assignexpose@*/
		dst = format[currToken].u.string.string = start;
		/*@=temptrans =assignexpose@*/
	    }

	    if (*start == '\\') {
		start++;
		*dst++ = escapedChar(*start++);
	    } else {
		*dst++ = *start++;
	    }
	    /*@switchbreak@*/ break;
	}
	if (done)
	    break;
    }
    /*@=infloops@*/

    *dst = '\0';

    currToken++;
    for (i = 0; i < currToken; i++) {
	if (format[i].type == PTOK_STRING)
	    format[i].u.string.len = strlen(format[i].u.string.string);
    }

    *numTokensPtr = currToken;
    *formatPtr = format;

    return 0;
}

/**
 */
static int parseExpression(sprintfToken token, char * str, 
		const headerTagTableEntry tags, 
		const headerSprintfExtension extensions,
		/*@out@*/ char ** endPtr,
		/*@null@*/ /*@out@*/ errmsg_t * errmsg)
{
    headerTagTableEntry tag;
    headerSprintfExtension ext;
    char * chptr;
    char * end;

    if (errmsg) *errmsg = NULL;
    chptr = str;
    while (*chptr && *chptr != '?') chptr++;

    if (*chptr != '?') {
	/*@-observertrans -readonlytrans@*/
	if (errmsg) *errmsg = _("? expected in expression");
	/*@=observertrans =readonlytrans@*/
	return 1;
    }

    *chptr++ = '\0';;

    if (*chptr != '{') {
	/*@-observertrans -readonlytrans@*/
	if (errmsg) *errmsg = _("{ expected after ? in expression");
	/*@=observertrans =readonlytrans@*/
	return 1;
    }

    chptr++;

    if (parseFormat(chptr, tags, extensions, &token->u.cond.ifFormat, 
		    &token->u.cond.numIfTokens, &end, PARSER_IN_EXPR, errmsg)) 
	return 1;

    if (!*end) {
	/*@-observertrans -readonlytrans@*/
	if (errmsg) *errmsg = _("} expected in expression");
	/*@=observertrans =readonlytrans@*/
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    chptr = end;
    if (*chptr != ':' && *chptr != '|') {
	/*@-observertrans -readonlytrans@*/
	if (errmsg) *errmsg = _(": expected following ? subexpression");
	/*@=observertrans =readonlytrans@*/
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    if (*chptr == '|') {
	(void) parseFormat(xstrdup(""), tags, extensions,
			&token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR, 
			errmsg);
    } else {
	chptr++;

	if (*chptr != '{') {
	    /*@-observertrans -readonlytrans@*/
	    if (errmsg) *errmsg = _("{ expected after : in expression");
	    /*@=observertrans =readonlytrans@*/
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr++;

	if (parseFormat(chptr, tags, extensions, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR, 
			errmsg)) 
	    return 1;
	if (!*end) {
	    /*@-observertrans -readonlytrans@*/
	    if (errmsg) *errmsg = _("} expected in expression");
	    /*@=observertrans =readonlytrans@*/
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr = end;
	if (*chptr != '|') {
	    /*@-observertrans -readonlytrans@*/
	    if (errmsg) *errmsg = _("| expected at end of expression");
	    /*@=observertrans =readonlytrans@*/
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.elseFormat =
		freeFormat(token->u.cond.elseFormat, token->u.cond.numElseTokens);
	    return 1;
	}
    }
	
    chptr++;

    *endPtr = chptr;

    findTag(str, tags, extensions, &tag, &ext);

    if (tag) {
	token->u.cond.tag.ext = NULL;
	token->u.cond.tag.tag = tag->val;
    } else if (ext) {
	token->u.cond.tag.ext = ext->u.tagFunction;
	token->u.cond.tag.extNum = ext - extensions;
    } else {
	token->u.cond.tag.ext = NULL;
	token->u.cond.tag.tag = -1;
    }
	
    token->type = PTOK_COND;

    return 0;
}

/**
 * @return		0 on success, 1 on failure
 */
static int getExtension(Header h, headerTagTagFunction fn,
		/*@out@*/ hTYP_t typeptr,
		/*@out@*/ hPTR_t * data,
		/*@out@*/ hCNT_t countptr,
		extensionCache ext)
	/*@modifies *typeptr, *data, *countptr, ext @*/
{
    if (!ext->avail) {
	if (fn(h, &ext->type, &ext->data, &ext->count, &ext->freeit))
	    return 1;
	ext->avail = 1;
    }

    if (typeptr) *typeptr = ext->type;
    if (data) *data = ext->data;
    if (countptr) *countptr = ext->count;

    return 0;
}

/**
 */
/*@observer@*/
static char * formatValue(sprintfTag tag, Header h, 
		const headerSprintfExtension extensions,
		extensionCache extCache, int element,
		char ** valp, int * vallenp, int * allocedp)
	/*@modifies extCache, *valp, *vallenp, *allocedp @*/
{
    char * val = NULL;
    int need = 0;
    char * t, * te;
    char buf[20];
    int_32 count, type;
    hPTR_t data;
    unsigned int intVal;
    const char ** strarray;
    int datafree = 0;
    int countBuf;
    headerTagFormatFunction tagtype = NULL;
    headerSprintfExtension ext;

    memset(buf, 0, sizeof(buf));
    /*@-branchstate@*/
    if (tag->ext) {
	if (getExtension(h, tag->ext, &type, &data, &count, 
			 extCache + tag->extNum))
	{
	    count = 1;
	    type = RPM_STRING_TYPE;	
	    data = "(none)";
	}
    } else {
	if (!headerGetEntry(h, tag->tag, &type, (void **)&data, &count)) {
	    count = 1;
	    type = RPM_STRING_TYPE;	
	    data = "(none)";
	}

	datafree = 1;
    }
    /*@=branchstate@*/

    if (tag->arrayCount) {
	/*@-observertrans -modobserver@*/
	data = headerFreeData(data, type);
	/*@=observertrans =modobserver@*/

	countBuf = count;
	data = &countBuf;
	count = 1;
	type = RPM_INT32_TYPE;
    }

    (void) stpcpy( stpcpy(buf, "%"), tag->format);

    if (tag->type) {
	ext = extensions;
	while (ext->type != HEADER_EXT_LAST) {
	    if (ext->name != NULL && ext->type == HEADER_EXT_FORMAT
	    && !strcmp(ext->name, tag->type))
	    {
		tagtype = ext->u.formatFunction;
		break;
	    }

	    if (ext->type == HEADER_EXT_MORE)
		ext = ext->u.more;
	    else
		ext++;
	}
    }
    
    /*@-branchstate@*/
    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
	strarray = (const char **)data;

	if (tagtype)
	    val = tagtype(RPM_STRING_TYPE, strarray[element], buf, tag->pad, 0);

	if (val) {
	    need = strlen(val);
	} else {
	    need = strlen(strarray[element]) + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "s");
	    /*@-formatconst@*/
	    sprintf(val, buf, strarray[element]);
	    /*@=formatconst@*/
	}

	/*@-observertrans -modobserver@*/
	if (datafree) data = _free(data);
	/*@=observertrans =modobserver@*/

	break;

    case RPM_STRING_TYPE:
	if (tagtype)
	    val = tagtype(RPM_STRING_ARRAY_TYPE, data, buf, tag->pad,  0);

	if (val) {
	    need = strlen(val);
	} else {
	    need = strlen(data) + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "s");
	    /*@-formatconst@*/
	    sprintf(val, buf, data);
	    /*@=formatconst@*/
	}
	break;

    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
	switch (type) {
	case RPM_CHAR_TYPE:	
	case RPM_INT8_TYPE:
	    intVal = *(((int_8 *) data) + element);
	    /*@innerbreak@*/ break;
	case RPM_INT16_TYPE:
	    intVal = *(((uint_16 *) data) + element);
	    /*@innerbreak@*/ break;
	default:		/* keep -Wall quiet */
	case RPM_INT32_TYPE:
	    intVal = *(((int_32 *) data) + element);
	    /*@innerbreak@*/ break;
	}

	if (tagtype)
	    val = tagtype(RPM_INT32_TYPE, &intVal, buf, tag->pad,  element);

	if (val) {
	    need = strlen(val);
	} else {
	    need = 10 + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "d");
	    /*@-formatconst@*/
	    sprintf(val, buf, intVal);
	    /*@=formatconst@*/
	}
	break;

    case RPM_BIN_TYPE:
	if (tagtype)
	    val = tagtype(RPM_BIN_TYPE, data, buf, tag->pad, count);

	if (val) {
	    need = count;	/* XXX broken iff RPM_BIN_TYPE extension */
	} else {
#ifdef	NOTYET
	    val = memcpy(xmalloc(count), data, count);
#else
	    /* XXX format string not used */
	    static char hex[] = "0123456789abcdef";
	    const char * s = data;

	    need = 2*count + tag->pad;
	    val = t = xmalloc(need+1);
	    while (count-- > 0) {
		unsigned int i;
		i = *s++;
		*t++ = hex[ (i >> 4) & 0xf ];
		*t++ = hex[ (i     ) & 0xf ];
	    }
	    *t = '\0';
#endif
	}
	break;

    default:
	need = sizeof("(unknown type)") - 1;
	val = xstrdup("(unknown type)");
	break;
    }
    /*@=branchstate@*/

    /*@-branchstate@*/
    if (val && need > 0) {
	if (((*vallenp) + need) >= (*allocedp)) {
	    if ((*allocedp) <= need)
		(*allocedp) += need;
	    (*allocedp) <<= 1;
/*@-unqualifiedtrans@*/ /* FIX: double indirection */
	    (*valp) = xrealloc((*valp), (*allocedp)+1);	
/*@=unqualifiedtrans@*/
	}
	t = (*valp) + (*vallenp);
	te = stpcpy(t, val);
	(*vallenp) += (te - t);
	val = _free(val);
    }
    /*@=branchstate@*/

    return ((*valp) + (*vallenp));
}

/**
 */
/*@observer@*/
static char * singleSprintf(Header h, sprintfToken token,
		const headerSprintfExtension extensions,
		extensionCache extCache, int element,
		char ** valp, int * vallenp, int * allocedp)
	/*@modifies h, extCache, *valp, *vallenp, *allocedp @*/
{
    char * t, * te;
    int i, j;
    int numElements;
    int type;
    sprintfToken condFormat;
    int condNumFormats;
    int need;

    /* we assume the token and header have been validated already! */

    switch (token->type) {
    case PTOK_NONE:
	break;

    case PTOK_STRING:
	need = token->u.string.len;
	if (need <= 0) break;
	if (((*vallenp) + need) >= (*allocedp)) {
	    if ((*allocedp) <= need)
		(*allocedp) += need;
	    (*allocedp) <<= 1;
/*@-unqualifiedtrans@*/ /* FIX: double indirection */
	    (*valp) = xrealloc((*valp), (*allocedp)+1);	
/*@=unqualifiedtrans@*/
	}
	t = (*valp) + (*vallenp);
	te = stpcpy(t, token->u.string.string);
	(*vallenp) += (te - t);
	break;

    case PTOK_TAG:
	t = (*valp) + (*vallenp);
	te = formatValue(&token->u.tag, h, extensions, extCache,
			(token->u.tag.justOne ? 0 : element),
			valp, vallenp, allocedp);
	break;

    case PTOK_COND:
	if (token->u.cond.tag.ext ||
	    headerIsEntry(h, token->u.cond.tag.tag)) {
	    condFormat = token->u.cond.ifFormat;
	    condNumFormats = token->u.cond.numIfTokens;
	} else {
	    condFormat = token->u.cond.elseFormat;
	    condNumFormats = token->u.cond.numElseTokens;
	}

	need = condNumFormats * 20;
	if (condFormat == NULL || need <= 0) break;
	if (((*vallenp) + need) >= (*allocedp)) {
	    if ((*allocedp) <= need)
		(*allocedp) += need;
	    (*allocedp) <<= 1;
/*@-unqualifiedtrans@*/ /* FIX: double indirection */
	    (*valp) = xrealloc((*valp), (*allocedp)+1);	
/*@=unqualifiedtrans@*/
	}

	t = (*valp) + (*vallenp);
	for (i = 0; i < condNumFormats; i++)
	    te = singleSprintf(h, condFormat + i, extensions, extCache,
				element, valp, vallenp, allocedp);
	break;

    case PTOK_ARRAY:
	numElements = -1;
	for (i = 0; i < token->u.array.numTokens; i++) {
	    if (token->u.array.format[i].type != PTOK_TAG ||
		token->u.array.format[i].u.tag.arrayCount ||
		token->u.array.format[i].u.tag.justOne) continue;

	    if (token->u.array.format[i].u.tag.ext) {
		const void * data;
		if (getExtension(h, token->u.array.format[i].u.tag.ext,
				 &type, &data, &numElements, 
				 extCache + 
				   token->u.array.format[i].u.tag.extNum))
		     continue;
	    } else {
		if (!headerGetEntry(h, token->u.array.format[i].u.tag.tag, 
				    &type, NULL, &numElements))
		    continue;
	    } 
	    /*@loopbreak@*/ break;
	}

	if (numElements == -1) {
	    need = sizeof("(none)") - 1;
	    if (((*vallenp) + need) >= (*allocedp)) {
		if ((*allocedp) <= need)
		    (*allocedp) += need;
		(*allocedp) <<= 1;
/*@-unqualifiedtrans@*/ /* FIX: double indirection */
		(*valp) = xrealloc((*valp), (*allocedp)+1);	
/*@=unqualifiedtrans@*/
	    }
	    t = (*valp) + (*vallenp);
	    te = stpcpy(t, "(none)");
	    (*vallenp) += (te - t);
	} else {
	    need = numElements * token->u.array.numTokens * 10;
	    if (need <= 0) break;
	    if (((*vallenp) + need) >= (*allocedp)) {
		if ((*allocedp) <= need)
		    (*allocedp) += need;
		(*allocedp) <<= 1;
/*@-unqualifiedtrans@*/ /* FIX: double indirection */
		(*valp) = xrealloc((*valp), (*allocedp)+1);	
/*@=unqualifiedtrans@*/
	    }

	    t = (*valp) + (*vallenp);
	    for (j = 0; j < numElements; j++) {
		for (i = 0; i < token->u.array.numTokens; i++)
		    te = singleSprintf(h, token->u.array.format + i, 
					extensions, extCache, j,
					valp, vallenp, allocedp);
	    }
	}
	break;
    }

    return ((*valp) + (*vallenp));
}

/**
 */
static /*@only@*/ extensionCache
allocateExtensionCache(const headerSprintfExtension extensions)
	/*@*/
{
    headerSprintfExtension ext = extensions;
    int i = 0;

    while (ext->type != HEADER_EXT_LAST) {
	i++;
	if (ext->type == HEADER_EXT_MORE)
	    ext = ext->u.more;
	else
	    ext++;
    }

    /*@-sizeoftype@*/
    return xcalloc(i, sizeof(struct extensionCache));
    /*@=sizeoftype@*/
}

/**
 * @return		NULL always
 */
static /*@null@*/ extensionCache
freeExtensionCache(const headerSprintfExtension extensions,
		        /*@only@*/ extensionCache cache)
	/*@*/
{
    headerSprintfExtension ext = extensions;
    int i = 0;

    while (ext->type != HEADER_EXT_LAST) {
	if (cache[i].freeit) cache[i].data = _free(cache[i].data);

	i++;
	if (ext->type == HEADER_EXT_MORE)
	    ext = ext->u.more;
	else
	    ext++;
    }

    cache = _free(cache);
    return NULL;
}

/** \ingroup header
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 *
 * @param h		header
 * @param fmt		format to use
 * @param tbltags	array of tag name/value pairs
 * @param extensions	chained table of formatting extensions.
 * @retval errmsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
static /*@only@*/ /*@null@*/
char * headerSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tbltags,
		     const struct headerSprintfExtension_s * extensions,
		     /*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies *errmsg @*/
{
    /*@-castexpose@*/	/* FIX: legacy API shouldn't change. */
    headerSprintfExtension exts = (headerSprintfExtension) extensions;
    headerTagTableEntry tags = (headerTagTableEntry) tbltags;
    /*@=castexpose@*/
    char * t;
    char * fmtString;
    sprintfToken format;
    int numTokens;
    char * val = NULL;
    int vallen = 0;
    int alloced = 0;
    int i;
    extensionCache extCache;
 
    /*fmtString = escapeString(fmt);*/
    fmtString = xstrdup(fmt);
   
    if (parseFormat(fmtString, tags, exts, &format, &numTokens, 
		    NULL, PARSER_BEGIN, errmsg)) {
	fmtString = _free(fmtString);
	return NULL;
    }

    extCache = allocateExtensionCache(exts);

    for (i = 0; i < numTokens; i++) {
	/*@-mods@*/
	t = singleSprintf(h, format + i, exts, extCache, 0,
		&val, &vallen, &alloced);
	/*@=mods@*/
    }

    if (val != NULL && vallen < alloced)
	val = xrealloc(val, vallen+1);	

    fmtString = _free(fmtString);
    extCache = freeExtensionCache(exts, extCache);
    format = _free(format);

    return val;
}

/**
 */
static char * octalFormat(int_32 type, hPTR_t data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
	/*@modifies formatPrefix @*/
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(20 + padding);
	strcat(formatPrefix, "o");
	/*@-formatconst@*/
	sprintf(val, formatPrefix, *((int_32 *) data));
	/*@=formatconst@*/
    }

    return val;
}

/**
 */
static char * hexFormat(int_32 type, hPTR_t data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
	/*@modifies formatPrefix @*/
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(20 + padding);
	strcat(formatPrefix, "x");
	/*@-formatconst@*/
	sprintf(val, formatPrefix, *((int_32 *) data));
	/*@=formatconst@*/
    }

    return val;
}

/**
 */
static char * realDateFormat(int_32 type, hPTR_t data, 
		char * formatPrefix, int padding, /*@unused@*/int element,
		const char * strftimeFormat)
	/*@modifies formatPrefix @*/
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	struct tm * tstruct;
	char buf[50];

	val = xmalloc(50 + padding);
	strcat(formatPrefix, "s");

	/* this is important if sizeof(int_32) ! sizeof(time_t) */
	{   time_t dateint = *((int_32 *) data);
	    tstruct = localtime(&dateint);
	}
	buf[0] = '\0';
	if (tstruct)
	    (void) strftime(buf, sizeof(buf) - 1, strftimeFormat, tstruct);
	/*@-formatconst@*/
	sprintf(val, formatPrefix, buf);
	/*@=formatconst@*/
    }

    return val;
}

/**
 */
static char * dateFormat(int_32 type, hPTR_t data, 
		         char * formatPrefix, int padding, int element)
	/*@modifies formatPrefix @*/
{
    return realDateFormat(type, data, formatPrefix, padding, element, "%c");
}

/**
 */
static char * dayFormat(int_32 type, hPTR_t data, 
		         char * formatPrefix, int padding, int element)
	/*@modifies formatPrefix @*/
{
    return realDateFormat(type, data, formatPrefix, padding, element, 
			  "%a %b %d %Y");
}

/**
 */
static char * shescapeFormat(int_32 type, hPTR_t data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
	/*@modifies formatPrefix @*/
{
    char * result, * dst, * src, * buf;

    if (type == RPM_INT32_TYPE) {
	result = xmalloc(padding + 20);
	strcat(formatPrefix, "d");
	/*@-formatconst@*/
	sprintf(result, formatPrefix, *((int_32 *) data));
	/*@=formatconst@*/
    } else {
	buf = alloca(strlen(data) + padding + 2);
	strcat(formatPrefix, "s");
	/*@-formatconst@*/
	sprintf(buf, formatPrefix, data);
	/*@=formatconst@*/

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

    }

    return result;
}

/*@-type@*/ /* FIX: cast? */
const struct headerSprintfExtension_s headerDefaultFormats[] = {
    { HEADER_EXT_FORMAT, "octal", { octalFormat } },
    { HEADER_EXT_FORMAT, "hex", { hexFormat } },
    { HEADER_EXT_FORMAT, "date", { dateFormat } },
    { HEADER_EXT_FORMAT, "day", { dayFormat } },
    { HEADER_EXT_FORMAT, "shescape", { shescapeFormat } },
    { HEADER_EXT_LAST, NULL, { NULL } }
};
/*@=type@*/

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
static
void headerCopyTags(Header headerFrom, Header headerTo, hTAG_t tagstocopy)
	/*@modifies headerTo @*/
{
    int * p;

    if (headerFrom == headerTo)
	return;

    for (p = tagstocopy; *p != 0; p++) {
	char *s;
	int_32 type;
	int_32 count;
	if (headerIsEntry(headerTo, *p))
	    continue;
	if (!headerGetEntryMinMemory(headerFrom, *p, &type,
				(hPTR_t *) &s, &count))
	    continue;
	(void) headerAddEntry(headerTo, *p, type, s, count);
	s = headerFreeData(s, type);
    }
}

/**
 * Header tag iterator data structure.
 */
struct headerIteratorS {
/*@unused@*/ Header h;		/*!< Header being iterated. */
/*@unused@*/ int next_index;	/*!< Next tag index. */
};

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
static /*@null@*/
HeaderIterator headerFreeIterator(/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/
{
    hi->h = headerFree(hi->h, "Iterator");
    hi = _free(hi);
    return hi;
}

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
static
HeaderIterator headerInitIterator(Header h)
	/*@modifies h */
{
    HeaderIterator hi = xmalloc(sizeof(*hi));

    headerSort(h);

    hi->h = headerLink(h, "Iterator");
    hi->next_index = 0;
    return hi;
}

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval tag		address of tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
static
int headerNextIterator(HeaderIterator hi,
		/*@null@*/ /*@out@*/ hTAG_t tag,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies hi, *tag, *type, *p, *c @*/
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
    /*@-noeffect@*/	/* LCL: no clue */
    hi->next_index++;
    /*@=noeffect@*/

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
static /*@null@*/
Header headerCopy(Header h)
	/*@modifies h @*/
{
    Header nh = headerNew();
    HeaderIterator hi;
    int_32 tag, type, count;
    hPTR_t ptr;
   
    /*@-branchstate@*/
    for (hi = headerInitIterator(h);
	headerNextIterator(hi, &tag, &type, &ptr, &count);
	ptr = headerFreeData((void *)ptr, type))
    {
	if (ptr) (void) headerAddEntry(nh, tag, type, ptr, count);
    }
    hi = headerFreeIterator(hi);
    /*@=branchstate@*/

    return headerReload(nh, HEADER_IMAGE);
}

/*@observer@*/ /*@unchecked@*/
static struct HV_s hdrVec1 = {
    XheaderLink,
    XheaderUnlink,
    XheaderFree,
    headerNew,
    headerSort,
    headerUnsort,
    headerSizeof,
    headerUnload,
    headerReload,
    headerCopy,
    headerLoad,
    headerCopyLoad,
    headerRead,
    headerWrite,
    headerIsEntry,
    headerFreeTag,
    headerGetEntry,
    headerGetEntryMinMemory,
    headerAddEntry,
    headerAppendEntry,
    headerAddOrAppendEntry,
    headerAddI18NString,
    headerModifyEntry,
    headerRemoveEntry,
    headerSprintf,
    headerCopyTags,
    headerFreeIterator,
    headerInitIterator,
    headerNextIterator,
    NULL, NULL,
    1
};

/*@-compmempass -redef@*/
/*@observer@*/ /*@unchecked@*/
HV_t hdrVec = &hdrVec1;
/*@=compmempass =redef@*/
