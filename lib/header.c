/** \ingroup header
 * \file lib/header.c
 *
 */

/* RPM - Copyright (C) 1995-2000 Red Hat Software */

/* Data written to file descriptors is in network byte order.    */
/* Data read from file descriptors is expected to be in          */
/* network byte order and is converted on the fly to host order. */

#include "system.h"

#ifdef	__LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
#else
#include <netinet/in.h>
#endif	/* __LCLINT__ */

#include <rpmio.h>
#include <header.h>
#include <assert.h>

/* XXX avoid rpmlib.h for debugging. */
/*@observer@*/ const char *const tagName(int tag)	/*@*/;

#define INDEX_MALLOC_SIZE 8

#define PARSER_BEGIN 	0
#define PARSER_IN_ARRAY 1
#define PARSER_IN_EXPR  2

static unsigned char header_magic[8] = {
	0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/** \ingroup header
 * Alignment needs (and sizeof scalars types) for internal rpm data types.
 */
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
    void * data; 		/*!< Location of tag data. */
    int length;			/*!< No. bytes of data. */
};

/**
 * The Header data structure.
 */
struct headerToken {
    struct indexEntry *index;	/*!< Array of tags. */
    int indexUsed;		/*!< Current size of tag array. */
    int indexAlloced;		/*!< Allocated size of tag array. */
    int region_allocated;	/*!< Is 1st header region allocated? */
    int sorted; 		/*!< Are header entries sorted? */
    int reloaded;		/*!< Append region data? */
/*@refs@*/ int nrefs;	/*!< Reference count. */
};

/**
 */
struct sprintfTag {
    headerTagTagFunction ext;   /*!< if NULL tag element is invalid */
    int extNum;
    int_32 tag;
    int justOne;
    int arrayCount;
/*@kept@*/ char * format;
/*@kept@*/ char * type;
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
	    /*@only@*/ struct sprintfToken * ifFormat;
	    int numIfTokens;
	    /*@only@*/ struct sprintfToken * elseFormat;
	    int numElseTokens;
	    struct sprintfTag tag;
	} cond;
    } u;
};

#if HAVE_MCHECK_H
static int probe_headers = 0;
#define	HEADERPROBE(_h, _msg)	if (probe_headers) headerProbe((_h), (_msg))

static void headerProbeAddr(Header h, const char * msg,
	void * p, const char * imsg)
{
    const char * mchkstr = NULL;
    switch (mprobe(p)) {
    case MCHECK_DISABLED:
    case MCHECK_OK:
	return;
	/*@notreached@*/ break;
    case MCHECK_HEAD:
	mchkstr = "HEAD";
	break;
    case MCHECK_TAIL:
	mchkstr = "TAIL";
	break;
    case MCHECK_FREE:
	mchkstr = "FREE";
	break;
    }
    fprintf(stderr, "*** MCHECK_%s h %p", mchkstr, h);
    if (imsg && p)
	fprintf(stderr, " %s %p", imsg, p);
    if (msg)
	fprintf(stderr, " %s", msg);
    fprintf(stderr, "\n");
}

static void headerProbe(Header h, const char *msg)
{
    char imsg[256];
    int i;

    headerProbeAddr(h, msg, h, "header");
    sprintf(imsg, "index (used %d)", h->indexUsed);
    headerProbeAddr(h, msg, h->index, imsg);
    for (i = 0; i < h->indexUsed; i++) {
	sprintf(imsg, "index[%d:%d].data", i, h->indexUsed);
	headerProbeAddr(h, msg, h->index[i].data, imsg);
    }
}
#else	/* HAVE_MCHECK_H */
#define	HEADERPROBE(_h, _msg)
#endif	/* HAVE_MCHECK_H */

/**
 * Return length of entry data.
 * @param type		entry data type
 * @param p		entry data
 * @param count		entry item count
 * @param onDisk	data is concatenated strings (with NUL's))?
 * @return		no. bytes in data
 */
static int dataLength(int_32 type, const void * p, int_32 count, int onDisk)
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
	fprintf(stderr, _("dataLength() RPM_STRING_TYPE count must be 1.\n"));
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
	fprintf(stderr, _("Data type %d not supported\n"), (int) type);
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;
    }

    return length;
}

/**
 * Swap int_32 and int_16 arrays within header region.
 * @param entry		header entry
 * @param il		no. of entries
 * @param dl		start no. bytes of data
 * @param pe		header physical entry pointer (swapped)
 * @param dataStart	header data
 * @param myoffset	translate start of data (must be 0 now)
 * @return		no. bytes of data in region
 */
static int regionSwab(struct indexEntry * entry, int il, int dl,
		const struct entryInfo * pe, char * dataStart, int myoff)
{
    for (; il > 0; il--, pe++) {
	struct indexEntry ie;
	void * t;

	ie.info.tag = ntohl(pe->tag);
	ie.info.type = ntohl(pe->type);
assert(ie.info.type >= RPM_MIN_TYPE && ie.info.type <= RPM_MAX_TYPE);
	ie.info.count = ntohl(pe->count);
	ie.info.offset = ntohl(pe->offset);
	ie.data = t = dataStart + ie.info.offset + myoff;
	ie.length = dataLength(ie.info.type, ie.data, ie.info.count, 1);

	if (entry) {
	    ie.info.offset = ((char *)pe - dataStart);	/* negative offset */
	    *entry = ie;	/* structure assignment */
	    entry++;
	}

	dl += ie.length;

	/* Perform endian conversions */
	switch (ntohl(pe->type)) {
	case RPM_INT32_TYPE:
	    for (; ie.info.count > 0; ie.info.count--, ((int_32 *)t) += 1)
		*((int_32 *)t) = htonl(*((int_32 *)t));
	    break;
	case RPM_INT16_TYPE:
	    for (; ie.info.count > 0; ie.info.count--, ((int_16 *)t) += 1)
		*((int_16 *)t) = htons(*((int_16 *)t));
	    break;
	}
    }
    return dl;
}

/**
 * Retrieve data from header entry.
 * @param entry		header entry
 * @retval type		address of type (or NULL)
 * @retval p		address of data (or NULL)
 * @retval c		address of count (or NULL)
 * @param minMem	string pointers refer to header memory?
 */
static void copyEntry(const struct indexEntry * entry, /*@out@*/ int_32 * type,
	/*@out@*/ const void ** p, /*@out@*/ int_32 * c, int minMem)
		/*@modifies *type, *p, *c @*/
{
    if (type) 
	*type = entry->info.type;
    if (c) 
	*c = entry->info.count;
    if (p == NULL)
	return;

    /* Now look it up */
    switch (entry->info.type) {
    case RPM_BIN_TYPE:
	*p = (!minMem
		? memcpy(xmalloc(entry->length), entry->data, entry->length)
		: entry->data);
	break;
    case RPM_STRING_TYPE:
	if (entry->info.count == 1) {
	    *p = entry->data;
	    break;
	}
	/*@fallthrough@*/
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	const char ** ptrEntry;
	char * t;
	int i = entry->info.count;
	int tableSize = i * sizeof(char *);

	if (minMem) {
	    ptrEntry = *p = xmalloc(tableSize);
	    t = entry->data;
	} else {
	    t = xmalloc(tableSize + entry->length);
	    ptrEntry = *p = (void *)t;
	    t += tableSize;
	    memcpy(t, entry->data, entry->length);
	}
	while (i--) {
	    *ptrEntry++ = t;
	    t = strchr(t, 0);
	    t++;
	}
    }	break;

    default:
	*p = entry->data;
	break;
    }
}

/**
 * Header tag iterator data structure.
 */
struct headerIteratorS {
    Header h;		/*!< Header being iterated. */
    int next_index;	/*!< Next tag index. */
};

HeaderIterator headerInitIterator(Header h)
{
    HeaderIterator hi = xmalloc(sizeof(struct headerIteratorS));

    headerSort(h);

    hi->h = headerLink(h);
    hi->next_index = 0;
    return hi;
}

void headerFreeIterator(HeaderIterator iter)
{
    headerFree(iter->h);
    free(iter);
}

int headerNextIterator(HeaderIterator hi,
		 int_32 * tag, int_32 * type, const void ** p, int_32 * c)
{
    Header h = hi->h;
    int slot = hi->next_index;
    struct indexEntry * entry; 

    if (slot >= h->indexUsed)
	return 0;
    hi->next_index++;
    entry = h->index + slot;

    if (tag)
	*tag = entry->info.tag;

    copyEntry(entry, type, p, c, 0);

    return 1;
}

static int indexCmp(const void *avp, const void *bvp)	/*@*/
{
    const struct indexEntry * ap = avp, * bp = bvp;
    return (ap->info.tag - bp->info.tag);
}

void headerSort(Header h)
{
    if (!h->sorted) {
	qsort(h->index, h->indexUsed, sizeof(*h->index), indexCmp);
	h->sorted = 1;
    }
}

static int offsetCmp(const void *avp, const void *bvp) /*@*/
{
    const struct indexEntry * ap = avp, * bp = bvp;
    int rc = (ap->info.offset - bp->info.offset);

    if (rc == 0)
	rc = (ap->info.tag - bp->info.tag);
    return rc;
}

void headerUnsort(Header h)
{
    qsort(h->index, h->indexUsed, sizeof(*h->index), offsetCmp);
}

Header headerCopy(Header h)
{
    Header res = headerNew();
    HeaderIterator hi;
    int_32 tag, type, count;
    const void *ptr;
    int prevtag = 0;
   
    for (hi = headerInitIterator(h);
	headerNextIterator(hi, &tag, &type, &ptr, &count);
	ptr = headerFreeData((void *)ptr, type))
    {
	if (prevtag && prevtag > tag)
	    res->sorted = 0;
	prevtag = tag;
	headerAddEntry(res, tag, type, ptr, count);
    }
    headerFreeIterator(hi);

    return res;
}

Header headerLoad(void *pv)
{
    int_32 *ei = (int_32 *) pv;
    int_32 il = ntohl(ei[0]);		/* index length */
    int_32 dl = ntohl(ei[1]);		/* data length */
    int pvlen = sizeof(il) + sizeof(dl) +
               (il * sizeof(struct entryInfo)) + dl;
    Header h = xcalloc(1, sizeof(*h));

    struct entryInfo * pe = (struct entryInfo *) &ei[2];
    char * dataStart = (char *) (pe + il);
    struct indexEntry * entry; 
    int i;

    h->indexAlloced = il + 1;
    h->indexUsed = il;
    h->index = xcalloc(h->indexAlloced, sizeof(*h->index));
    h->sorted = 1;
    h->reloaded = 0;
    h->nrefs = 1;

    entry = h->index;
    i = 0;
    if (!(htonl(pe->tag) < HEADER_I18NTABLE)) {
	entry->info.type = REGION_TAG_TYPE;
	entry->info.tag = HEADER_IMAGE;
	entry->info.count = REGION_TAG_COUNT;
	entry->info.offset = ((char *)(pe-1) - dataStart); /* negative offset */
	entry->data = pe;
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	entry++;
	regionSwab(entry, il, 0, pe, dataStart, 0);
	i++;
	il++;
	h->indexUsed++;
    } else {
	int_32 * stei = memcpy(alloca(REGION_TAG_COUNT), dataStart + ntohl(pe->offset), REGION_TAG_COUNT);
	int_32 rdl = -ntohl(stei[2]);	/* negative offset */
	int_32 ril = rdl/sizeof(*pe);
	int dlen;
	int j = 0;

	entry->info.type = htonl(pe->type);
	if (entry->info.type < RPM_MIN_TYPE || entry->info.type > RPM_MAX_TYPE)
	    return NULL;
	entry->info.tag = htonl(pe->tag);
	entry->info.count = htonl(pe->count);
	entry->info.offset = -rdl;	/* negative offset */

	entry->length = pvlen - sizeof(il) - sizeof(dl);
	dlen = regionSwab(entry+1, ril-1, 0, pe+1, dataStart, 0);
	entry->data = dataStart + entry->info.offset;

	j += ril;

	if (j < h->indexUsed) {
	    dlen = regionSwab(entry+ril, h->indexUsed-ril, dlen, pe+ril, dataStart, 0);
	}
    }

    h->sorted = 0;
    headerSort(h);

    return h;
}

static /*@only@*/ void * doHeaderUnload(Header h, /*@out@*/ int * lengthPtr)
	/*@modifies h, *lengthPtr @*/
{
    int_32 * ei;
    struct entryInfo * pe;
    char * dataStart;
    char * te;
    unsigned pad = 0;
    unsigned len;
    int_32 il;
    int_32 dl;
    struct indexEntry * entry; 
    int i;

    /* Sort entries by (region,tag) */
    headerUnsort(h);

    /* XXX Hack to avoid the headerSort() undoing headerUnsort. */
    i = h->sorted;
    h->sorted = 1;
    len = headerSizeof(h, HEADER_MAGIC_NO);
    h->sorted = i;

    il = h->indexUsed;
    dl = len - (sizeof(il) + sizeof(dl) + (il * sizeof(struct entryInfo)));
    entry = h->index;
    i = 0;

    ei = xmalloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);

    pe = (struct entryInfo *) &ei[2];
    dataStart = te = (char *) (pe + il);

    for (; i < h->indexUsed; i++, entry++, pe++) {
const char *t;
	const char * src;
	int_32 type;
	int count;
	int dlen;

t = te;
	if (ENTRY_IS_REGION(entry)) {
	    int_32 rdl = -entry->info.offset;	/* negative offset */
	    int_32 ril = rdl/sizeof(*pe);

src = (char *)entry->data;
dlen = (entry->length - rdl + sizeof(*pe));

	    if (!(h->reloaded || entry->info.tag == HEADER_IMAGE)) {
		src += sizeof(*pe);
		dlen -= REGION_TAG_COUNT;
	    }

	    memcpy(pe+1, src,  (rdl - sizeof(*pe)));
	    memcpy(te, src + rdl - sizeof(*pe), dlen);
	    count = regionSwab(NULL, ril-1, 0, pe+1, te, 0);
	    te += dlen;

	    pe->tag = htonl(entry->info.tag);
	    pe->type = htonl(entry->info.type);
	    pe->count = htonl(entry->info.count);

	    if (!(h->reloaded || entry->info.tag == HEADER_IMAGE)) {
		pe->offset = htonl(te - dataStart - REGION_TAG_COUNT);
	    } else {
		int_32 * stei = memcpy(alloca(REGION_TAG_COUNT), te, REGION_TAG_COUNT);
		stei[0] = pe->tag;
		stei[1] = pe->type;
		stei[2] = htonl(-rdl);	/* YYY +REGION_TAG_COUNT */
		stei[3] = pe->count;
		memcpy(te, stei, REGION_TAG_COUNT);
		pe->offset = htonl(te - dataStart);
		te += REGION_TAG_COUNT;
	    }

	    i += (ril-1);
	    entry += (ril-1);
	    pe += (ril-1);
	    continue;
	}

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

	pe->tag = htonl(entry->info.tag);
	pe->type = htonl(entry->info.type);
	pe->count = htonl(entry->info.count);
	pe->offset = htonl(te - dataStart);

	/* copy data w/ endian conversions */
	switch (entry->info.type) {
	case RPM_INT32_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int_32 *)te) = htonl(*((int_32 *)src));
		te += sizeof(int_32);
		src += sizeof(int_32);
	    }
	    break;

	case RPM_INT16_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int_16 *)te) = htons(*((int_16 *)src));
		te += sizeof(int_16);
		src += sizeof(int_16);
	    }
	    break;

	default:
	    memcpy(te, entry->data, entry->length);
	    te += entry->length;
	    break;
	}
    }
   
    /* Insure that there are no memcpy dribbles. */
    assert(((char *)pe) == dataStart);
    assert((((char *)ei)+len) == te);

    if (lengthPtr)
	*lengthPtr = len;

    h->sorted = 0;
    headerSort(h);

    return (void *)ei;
}

void *headerUnload(Header h)
{
    int length;
    void * uh = doHeaderUnload(h, &length);
    return uh;
}

Header headerReload(Header h, int tag)
{
    Header nh;
    int length;
    void * uh = doHeaderUnload(h, &length);

    headerFree(h);
    nh = headerLoad(uh);
    nh->region_allocated = 1;
    nh->reloaded = 1;
    if (ENTRY_IS_REGION(nh->index)) {
	if (tag == HEADER_SIGNATURES || tag == HEADER_IMMUTABLE)
	    nh->index[0].info.tag = tag;
    }
    return nh;
}

int headerWrite(FD_t fd, Header h, enum hMagic magicp)
{
    int length;
    const void * uh;
    ssize_t nb;

    uh = doHeaderUnload(h, &length);
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
    free((void *)uh);
    return (nb == length ? 0 : 1);
}

Header headerRead(FD_t fd, enum hMagic magicp)
{
    int_32 block[4];
    int_32 reserved;
    int_32 * ei;
    int_32 il;
    int_32 dl;
    int_32 magic;
    Header h = NULL;
    int len;
    int i;

    memset(block, 0, sizeof(block));
    i = 2;
    if (magicp == HEADER_MAGIC_YES)
	i += 2;

    if (timedRead(fd, (char *)block, i*sizeof(*block)) != (i * sizeof(*block)))
	goto exit;

    i = 0;

    if (magicp == HEADER_MAGIC_YES) {
	magic = block[i++];
	if (memcmp(&magic, header_magic, sizeof(magic)))
	    goto exit;
	reserved = block[i++];
    }
    
    il = ntohl(block[i++]);
    dl = ntohl(block[i++]);

    len = sizeof(il) + sizeof(dl) + (il * sizeof(struct entryInfo)) + dl;

    /*
     * XXX Limit total size of header to 32Mb (~16 times largest known size).
     */
    if (len > (32*1024*1024))
	goto exit;

    ei = xmalloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);
    len -= sizeof(il) + sizeof(dl);

    if (timedRead(fd, (char *)&ei[2], len) != len) {
	free(ei);
	goto exit;
    }
    
    if ((h = headerLoad(ei)) != NULL)
	h->region_allocated = 1;

exit:
    return h;
}

void headerDump(Header h, FILE *f, int flags, 
		const struct headerTagTableEntry * tags)
{
    int i;
    struct indexEntry *p;
    const struct headerTagTableEntry * tage;
    const char *tag;
    char *type;

    /* First write out the length of the index (count of index entries) */
    fprintf(f, "Entry count: %d\n", h->indexUsed);

    /* Now write the index */
    p = h->index;
    fprintf(f, "\n             CT  TAG                  TYPE         "
		"      OFSET      COUNT\n");
    for (i = 0; i < h->indexUsed; i++) {
	switch (p->info.type) {
	case RPM_NULL_TYPE:   		type = "NULL_TYPE"; 	break;
	case RPM_CHAR_TYPE:   		type = "CHAR_TYPE"; 	break;
	case RPM_BIN_TYPE:   		type = "BIN_TYPE"; 	break;
	case RPM_INT8_TYPE:   		type = "INT8_TYPE"; 	break;
	case RPM_INT16_TYPE:  		type = "INT16_TYPE"; 	break;
	case RPM_INT32_TYPE:  		type = "INT32_TYPE"; 	break;
	/*case RPM_INT64_TYPE:  	type = "INT64_TYPE"; 	break;*/
	case RPM_STRING_TYPE: 	    	type = "STRING_TYPE"; 	break;
	case RPM_STRING_ARRAY_TYPE: 	type = "STRING_ARRAY_TYPE"; break;
	case RPM_I18NSTRING_TYPE: 	type = "I18N_STRING_TYPE"; break;
	default:		    	type = "(unknown)";	break;
	}

	tage = tags;
	while (tage->name && tage->val != p->info.tag) tage++;

	if (!tage->name)
	    tag = "(unknown)";
	else
	    tag = tage->name;

	fprintf(f, "Entry      : %.3d (%d)%-14s %-18s 0x%.8x %.8d\n", i,
		p->info.tag, tag, type, (unsigned) p->info.offset, (int) 
		p->info.count);

	if (flags & HEADER_DUMP_INLINE) {
	    char *dp = p->data;
	    int c = p->info.count;
	    int ct = 0;

	    /* Print the data inline */
	    switch (p->info.type) {
	    case RPM_INT32_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%08x (%d)\n", ct++,
			    (unsigned) *((int_32 *) dp),
			    (int) *((int_32 *) dp));
		    dp += sizeof(int_32);
		}
		break;

	    case RPM_INT16_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%04x (%d)\n", ct++,
			    (unsigned) (*((int_16 *) dp) & 0xffff),
			    (int) *((int_16 *) dp));
		    dp += sizeof(int_16);
		}
		break;
	    case RPM_INT8_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%02x (%d)\n", ct++,
			    (unsigned) (*((int_8 *) dp) & 0xff),
			    (int) *((int_8 *) dp));
		    dp += sizeof(int_8);
		}
		break;
	    case RPM_BIN_TYPE:
		while (c > 0) {
		    fprintf(f, "       Data: %.3d ", ct);
		    while (c--) {
			fprintf(f, "%02x ", (unsigned) (*(int_8 *)dp & 0xff));
			ct++;
			dp += sizeof(int_8);
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
		exit(EXIT_FAILURE);
		/*@notreached@*/ break;
	    }
	}
	p++;
    }
}

/**
 * Find matching (tag,type) entry in header.
 * @param h		header
 * @param tag		entry tag
 * @param type		entry type
 * @return 		header entry
 */
static struct indexEntry *findEntry(Header h, int_32 tag, int_32 type)
{
    struct indexEntry * entry, * entry2, * last;
    struct indexEntry key;

    if (!h->sorted) headerSort(h);

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
    while (entry2->info.tag == tag && entry2->info.type != type &&
	   entry2 < last) entry2++;

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    return NULL;
}

int headerIsEntry(Header h, int_32 tag)
{
    return (findEntry(h, tag, RPM_NULL_TYPE) ? 1 : 0);
}

int headerGetRawEntry(Header h, int_32 tag, int_32 * type, const void ** p,
			int_32 *c)
{
    struct indexEntry * entry;

    if (p == NULL) return headerIsEntry(h, tag);

    /* First find the tag */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) {
	if (p) *p = NULL;
	if (c) *c = 0;
	return 0;
    }

    copyEntry(entry, type, p, c, 0);

    return 1;
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
	;
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    /* Next, try stripping optional codeset and matching.  */
    for (fe = l; fe < le && *fe != '.'; fe++)
	;
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    /* Finally, try stripping optional country code and matching. */
    for (fe = l; fe < le && *fe != '_'; fe++)
	;
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
/*@dependent@*/ static char *
headerFindI18NString(Header h, struct indexEntry *entry)
{
    const char *lang, *l, *le;
    struct indexEntry * table;

    /* XXX Drepper sez' this is the order. */
    if ((lang = getenv("LANGUAGE")) == NULL &&
	(lang = getenv("LC_ALL")) == NULL &&
        (lang = getenv("LC_MESSAGES")) == NULL &&
	(lang = getenv("LANG")) == NULL)
	    return entry->data;
    
    if ((table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE)) == NULL)
	return entry->data;

    for (l = lang; *l; l = le) {
	const char *td;
	char *ed;
	int langNum;

	while (*l && *l == ':')			/* skip leading colons */
	    l++;
	if (*l == '\0')
	    break;
	for (le = l; *le && *le != ':'; le++)	/* find end of this locale */
	    ;

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
static int intGetEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
	/*@out@*/ const void **p, /*@out@*/ int_32 *c, int minMem)
		/*@modifies *type, *p, *c @*/
{
    struct indexEntry * entry;

    HEADERPROBE(h, "intGetEntry");
    /* First find the tag */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (entry == NULL) {
	if (type) type = 0;
	if (p) *p = NULL;
	if (c) *c = 0;
	return 0;
    }

    switch (entry->info.type) {
    case RPM_I18NSTRING_TYPE:
	if (type) *type = RPM_STRING_TYPE;
	if (c) *c = 1;
	/*@-dependenttrans@*/
	if (p) *p = headerFindI18NString(h, entry);
	/*@=dependenttrans@*/
	break;
    default:
	copyEntry(entry, type, p, c, minMem);
	break;
    }

    return 1;
}

int headerGetEntryMinMemory(Header h, int_32 tag, int_32 *type, const void **p, 
			    int_32 *c)
{
    return intGetEntry(h, tag, type, p, c, 1);
}

int headerGetEntry(Header h, int_32 tag, int_32 * type, void **p, int_32 * c)
{
    return intGetEntry(h, tag, type, (const void **)p, c, 0);
}

Header headerNew()
{
    Header h = xcalloc(1, sizeof(*h));

    h->indexAlloced = INDEX_MALLOC_SIZE;
    h->indexUsed = 0;
    h->region_allocated = 1;
    h->sorted = 1;
    h->reloaded = 0;
    h->nrefs = 1;

    h->index = (h->indexAlloced
	? xcalloc(h->indexAlloced, sizeof(*h->index))
	: NULL);

    return h;
}

void headerFree(Header h)
{

    if (h == NULL || --h->nrefs > 0)
	return;

    if (h->index) {
	struct indexEntry * entry = h->index;
	int i;
	for (i = 0; i < h->indexUsed; i++, entry++) {
	    if (h->region_allocated && ENTRY_IS_REGION(entry)) {
		if (entry->length > 0) {
		    int_32 * ei = entry->data;
		    ei -= 2; /* XXX HACK: adjust to beginning of header. */
		    free(ei);
		}
	    } else if (!ENTRY_IN_REGION(entry)) {
		free(entry->data);
	    }
	    entry->data = NULL;
	}
	free(h->index);
	h->index = NULL;
    }

    /*@-refcounttrans@*/ free(h); /*@=refcounttrans@*/
}

Header headerLink(Header h)
{
    HEADERPROBE(h, "headerLink");
    h->nrefs++;
    /*@-refcounttrans@*/ return h; /*@=refcounttrans@*/
}

int headerUsageCount(Header h)
{
    return h->nrefs;
}

unsigned int headerSizeof(Header h, enum hMagic magicp)
{
    struct indexEntry * entry;
    unsigned int size = 0, rsize = 0, pad = 0;
    int i;

    headerSort(h);

    size += sizeof(int_32);	/* count of index entries */
    size += sizeof(int_32);	/* length of data */
    size += sizeof(struct entryInfo) * h->indexUsed;
    switch (magicp) {
    case HEADER_MAGIC_YES:
	size += sizeof(header_magic);
	break;
    case HEADER_MAGIC_NO:
	break;
    }

    entry = h->index;
    for (i = 0; i < h->indexUsed; i++, entry++) {
	unsigned diff;
	int_32 type;

        if (ENTRY_IS_REGION(entry)) {
            if (entry->info.count > 0)
                rsize += entry->info.count;
            continue;
        }

	/* Alignment */
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    diff = typeSizes[type] - (size % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		size += diff;
		pad += diff;
	    }
	}

	size += entry->length;
    }

    size += rsize;
   
    return size;
}

static void copyData(int_32 type, /*@out@*/ void * dstPtr, const void * srcPtr,
	int_32 c, int dataLength)
		/*@modifies *dstPtr @*/
{
    const char ** src;
    char * dst;
    int i, len;

    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
	/* Otherwise, p is char** */
	i = c;
	src = (const char **) srcPtr;
	dst = dstPtr;
	while (i--) {
	    len = *src ? strlen(*src) + 1 : 0;
	    memcpy(dst, *src, len);
	    dst += len;
	    src++;
	}
	break;

    default:
	memcpy(dstPtr, srcPtr, dataLength);
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
static void * grabData(int_32 type, const void * p, int_32 c,
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

int headerAddEntry(Header h, int_32 tag, int_32 type, const void *p, int_32 c)
{
    struct indexEntry *entry;

    if (c <= 0) {
	fprintf(stderr, _("Bad count for headerAddEntry(): %d\n"), (int) c);
	exit(EXIT_FAILURE);
	/*@notreached@*/
    }

    /* Allocate more index space if necessary */
    if (h->indexUsed == h->indexAlloced) {
	h->indexAlloced += INDEX_MALLOC_SIZE;
	h->index = xrealloc(h->index,
			h->indexAlloced * sizeof(struct indexEntry));
    }

    /* Fill in the index */
    entry = h->index + h->indexUsed;
    entry->info.tag = tag;
    entry->info.type = type;
    entry->info.count = c;
    entry->info.offset = 0;
    entry->data = grabData(type, p, c, &entry->length);

    if (h->indexUsed > 0 && tag < h->index[h->indexUsed-1].info.tag)
	h->sorted = 0;
    h->indexUsed++;

    return 1;
}

char **
headerGetLangs(Header h)
{
    char **s, *e, **table;
    int i, type, count;

    if (!headerGetRawEntry(h, HEADER_I18NTABLE, &type, (const void **)&s, &count))
	return NULL;

    if ((table = (char **)xcalloc((count+1), sizeof(char *))) == NULL)
	return NULL;

    for (i = 0, e = *s; i < count > 0; i++, e += strlen(e)+1)
	table[i] = e;
    table[count] = NULL;

    return table;
}

int headerAddI18NString(Header h, int_32 tag, const char * string, const char * lang)
{
    struct indexEntry * table, * entry;
    char * chptr;
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
	    /*@-observertrans@*/
	    charArray[count++] = "C";
	    /*@=observertrans@*/
	} else {
	    /*@-observertrans@*/
	    charArray[count++] = "C";
	    /*@=observertrans@*/
	    charArray[count++] = lang;
	}
	if (!headerAddEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE, 
			&charArray, count))
	    return 0;
	table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE);
    }

    if (!lang) lang = "C";

    chptr = table->data;
    for (langNum = 0; langNum < table->info.count; langNum++) {
	if (!strcmp(chptr, lang)) break;
	chptr += strlen(chptr) + 1;
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
	memcpy(((char *)table->data) + table->length, lang, length);
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
	strcpy(((char *)entry->data) + entry->length + ghosts, string);

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

	/* Replace I18N string array */
	entry->length -= strlen(be) + 1;
	entry->length += sn;
	
	if (ENTRY_IN_REGION(entry)) {
	    entry->info.offset = 0;
	} else
	    free(entry->data);
	entry->data = buf;
    }

    return 0;
}

/* if there are multiple entries with this tag, the first one gets replaced */
int headerModifyEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c)
{
    struct indexEntry *entry;
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

    if (ENTRY_IN_REGION(entry)) {
	entry->info.offset = 0;
    } else
	free(oldData);

    return 1;
}

int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type,
			   void * p, int_32 c)
{
    return (findEntry(h, tag, type)
	? headerAppendEntry(h, tag, type, p, c)
	: headerAddEntry(h, tag, type, p, c));
}

int headerAppendEntry(Header h, int_32 tag, int_32 type, void * p, int_32 c)
{
    struct indexEntry *entry;
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

int headerRemoveEntry(Header h, int_32 tag)
{
    struct indexEntry * last = h->index + h->indexUsed;
    struct indexEntry * entry, * first;
    int ne;

    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) return 1;

    /* Make sure entry points to the first occurence of this tag. */
    while (entry > h->index && (entry - 1)->info.tag == tag)  
	entry--;

    /* Free data for tags being removed. */
    for (first = entry; first < last; first++) {
	if (first->info.tag != tag)
	    break;
	if (ENTRY_IN_REGION(first))
	    continue;
	free(first->data);
	first->data = NULL;
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

static void freeFormat( /*@only@*/ struct sprintfToken * format, int num)
{
    int i;

    for (i = 0; i < num; i++) {
	switch (format[i].type) {
	case PTOK_ARRAY:
	    freeFormat(format[i].u.array.format, format[i].u.array.numTokens);
	    break;
	case PTOK_COND:
	    freeFormat(format[i].u.cond.ifFormat, 
			format[i].u.cond.numIfTokens);
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
    free(format);
}

static void findTag(char * name, const struct headerTagTableEntry * tags, 
		    const struct headerSprintfExtension * extensions,
		    /*@out@*/const struct headerTagTableEntry ** tagMatch,
		    /*@out@*/const struct headerSprintfExtension ** extMatch)
	/*@modifies *tagMatch, *extMatch @*/
{
    const struct headerTagTableEntry * entry;
    const struct headerSprintfExtension * ext;
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
	if (ext->type == HEADER_EXT_TAG && !strcasecmp(ext->name, tagname))
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
	if (!strcasecmp(entry->name, tagname)) break;

    if (entry->name) {
	*tagMatch = entry;
	return;
    }
}

/* forward ref */
static int parseExpression(struct sprintfToken * token, char * str, 
	const struct headerTagTableEntry * tags, 
	const struct headerSprintfExtension * extensions,
	/*@out@*/char ** endPtr, /*@out@*/const char ** errmsg)
		/*@modifies str, *str, *token, *endPtr, *errmsg @*/;

static int parseFormat(char * str, const struct headerTagTableEntry * tags,
	const struct headerSprintfExtension * extensions,
	/*@out@*/struct sprintfToken ** formatPtr, /*@out@*/int * numTokensPtr,
	/*@out@*/char ** endPtr, int state, /*@out@*/const char ** errmsg)
	  /*@modifies str, *str, *formatPtr, *numTokensPtr, *endPtr, *errmsg @*/
{
    char * chptr, * start, * next, * dst;
    struct sprintfToken * format;
    int numTokens;
    int currToken;
    const struct headerTagTableEntry * tag;
    const struct headerSprintfExtension * ext;
    int i;
    int done = 0;

    /* upper limit on number of individual formats */
    numTokens = 0;
    for (chptr = str; *chptr; chptr++)
	if (*chptr == '%') numTokens++;
    numTokens = numTokens * 2 + 1;

    format = xcalloc(numTokens, sizeof(*format));
    if (endPtr) *endPtr = NULL;

    /*@-infloops@*/
    dst = start = str;
    currToken = -1;
    while (*start) {
	switch (*start) {
	case '%':
	    /* handle %% */
	    if (*(start + 1) == '%') {
		if (currToken < 0 || format[currToken].type != PTOK_STRING) {
		    currToken++;
		    format[currToken].type = PTOK_STRING;
		    dst = format[currToken].u.string.string = start;
		}

		start++;

		*dst++ = *start++;

		break; /* out of switch */
	    } 

	    currToken++;
	    *dst++ = '\0';
	    start++;

	    if (*start == '|') {
		char * newEnd;

		start++;
		if (parseExpression(format + currToken, start, tags, 
				    extensions, &newEnd, errmsg)) {
		    freeFormat(format, numTokens);
		    return 1;
		}
		start = newEnd;
		break; /* out of switch */
	    }

	    format[currToken].u.tag.format = start;
	    format[currToken].u.tag.pad = 0;
	    format[currToken].u.tag.justOne = 0;
	    format[currToken].u.tag.arrayCount = 0;

	    chptr = start;
	    while (*chptr && *chptr != '{' && *chptr != '%') chptr++;
	    if (!*chptr || *chptr == '%') {
		/*@-observertrans@*/
		*errmsg = _("missing { after %");
		/*@=observertrans@*/
		freeFormat(format, numTokens);
		return 1;
	    }

	    *chptr++ = '\0';

	    while (start < chptr) {
		if (isdigit(*start)) {
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
		/*@-observertrans@*/
		*errmsg = _("missing } after %{");
		/*@=observertrans@*/
		freeFormat(format, numTokens);
		return 1;
	    }
	    *next++ = '\0';

	    chptr = start;
	    while (*chptr && *chptr != ':') chptr++;

	    if (*chptr) {
		*chptr++ = '\0';
		if (!*chptr) {
		    /*@-observertrans@*/
		    *errmsg = _("empty tag format");
		    /*@=observertrans@*/
		    freeFormat(format, numTokens);
		    return 1;
		}
		format[currToken].u.tag.type = chptr;
	    } else {
		format[currToken].u.tag.type = NULL;
	    }
	    
	    if (!*start) {
		/*@-observertrans@*/
		*errmsg = _("empty tag name");
		/*@=observertrans@*/
		freeFormat(format, numTokens);
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
		/*@-observertrans@*/
		*errmsg = _("unknown tag");
		/*@=observertrans@*/
		freeFormat(format, numTokens);
		return 1;
	    }

	    format[currToken].type = PTOK_TAG;

	    start = next;

	    break;

	case '[':
	    *dst++ = '\0';
	    *start++ = '\0';
	    currToken++;

	    if (parseFormat(start, tags, extensions, 
			    &format[currToken].u.array.format,
			    &format[currToken].u.array.numTokens,
			    &start, PARSER_IN_ARRAY, errmsg)) {
		freeFormat(format, numTokens);
		return 1;
	    }

	    if (!start) {
		/*@-observertrans@*/
		*errmsg = _("] expected at end of array");
		/*@=observertrans@*/
		freeFormat(format, numTokens);
		return 1;
	    }

	    dst = start;

	    format[currToken].type = PTOK_ARRAY;

	    break;

	case ']':
	case '}':
	    if ((*start == ']' && state != PARSER_IN_ARRAY) ||
	        (*start == '}' && state != PARSER_IN_EXPR)) {
		if (*start == ']')
		    /*@-observertrans@*/
		    *errmsg = _("unexpected ]");
		    /*@=observertrans@*/
		else
		    /*@-observertrans@*/
		    *errmsg = _("unexpected }");
		    /*@=observertrans@*/
		freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    *endPtr = start;
	    done = 1;
	    break;

	default:
	    if (currToken < 0 || format[currToken].type != PTOK_STRING) {
		currToken++;
		format[currToken].type = PTOK_STRING;
		dst = format[currToken].u.string.string = start;
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

static int parseExpression(struct sprintfToken * token, char * str, 
	const struct headerTagTableEntry * tags, 
	const struct headerSprintfExtension * extensions,
	/*@out@*/ char ** endPtr, /*@out@*/ const char ** errmsg)
{
    const struct headerTagTableEntry * tag;
    const struct headerSprintfExtension * ext;
    char * chptr;
    char * end;

    *errmsg = NULL;
    chptr = str;
    while (*chptr && *chptr != '?') chptr++;

    if (*chptr != '?') {
	/*@-observertrans@*/
	*errmsg = _("? expected in expression");
	/*@=observertrans@*/
	return 1;
    }

    *chptr++ = '\0';;

    if (*chptr != '{') {
	/*@-observertrans@*/
	*errmsg = _("{ expected after ? in expression");
	/*@=observertrans@*/
	return 1;
    }

    chptr++;

    if (parseFormat(chptr, tags, extensions, &token->u.cond.ifFormat, 
		    &token->u.cond.numIfTokens, &end, PARSER_IN_EXPR, errmsg)) 
	return 1;

    if (!*end) {
	/*@-observertrans@*/
	*errmsg = _("} expected in expression");
	/*@=observertrans@*/
	freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	token->u.cond.ifFormat = NULL;
	return 1;
    }

    chptr = end;
    if (*chptr != ':' && *chptr != '|') {
	/*@-observertrans@*/
	*errmsg = _(": expected following ? subexpression");
	/*@=observertrans@*/
	freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	token->u.cond.ifFormat = NULL;
	return 1;
    }

    if (*chptr == '|') {
	parseFormat(xstrdup(""), tags, extensions, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR, 
			errmsg);
    } else {
	chptr++;

	if (*chptr != '{') {
	    /*@-observertrans@*/
	    *errmsg = _("{ expected after : in expression");
	    /*@=observertrans@*/
	    freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.ifFormat = NULL;
	    return 1;
	}

	chptr++;

	if (parseFormat(chptr, tags, extensions, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR, 
			errmsg)) 
	    return 1;
	if (!*end) {
	    /*@-observertrans@*/
	    *errmsg = _("} expected in expression");
	    /*@=observertrans@*/
	    freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.ifFormat = NULL;
	    return 1;
	}

	chptr = end;
	if (*chptr != '|') {
	    /*@-observertrans@*/
	    *errmsg = _("| expected at end of expression");
	    /*@=observertrans@*/
	    freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.ifFormat = NULL;
	    freeFormat(token->u.cond.elseFormat, token->u.cond.numElseTokens);
	    token->u.cond.elseFormat = NULL;
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

static int getExtension(Header h, headerTagTagFunction fn,
	/*@out@*/ int_32 * typeptr, /*@out@*/ const void ** data,
	/*@out@*/ int_32 * countptr, struct extensionCache * ext)
		/*@modifies *typeptr, *data, *countptr, ext->avail @*/
{
    if (!ext->avail) {
	if (fn(h, &ext->type, &ext->data, &ext->count, &ext->freeit))
	    return 1;
	ext->avail = 1;
    }

    *typeptr = ext->type;
    *data = ext->data;
    *countptr = ext->count;

    return 0;
}

static char * formatValue(struct sprintfTag * tag, Header h, 
			  const struct headerSprintfExtension * extensions,
			  struct extensionCache * extCache, int element)
		/*@modifies h, extCache->avail @*/
{
    int len;
    char buf[20];
    int_32 count, type;
    const void * data;
    unsigned int intVal;
    char * val = NULL;
    const char ** strarray;
    int mayfree = 0;
    int countBuf;
    headerTagFormatFunction tagtype = NULL;
    const struct headerSprintfExtension * ext;

    if (tag->ext) {
	if (getExtension(h, tag->ext, &type, &data, &count, 
			 extCache + tag->extNum)) {
	    count = 1;
	    type = RPM_STRING_TYPE;	
	    data = "(none)";		/* XXX i18n? NO!, sez; gafton */
	}
    } else {
	if (!headerGetEntry(h, tag->tag, &type, (void **)&data, &count)){
	    count = 1;
	    type = RPM_STRING_TYPE;	
	    data = "(none)";		/* XXX i18n? NO!, sez; gafton */
	}

	mayfree = 1;
    }

    if (tag->arrayCount) {
	/*@-observertrans -modobserver@*/
	headerFreeData(data, type);
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
	    if (ext->type == HEADER_EXT_FORMAT && 
		!strcmp(ext->name, tag->type)) {
		tagtype = ext->u.formatFunction;
		break;
	    }

	    if (ext->type == HEADER_EXT_MORE)
		ext = ext->u.more;
	    else
		ext++;
	}
    }
    
    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
	strarray = (const char **)data;

	if (tagtype)
	    val = tagtype(RPM_STRING_TYPE, strarray[element], buf, tag->pad, 0);

	if (!val) {
	    strcat(buf, "s");

	    len = strlen(strarray[element]) + tag->pad + 20;
	    val = xmalloc(len);
	    sprintf(val, buf, strarray[element]);
	}

	/*@-observertrans -modobserver@*/
	if (mayfree) free((void *)data);
	/*@=observertrans =modobserver@*/

	break;

    case RPM_STRING_TYPE:
	if (tagtype)
	    val = tagtype(RPM_STRING_ARRAY_TYPE, data, buf, tag->pad,  0);

	if (!val) {
	    strcat(buf, "s");

	    len = strlen(data) + tag->pad + 20;
	    val = xmalloc(len);
	    sprintf(val, buf, data);
	}
	break;

    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
	switch (type) {
	case RPM_CHAR_TYPE:	
	case RPM_INT8_TYPE:	intVal = *(((int_8 *) data) + element); break;
	case RPM_INT16_TYPE:	intVal = *(((uint_16 *) data) + element); break;
	default:		/* keep -Wall quiet */
	case RPM_INT32_TYPE:	intVal = *(((int_32 *) data) + element); break;
	}

	if (tagtype)
	    val = tagtype(RPM_INT32_TYPE, &intVal, buf, tag->pad,  element);

	if (!val) {
	    strcat(buf, "d");
	    len = 10 + tag->pad + 20;
	    val = xmalloc(len);
	    sprintf(val, buf, intVal);
	}
	break;

    default:
	val = xstrdup(_("(unknown type)"));
	break;
    }

    return val;
}

static const char * singleSprintf(Header h, struct sprintfToken * token,
			    const struct headerSprintfExtension * extensions,
			    struct extensionCache * extCache, int element)
		/*@modifies h, extCache->avail @*/
{
    char * val;
    const char * thisItem;
    int thisItemLen;
    int len, alloced;
    int i, j;
    int numElements;
    int type;
    struct sprintfToken * condFormat;
    int condNumFormats;

    /* we assume the token and header have been validated already! */

    switch (token->type) {
    case PTOK_NONE:
	break;

    case PTOK_STRING:
	val = xmalloc(token->u.string.len + 1);
	strcpy(val, token->u.string.string);
	break;

    case PTOK_TAG:
	val = formatValue(&token->u.tag, h, extensions, extCache,
			  token->u.tag.justOne ? 0 : element);
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

	alloced = condNumFormats * 20;
	val = xmalloc(alloced ? alloced : 1);
	*val = '\0';
	len = 0;

	for (i = 0; i < condNumFormats; i++) {
	    thisItem = singleSprintf(h, condFormat + i, 
				     extensions, extCache, element);
	    thisItemLen = strlen(thisItem);
	    if ((thisItemLen + len) >= alloced) {
		alloced = (thisItemLen + len) + 200;
		val = xrealloc(val, alloced);	
	    }
	    strcat(val, thisItem);
	    len += thisItemLen;
	    free((void *)thisItem);
	}

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
				    &type, (void **) &val, &numElements))
		    continue;
		headerFreeData(val, type);
	    } 
	    break;
	}

	if (numElements == -1) {
	    val = xstrdup("(none)");	/* XXX i18n? NO!, sez; gafton */
	} else {
	    alloced = numElements * token->u.array.numTokens * 20;
	    val = xmalloc(alloced);
	    *val = '\0';
	    len = 0;

	    for (j = 0; j < numElements; j++) {
		for (i = 0; i < token->u.array.numTokens; i++) {
		    thisItem = singleSprintf(h, token->u.array.format + i, 
					     extensions, extCache, j);
		    thisItemLen = strlen(thisItem);
		    if ((thisItemLen + len) >= alloced) {
			alloced = (thisItemLen + len) + 200;
			val = xrealloc(val, alloced);	
		    }
		    strcat(val, thisItem);
		    len += thisItemLen;
		    free((void *)thisItem);
		}
	    }
	}
	   
	break;
    }

    return val;
}

static struct extensionCache * allocateExtensionCache(
		     const struct headerSprintfExtension * extensions)
		/*@*/
{
    const struct headerSprintfExtension * ext = extensions;
    int i = 0;

    while (ext->type != HEADER_EXT_LAST) {
	i++;
	if (ext->type == HEADER_EXT_MORE)
	    ext = ext->u.more;
	else
	    ext++;
    }

    return xcalloc(i, sizeof(struct extensionCache));
}

static void freeExtensionCache(const struct headerSprintfExtension * extensions,
		        /*@only@*/struct extensionCache * cache)
{
    const struct headerSprintfExtension * ext = extensions;
    int i = 0;

    while (ext->type != HEADER_EXT_LAST) {
	if (cache[i].freeit) free((void *)cache[i].data);

	i++;
	if (ext->type == HEADER_EXT_MORE)
	    ext = ext->u.more;
	else
	    ext++;
    }

    free(cache);
}

char * headerSprintf(Header h, const char * origFmt, 
		     const struct headerTagTableEntry * tags,
		     const struct headerSprintfExtension * extensions,
		     const char ** errmsg)
{
    char * fmtString;
    struct sprintfToken * format;
    int numTokens;
    char * answer;
    int answerLength;
    int answerAlloced;
    int i;
    struct extensionCache * extCache;
 
    /*fmtString = escapeString(origFmt);*/
    fmtString = xstrdup(origFmt);
   
    if (parseFormat(fmtString, tags, extensions, &format, &numTokens, 
		    NULL, PARSER_BEGIN, errmsg)) {
	free(fmtString);
	return NULL;
    }

    extCache = allocateExtensionCache(extensions);

    answerAlloced = 1024;
    answerLength = 0;
    answer = xmalloc(answerAlloced);
    *answer = '\0';

    for (i = 0; i < numTokens; i++) {
	const char * piece;
	int pieceLength;

	piece = singleSprintf(h, format + i, extensions, extCache, 0);
	if (piece) {
	    pieceLength = strlen(piece);
	    if ((answerLength + pieceLength) >= answerAlloced) {
		while ((answerLength + pieceLength) >= answerAlloced) 
		    answerAlloced += 1024;
		answer = xrealloc(answer, answerAlloced);
	    }

	    strcat(answer, piece);
	    answerLength += pieceLength;
	    free((void *)piece);
	}
    }

    free(fmtString);
    freeExtensionCache(extensions, extCache);
    free(format);

    return answer;
}

static char * octalFormat(int_32 type, const void * data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
		/*@modifies formatPrefix @*/
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(20 + padding);
	strcat(formatPrefix, "o");
	sprintf(val, formatPrefix, *((int_32 *) data));
    }

    return val;
}

static char * hexFormat(int_32 type, const void * data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
		/*@modifies formatPrefix @*/
{
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(20 + padding);
	strcat(formatPrefix, "x");
	sprintf(val, formatPrefix, *((int_32 *) data));
    }

    return val;
}

static char * realDateFormat(int_32 type, const void * data, 
		char * formatPrefix, int padding, /*@unused@*/int element,
		char * strftimeFormat)
		/*@modifies formatPrefix @*/
{
    char * val;
    struct tm * tstruct;
    char buf[50];

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(50 + padding);
	strcat(formatPrefix, "s");

	/* this is important if sizeof(int_32) ! sizeof(time_t) */
	{   time_t dateint = *((int_32 *) data);
	    tstruct = localtime(&dateint);
	}
	(void)strftime(buf, sizeof(buf) - 1, strftimeFormat, tstruct);
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

static char * dateFormat(int_32 type, const void * data, 
		         char * formatPrefix, int padding, int element)
		/*@modifies formatPrefix @*/
{
    return realDateFormat(type, data, formatPrefix, padding, element, "%c");
}

static char * dayFormat(int_32 type, const void * data, 
		         char * formatPrefix, int padding, int element)
		/*@modifies formatPrefix @*/
{
    return realDateFormat(type, data, formatPrefix, padding, element, 
			  "%a %b %d %Y");
}

static char * shescapeFormat(int_32 type, const void * data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
		/*@modifies formatPrefix @*/
{
    char * result, * dst, * src, * buf;

    if (type == RPM_INT32_TYPE) {
	result = xmalloc(padding + 20);
	strcat(formatPrefix, "d");
	sprintf(result, formatPrefix, *((int_32 *) data));
    } else {
	buf = alloca(strlen(data) + padding + 2);
	strcat(formatPrefix, "s");
	sprintf(buf, formatPrefix, data);

	result = dst = xmalloc(strlen(buf) * 4 + 3);
	*dst++ = '\'';
	for (src = buf; *src; src++) {
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

const struct headerSprintfExtension headerDefaultFormats[] = {
    { HEADER_EXT_FORMAT, "octal", { octalFormat } },
    { HEADER_EXT_FORMAT, "hex", { hexFormat } },
    { HEADER_EXT_FORMAT, "date", { dateFormat } },
    { HEADER_EXT_FORMAT, "day", { dayFormat } },
    { HEADER_EXT_FORMAT, "shescape", { shescapeFormat } },
    { HEADER_EXT_LAST, NULL, { NULL } }
};

void headerCopyTags(Header headerFrom, Header headerTo, int *tagstocopy)
{
    int *p;

    if (headerFrom == headerTo)
	return;

    for (p = tagstocopy; *p != 0; p++) {
	char *s;
	int type, count;
	if (headerIsEntry(headerTo, *p))
	    continue;
	if (!headerGetEntryMinMemory(headerFrom, *p, &type,
				(const void **) &s, &count))
	    continue;
	headerAddEntry(headerTo, *p, type, s, count);
	headerFreeData(s, type);
    }
}
