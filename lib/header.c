/* RPM - Copyright (C) 1995 Red Hat Software
 *
 * header.c - routines for managing rpm headers
 */

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

#define INDEX_MALLOC_SIZE 8

#define PARSER_BEGIN 	0
#define PARSER_IN_ARRAY 1
#define PARSER_IN_EXPR  2

static unsigned char header_magic[4] = { 0x8e, 0xad, 0xe8, 0x01 };

/* handy -- this tells us alignments for defined elements as well */
static int typeSizes[] =  { 
	/* RPM_NULL_TYPE */		-1,
	/* RPM_CHAR_TYPE */		1,
	/* RPM_INT8_TYPE */		1,
	/* RPM_INT16_TYPE */		2,
	/* RPM_INT32_TYPE */		4,
	/* RPM_INT64_TYPE */		-1,
	/* RPM_STRING_TYPE */		-1,
	/* RPM_BIN_TYPE */		1,
	/* RPM_STRING_ARRAY_TYPE */	-1,
	/* RPM_I18NSTRING_TYPE */	-1 };

struct headerToken {
    struct indexEntry *index;
    int indexUsed;
    int indexAlloced;

    int sorted;  
    /*@refs@*/ int usageCount;
};

struct entryInfo {
    int_32 tag;
    int_32 type;
    int_32 offset;		/* Offset from beginning of data segment,
				   only defined on disk */
    int_32 count;
};

struct indexEntry {
    struct entryInfo info;
    /*@owned@*/ void * data; 
    int length;			/* Computable, but why bother? */
};

struct sprintfTag {
    /* if NULL tag element is invalid */
    headerTagTagFunction ext;   
    int extNum;
    int_32 tag;
    int justOne;
    int arrayCount;
    char * format;
    char * type;
    int pad;
};

struct extensionCache {
    int_32 type;
    int_32 count;
    int avail;
    int freeit;
    const void * data;
};

struct sprintfToken {
    enum { PTOK_NONE = 0, PTOK_TAG, PTOK_ARRAY, PTOK_STRING, PTOK_COND } type;
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

static void copyEntry(struct indexEntry * entry, /*@out@*/ int_32 * type,
	/*@out@*/ void ** p, /*@out@*/ int_32 * c, int minimizeMemory)
{
    int i, tableSize;
    char ** ptrEntry;
    char * chptr;

    if (type) 
	*type = entry->info.type;
    if (c) 
	*c = entry->info.count;
    if (p == NULL)
	return;

    /* Now look it up */
    switch (entry->info.type) {
      case RPM_STRING_TYPE:
	if (entry->info.count == 1) {
	    *p = entry->data;
	    break;
	}
	/*@fallthrough@*/
      case RPM_STRING_ARRAY_TYPE:
      case RPM_I18NSTRING_TYPE:
	i = entry->info.count;
	tableSize = i * sizeof(char *);
	if (minimizeMemory) {
	    ptrEntry = *p = xmalloc(tableSize);
	    chptr = entry->data;
	} else {
	    ptrEntry = *p = xmalloc(tableSize + entry->length);	/* XXX memory leak */
	    chptr = ((char *) *p) + tableSize;
	    memcpy(chptr, entry->data, entry->length);
	}
	while (i--) {
	    *ptrEntry++ = chptr;
	    chptr = strchr(chptr, 0);
	    chptr++;
	}
	break;

      default:
	*p = entry->data;
	break;
    }
}

static int dataLength(int_32 type, const void * p, int_32 count, int onDisk)
{
    int thisLen, length, i;
    char ** src, * chptr;

    length = 0;
    switch (type) {
      case RPM_STRING_TYPE:
	if (count == 1) {
	    /* Special case -- p is just the string */
	    length = strlen(p) + 1;
	    break;
	}
        /* This should not be allowed */
	fprintf(stderr, _("grabData() RPM_STRING_TYPE count must be 1.\n"));
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;

      case RPM_STRING_ARRAY_TYPE:
      case RPM_I18NSTRING_TYPE:
	/* This is like RPM_STRING_TYPE, except it's *always* an array */
	/* Compute sum of length of all strings, including null terminators */
	i = count;
	length = 0;

	if (onDisk) {
	    chptr = (char *) p;
	    while (i--) {
		thisLen = strlen(chptr) + 1;
		length += thisLen;
		chptr += thisLen;
	    }
	} else {
	    src = (char **) p;
	    while (i--) {
		/* add one for null termination */
		length += strlen(*src++) + 1;
	    }
	}
	break;

      default:
	if (typeSizes[type] != -1)
	    length = typeSizes[type] * count;
	else {
	    fprintf(stderr, _("Data type %d not supported\n"), (int) type);
	    exit(EXIT_FAILURE);
	    /*@notreached@*/
	}
	break;
    }

    return length;
}

/********************************************************************/
/*                                                                  */
/* Header iteration and copying                                     */
/*                                                                  */
/********************************************************************/

struct headerIteratorS {
    Header h;
    int next_index;
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

int headerNextIterator(HeaderIterator iter,
		 int_32 *tag, int_32 *type, void **p, int_32 *c)
{
    Header h = iter->h;
    int slot = iter->next_index;

    if (slot == h->indexUsed) {
	return 0;
    }
    iter->next_index++;

    if (tag) {
	*tag = h->index[slot].info.tag;
    }
    copyEntry(h->index + slot, type, p, c, 0);

    return 1;
}

static int indexCmp(const void *ap, const void *bp)
{
    int_32 a, b;

    a = ((struct indexEntry *)ap)->info.tag;
    b = ((struct indexEntry *)bp)->info.tag;
    
    if (a > b) {
	return 1;
    } else if (a < b) {
	return -1;
    } else {
	return 0;
    }
}

void headerSort(Header h)
{
    if (!h->sorted) {
	qsort(h->index, h->indexUsed, sizeof(struct indexEntry), indexCmp);
	h->sorted = 1;
    }
}

Header headerCopy(Header h)
{
    int_32 tag, type, count;
    void *ptr;
    HeaderIterator headerIter;
    Header res = headerNew();
   
#if 0	/* XXX harmless, but headerInitIterator() does this anyways */
    /* Sort the index -- not really necessary but some old apps may depend
       on this and it certainly won't hurt anything */
    headerSort(h);
#endif
    headerIter = headerInitIterator(h);

    while (headerNextIterator(headerIter, &tag, &type, &ptr, &count)) {
	headerAddEntry(res, tag, type, ptr, count);
	if (type == RPM_STRING_ARRAY_TYPE || 
	    type == RPM_I18NSTRING_TYPE) free(ptr);
    }

    res->sorted = 1;

    headerFreeIterator(headerIter);

    return res;
}

/********************************************************************/
/*                                                                  */
/* Header loading and unloading                                     */
/*                                                                  */
/********************************************************************/

Header headerLoad(void *pv)
{
    int_32 il;			/* index length, data length */
    char *p = pv;
    const char * dataStart;
    struct entryInfo * pe;
    struct indexEntry * entry; 
    struct headerToken *h = xmalloc(sizeof(struct headerToken));
    const char * src;
    char * dst;
    int i;
    int count;

    il = ntohl(*((int_32 *) p));
    p += sizeof(int_32);

    /* we can skip the data length -- we only store this to allow reading
       from disk */
    p += sizeof(int_32);

    h->indexAlloced = il;
    h->indexUsed = il;
    h->index = xmalloc(sizeof(struct indexEntry) * il);
    h->usageCount = 1;

    /* This assumes you only headerLoad() something you headerUnload()-ed */
    h->sorted = 1;

    pe = (struct entryInfo *) p;
    dataStart = (char *) (pe + h->indexUsed);

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++, pe++) {
	entry->info.type = htonl(pe->type);
	entry->info.tag = htonl(pe->tag);
	entry->info.count = htonl(pe->count);
	entry->info.offset = -1;

	if (entry->info.type < RPM_MIN_TYPE ||
	    entry->info.type > RPM_MAX_TYPE) return NULL;

	src = dataStart + htonl(pe->offset);
	entry->length = dataLength(entry->info.type, src, 
				   entry->info.count, 1);
	entry->data = dst = xmalloc(entry->length);

	/* copy data w/ endian conversions */
	switch (entry->info.type) {
	  case RPM_INT32_TYPE:
	    count = entry->info.count;
	    while (count--) {
		*((int_32 *)dst) = htonl(*((int_32 *)src));
		src += sizeof(int_32);
		dst += sizeof(int_32);
	    }
	    break;

	  case RPM_INT16_TYPE:
	    count = entry->info.count;
	    while (count--) {
		*((int_16 *)dst) = htons(*((int_16 *)src));
		src += sizeof(int_16);
		dst += sizeof(int_16);
	    }
	    break;

	  default:
	    memcpy(dst, src, entry->length);
	    break;
	}
    }

    return h;
}

static void *doHeaderUnload(Header h, /*@out@*/int * lengthPtr)
{
    int i;
    int type, diff;
    void *p;
    int_32 *pi;
    struct entryInfo * pe;
    struct indexEntry * entry; 
    char * chptr, * src, * dataStart;
    int count;

    headerSort(h);

    *lengthPtr = headerSizeof(h, 0);
    pi = p = xmalloc(*lengthPtr);

    *pi++ = htonl(h->indexUsed);

    /* data length */
    *pi++ = htonl(*lengthPtr - sizeof(int_32) - sizeof(int_32) -
		(sizeof(struct entryInfo) * h->indexUsed));

    pe = (struct entryInfo *) pi;
    dataStart = chptr = (char *) (pe + h->indexUsed);

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++, pe++) {
	pe->type = htonl(entry->info.type);
	pe->tag = htonl(entry->info.tag);
	pe->count = htonl(entry->info.count);

	/* Alignment */
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    diff = typeSizes[type] - ((chptr - dataStart) % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		memset(chptr, 0, diff);
		chptr += diff;
	    }
	}

	pe->offset = htonl(chptr - dataStart);

	/* copy data w/ endian conversions */
	switch (entry->info.type) {
	  case RPM_INT32_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int_32 *)chptr) = htonl(*((int_32 *)src));
		chptr += sizeof(int_32);
		src += sizeof(int_32);
	    }
	    break;

	  case RPM_INT16_TYPE:
	    count = entry->info.count;
	    src = entry->data;
	    while (count--) {
		*((int_16 *)chptr) = htons(*((int_16 *)src));
		chptr += sizeof(int_16);
		src += sizeof(int_16);
	    }
	    break;

	  default:
	    memcpy(chptr, entry->data, entry->length);
	    chptr += entry->length;
	    break;
	}
    }
   
    return p;
}

void *headerUnload(Header h)
{
    void * uh;
    int length;

    uh = doHeaderUnload(h, &length);
    return uh;
}

/********************************************************************/
/*                                                                  */
/* Reading and writing headers                                      */
/*                                                                  */
/********************************************************************/

int headerWrite(FD_t fd, Header h, int magicp)
{
    void * p;
    int length;
    int_32 l;
    ssize_t nb;

    p = doHeaderUnload(h, &length);

    if (magicp) {
	nb = Fwrite(header_magic, sizeof(char), sizeof(header_magic), fd);
	if (nb != sizeof(header_magic)) {
	    free(p);
	    return 1;
	}
	l = htonl(0);
	nb = Fwrite(&l, sizeof(char), sizeof(l), fd);
	if (nb != sizeof(l)) {
	    free(p);
	    return 1;
	}
    }
    
    nb = Fwrite(p, sizeof(char), length, fd);
    if (nb != length) {
	free(p);
	return 1;
    }

    free(p);
    return 0;
}

Header headerRead(FD_t fd, int magicp)
{
    int_32 block[40];
    int_32 reserved;
    int_32 * p;
    int_32 il, dl;
    int_32 magic;
    Header h;
    void * dataBlock;
    int totalSize;
    int i;

    i = 2;
    if (magicp == HEADER_MAGIC_YES)
	i += 2;

    if (timedRead(fd, (char *)block, i * sizeof(*block)) != (i * sizeof(*block)))
	return NULL;
    i = 0;

    if (magicp == HEADER_MAGIC_YES) {
	magic = block[i++];
	if (memcmp(&magic, header_magic, sizeof(magic))) {
	    return NULL;
	}

	reserved = block[i++];
    }
    
    il = ntohl(block[i++]);
    dl = ntohl(block[i++]);

    totalSize = sizeof(int_32) + sizeof(int_32) + 
		(il * sizeof(struct entryInfo)) + dl;

    /*
     * XXX Limit total size of header to 32Mb (~16 times largest known size).
     */
    if (totalSize > (32*1024*1024))
	return NULL;

    dataBlock = p = xmalloc(totalSize);
    *p++ = htonl(il);
    *p++ = htonl(dl);

    totalSize -= sizeof(int_32) + sizeof(int_32);
    if (timedRead(fd, (char *)p, totalSize) != totalSize)
	return NULL;
    
    h = headerLoad(dataBlock);

    free(dataBlock);

    return h;
}

int headerGzWrite(FD_t fd, Header h, int magicp)
{
    void * p;
    int length;
    int_32 l;
    ssize_t nb;

    p = doHeaderUnload(h, &length);

    if (magicp) {
	nb = Fwrite(header_magic, sizeof(char), sizeof(header_magic), fd);
	if (nb != sizeof(header_magic)) {
	    free(p);
	    return 1;
	}
	l = htonl(0);
	nb = Fwrite(&l, sizeof(char), sizeof(l), fd);
	if (nb != sizeof(l)) {
	    free(p);
	    return 1;
	}
    }
    
    nb = Fwrite(p, sizeof(char), length, fd);
    if (nb != length) {
	free(p);
	return 1;
    }

    free(p);
    return 0;
}

Header headerGzRead(FD_t fd, int magicp)
{
    int_32 reserved;
    int_32 * p;
    int_32 il, dl;
    int_32 magic;
    Header h;
    void * block;
    int totalSize;

    if (magicp == HEADER_MAGIC_YES) {
	if (Fread(&magic, sizeof(char), sizeof(magic), fd) != sizeof(magic))
	    return NULL;
	if (memcmp(&magic, header_magic, sizeof(magic))) {
	    return NULL;
	}

	if (Fread(&reserved, sizeof(char), sizeof(reserved), fd) != sizeof(reserved))
	    return NULL;
    }
    
    /* First read the index length (count of index entries) */
    if (Fread(&il, sizeof(char), sizeof(il), fd) != sizeof(il)) 
	return NULL;

    il = ntohl(il);

    /* Then read the data length (number of bytes) */
    if (Fread(&dl, sizeof(char), sizeof(dl), fd) != sizeof(dl)) 
	return NULL;

    dl = ntohl(dl);

    totalSize = sizeof(int_32) + sizeof(int_32) + 
		(il * sizeof(struct entryInfo)) + dl;

    block = p = xmalloc(totalSize);
    *p++ = htonl(il);
    *p++ = htonl(dl);

    totalSize -= sizeof(int_32) + sizeof(int_32);
    if (Fread(p, sizeof(char), totalSize, fd) != totalSize)
	return NULL;
    
    h = headerLoad(block);

    free(block);

    return h;
}

/********************************************************************/
/*                                                                  */
/* Header dumping                                                   */
/*                                                                  */
/********************************************************************/

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
	    /*case RPM_INT64_TYPE:  		type = "INT64_TYPE"; 	break;*/
	    case RPM_STRING_TYPE: 	    	type = "STRING_TYPE"; 	break;
	    case RPM_STRING_ARRAY_TYPE: 	type = "STRING_ARRAY_TYPE"; break;
	    case RPM_I18NSTRING_TYPE:	 	type = "I18N_STRING_TYPE"; break;
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

/********************************************************************/
/*                                                                  */
/* Entry lookup                                                     */
/*                                                                  */
/********************************************************************/

static struct indexEntry *findEntry(Header h, int_32 tag, int_32 type)
{
    struct indexEntry * entry, * entry2, * last;
    struct indexEntry key;

    if (!h->sorted) headerSort(h);

    key.info.tag = tag;

    entry2 = entry = 
	bsearch(&key, h->index, h->indexUsed, sizeof(struct indexEntry), 
		indexCmp);
    if (!entry) return NULL;

    if (type == RPM_NULL_TYPE) return entry;

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

int headerGetRawEntry(Header h, int_32 tag, int_32 *type, void **p, int_32 *c)
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

static int headerMatchLocale(const char *td, const char *l, const char *le)
{
    const char *fe;

    /*
     * The range [l,le) contains the next locale to match:
     *    ll[_CC][.EEEEE][@dddd]
     * where
     *    ll	ISO language code (in lowercase).
     *    CC	(optional) ISO coutnry code (in uppercase).
     *    EEEEE	(optional) encoding (not really standardized).
     *    dddd	(optional) dialect.
     */

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

static int intGetEntry(Header h, int_32 tag, /*@out@*/ int_32 *type,
	/*@out@*/ void **p, /*@out@*/ int_32 *c, int minMem)
{
    struct indexEntry * entry;
    char * chptr;

    HEADERPROBE(h, "intGetEntry");
    /* First find the tag */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) {
	if (p) *p = NULL;
	if (c) *c = 0;
	return 0;
    }

    if (entry->info.type == RPM_I18NSTRING_TYPE) {
	chptr = headerFindI18NString(h, entry);

	if (type) *type = RPM_STRING_TYPE;
	if (c) *c = 1;

	*p = chptr;
    } else {
	copyEntry(entry, type, p, c, minMem);
    }

    return 1;
}

int headerGetEntryMinMemory(Header h, int_32 tag, int_32 *type, void **p, 
			    int_32 *c)
{
    return intGetEntry(h, tag, type, p, c, 1);
}

int headerGetEntry(Header h, int_32 tag, int_32 * type, void **p, int_32 * c)
{
    return intGetEntry(h, tag, type, p, c, 0);
}

/********************************************************************/
/*                                                                  */
/* Header creation and deletion                                     */
/*                                                                  */
/********************************************************************/

Header headerNew()
{
    Header h = xmalloc(sizeof(struct headerToken));

    h->indexAlloced = INDEX_MALLOC_SIZE;
    h->index = xcalloc(h->indexAlloced, sizeof(struct indexEntry));
    h->indexUsed = 0;

    h->sorted = 0;
    h->usageCount = 1;

    return (Header) h;
}

void headerFree(Header h)
{
    int i;

    if (--h->usageCount) return;
    for (i = 0; i < h->indexUsed; i++)
	free(h->index[i].data);

    free(h->index);
    /*@-refcounttrans@*/ free(h); /*@=refcounttrans@*/
}

Header headerLink(Header h)
{
    HEADERPROBE(h, "headerLink");
    h->usageCount++;
    return h;
}

int headerUsageCount(Header h)
{
    return h->usageCount;
}

unsigned int headerSizeof(Header h, int magicp)
{
    unsigned int size;
    int i, diff;
    int_32 type;

    headerSort(h);

    size = sizeof(int_32);	/* count of index entries */
    size += sizeof(int_32);	/* length of data */
    size += sizeof(struct entryInfo) * h->indexUsed;
    if (magicp) {
	size += 8;
    }

    for (i = 0; i < h->indexUsed; i++) {
	/* Alignment */
	type = h->index[i].info.type;
	if (typeSizes[type] > 1) {
	    diff = typeSizes[type] - (size % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		size += diff;
	    }
	}

	size += h->index[i].length;
    }
   
    return size;
}

static void copyData(int_32 type, /*@out@*/void * dstPtr, const void * srcPtr, int_32 c, 
			int dataLength)
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

static void * grabData(int_32 type, const void * p, int_32 c, int * lengthPtr)
{
    int length;
    void * data;

    length = dataLength(type, p, c, 0);
    data = xmalloc(length);

    copyData(type, data, p, c, length);

    *lengthPtr = length;
    return data;
}

/********************************************************************/
/*                                                                  */
/* Adding and modifying entries                                     */
/*                                                                  */
/********************************************************************/

int headerAddEntry(Header h, int_32 tag, int_32 type, const void *p, int_32 c)
{
    struct indexEntry *entry;

    h->sorted = 0;

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
    entry = h->index + h->indexUsed++;
    entry->info.tag = tag;
    entry->info.type = type;
    entry->info.count = c;
    entry->info.offset = -1;

    entry->data = grabData(type, p, c, &entry->length);

    h->sorted = 0;

    return 1;
}

char **
headerGetLangs(Header h)
{
    char **s, *e, **table;
    int i, type, count;

    if (!headerGetRawEntry(h, HEADER_I18NTABLE, &type, (void **)&s, &count))
	return NULL;

    if ((table = (char **)xcalloc((count+1), sizeof(char *))) == NULL)
	return NULL;

    for (i = 0, e = *s; i < count > 0; i++, e += strlen(e)+1) {
	table[i] = e;
    }
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

    if (!table && entry) {
	return 0;		/* this shouldn't ever happen!! */
    }

    if (!table && !entry) {
	const char * charArray[2];
	int count = 0;
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

    if (!lang) lang = "C";

    chptr = table->data;
    for (langNum = 0; langNum < table->info.count; langNum++) {
	if (!strcmp(chptr, lang)) break;
	chptr += strlen(chptr) + 1;
    }

    if (langNum >= table->info.count) {
	length = strlen(lang) + 1;
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
    if (!entry) {
	return 0;
    }

    /* make sure entry points to the first occurence of this tag */
    while (entry > h->index && (entry - 1)->info.tag == tag)  
	entry--;

    /* free after we've grabbed the new data in case the two are intertwined;
       that's a bad idea but at least we won't break */
    oldData = entry->data;

    entry->info.count = c;
    entry->info.type = type;
    entry->data = grabData(type, p, c, &entry->length);

    free(oldData);
    
    return 1;
}

int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type,
			   void * p, int_32 c)
{
    if (findEntry(h, tag, type)) {
	return headerAppendEntry(h, tag, type, p, c);
    } else {
	return headerAddEntry(h, tag, type, p, c);
    }
}

int headerAppendEntry(Header h, int_32 tag, int_32 type, void * p, int_32 c)
{
    struct indexEntry *entry;
    int length;

    /* First find the tag */
    entry = findEntry(h, tag, type);
    if (!entry) {
	return 0;
    }

    if (type == RPM_STRING_TYPE || type == RPM_I18NSTRING_TYPE) {
	/* we can't do this */
	return 0;
    }

    length = dataLength(type, p, c, 0);

    entry->data = xrealloc(entry->data, entry->length + length);
    copyData(type, ((char *) entry->data) + entry->length, p, c, length);

    entry->length += length;

    entry->info.count += c;

    return 0;
}

int headerRemoveEntry(Header h, int_32 tag)
{
    struct indexEntry * entry, * last;

    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) return 1;

    /* make sure entry points to the first occurence of this tag */
    while (entry > h->index && (entry - 1)->info.tag == tag)  
	entry--;

    /* We might be better off just counting the number of items off the
       end and issuing one big memcpy, but memcpy() doesn't have to work
       on overlapping regions thanks to ANSI <sigh>. A alloca() and two
       memcpy() would probably still be a win (as our moving from the
       end to the middle isn't very nice to the qsort() we'll have to
       do to make up for this!), but I'm too lazy to implement it. Just
       remember that this repeating this is basically nlogn thanks to this
       dumb implementation (but n is the best we'd do anyway) */

    last = h->index + h->indexUsed;
    while (entry->info.tag == tag && entry < last) {
	free(entry->data);
	*(entry++) = *(--last);
    }
    h->indexUsed = last - h->index;

    h->sorted = 0;

    return 0;
}

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

      default:		return ch;
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
{
    const struct headerTagTableEntry * entry;
    const struct headerSprintfExtension * ext;
    char * tagname;

    *tagMatch = NULL;
    *extMatch = NULL;

    if (strncmp("RPMTAG_", name, sizeof("RPMTAG_")-1)) {
	tagname = alloca(strlen(name) + 10);
	strcpy(tagname, "RPMTAG_");
	strcat(tagname, name);
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
	/*@out@*/char ** endPtr, /*@out@*/const char ** error);

static int parseFormat(char * str, const struct headerTagTableEntry * tags,
	const struct headerSprintfExtension * extensions,
	/*@out@*/struct sprintfToken ** formatPtr, /*@out@*/int * numTokensPtr,
	/*@out@*/char ** endPtr, int state, /*@out@*/const char ** error)
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

    dst = start = str;
    currToken = -1;
    while (*start && !done) {
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
				    extensions, &newEnd, error)) {
		    freeFormat(format, numTokens);
		    return 1;
		}
		start = newEnd;
	    } else {
		format[currToken].u.tag.format = start;
		format[currToken].u.tag.pad = 0;
		format[currToken].u.tag.justOne = 0;
		format[currToken].u.tag.arrayCount = 0;

		chptr = start;
		while (*chptr && *chptr != '{' && *chptr != '%') chptr++;
		if (!*chptr || *chptr == '%') {
		    *error = _("missing { after %");
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
		    *error = _("missing } after %{");
		    freeFormat(format, numTokens);
		    return 1;
		}
		*next++ = '\0';

		chptr = start;
		while (*chptr && *chptr != ':') chptr++;

		if (*chptr) {
		    *chptr++ = '\0';
		    if (!*chptr) {
			*error = _("empty tag format");
			freeFormat(format, numTokens);
			return 1;
		    }
		    format[currToken].u.tag.type = chptr;
		} else {
		    format[currToken].u.tag.type = NULL;
		}
	    
		if (!*start) {
		    *error = _("empty tag name");
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
		    *error = _("unknown tag");
		    freeFormat(format, numTokens);
		    return 1;
		}

		format[currToken].type = PTOK_TAG;

		start = next;
	    }

	    break;

	  case '[':
	    *dst++ = '\0';
	    *start++ = '\0';
	    currToken++;

	    if (parseFormat(start, tags, extensions, 
			    &format[currToken].u.array.format,
			    &format[currToken].u.array.numTokens,
			    &start, PARSER_IN_ARRAY, error)) {
		freeFormat(format, numTokens);
		return 1;
	    }

	    if (!start) {
		*error = _("] expected at end of array");
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
		    *error = _("unexpected ]");
		else
		    *error = _("unexpected }");
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
    }

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
	/*@out@*/ char ** endPtr, /*@out@*/ const char ** error)
{
    char * chptr, * end;
    const struct headerTagTableEntry * tag;
    const struct headerSprintfExtension * ext;

    *error = NULL;
    chptr = str;
    while (*chptr && *chptr != '?') chptr++;

    if (*chptr != '?') {
	*error = _("? expected in expression");
	return 1;
    }

    *chptr++ = '\0';;

    if (*chptr != '{') {
	*error = _("{ expected after ? in expression");
	return 1;
    }

    chptr++;

    if (parseFormat(chptr, tags, extensions, &token->u.cond.ifFormat, 
		    &token->u.cond.numIfTokens, &end, PARSER_IN_EXPR, error)) 
	return 1;
    if (!*end) {
	*error = _("} expected in expression");
	freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	token->u.cond.ifFormat = NULL;
	return 1;
    }

    chptr = end;
    if (*chptr != ':' && *chptr != '|') {
	*error = _(": expected following ? subexpression");
	freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	token->u.cond.ifFormat = NULL;
	return 1;
    }

    if (*chptr == '|') {
	parseFormat(xstrdup(""), tags, extensions, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR, 
			error);
    } else {
	chptr++;

	if (*chptr != '{') {
	    *error = _("{ expected after : in expression");
	    freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.ifFormat = NULL;
	    return 1;
	}

	chptr++;

	if (parseFormat(chptr, tags, extensions, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR, 
			error)) 
	    return 1;
	if (!*end) {
	    *error = _("} expected in expression");
	    freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.ifFormat = NULL;
	    return 1;
	}

	chptr = end;
	if (*chptr != '|') {
	    *error = _("| expected at end of expression");
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
	if (type == RPM_STRING_ARRAY_TYPE) free((void *)data);

	countBuf = count;
	data = &countBuf;
	count = 1;
	type = RPM_INT32_TYPE;
    }

    strcpy(buf, "%");
    strcat(buf, tag->format);

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

	if (tagtype) {
	    val = tagtype(RPM_STRING_TYPE, strarray[element], buf, tag->pad, 0);
	}

	if (!val) {
	    strcat(buf, "s");

	    len = strlen(strarray[element]) + tag->pad + 20;
	    val = xmalloc(len);
	    sprintf(val, buf, strarray[element]);
	}

	if (mayfree) free((void *)data);

	break;

      case RPM_STRING_TYPE:
	if (tagtype) {
	    val = tagtype(RPM_STRING_ARRAY_TYPE, data, buf, tag->pad,  0);
	}

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
	  case RPM_INT32_TYPE:	intVal = *(((int_32 *) data) + element);
	}

	if (tagtype) {
	    val = tagtype(RPM_INT32_TYPE, &intVal, buf, tag->pad,  element);
	}

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

static char * singleSprintf(Header h, struct sprintfToken * token,
			    const struct headerSprintfExtension * extensions,
			    struct extensionCache * extCache, int element)
{
    char * val, * thisItem;
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
	    free(thisItem);
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
		if (type == RPM_STRING_ARRAY_TYPE) free(val);
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
		    free(thisItem);
		}
	    }
	}
	   
	break;
    }

    return val;
}

static struct extensionCache * allocateExtensionCache(
		     const struct headerSprintfExtension * extensions)
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
		     const char ** error)
{
    char * fmtString;
    struct sprintfToken * format;
    int numTokens;
    char * answer;
    const char * piece;
    int answerLength;
    int answerAlloced;
    int pieceLength;
    int i;
    struct extensionCache * extCache;
 
    /*fmtString = escapeString(origFmt);*/
    fmtString = xstrdup(origFmt);
   
    if (parseFormat(fmtString, tags, extensions, &format, &numTokens, 
		    NULL, PARSER_BEGIN, error)) {
	free(fmtString);
	return NULL;
    }

    extCache = allocateExtensionCache(extensions);

    answerAlloced = 1024;
    answerLength = 0;
    answer = xmalloc(answerAlloced);
    *answer = '\0';

    for (i = 0; i < numTokens; i++) {
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
{
    return realDateFormat(type, data, formatPrefix, padding, element, "%c");
}

static char * dayFormat(int_32 type, const void * data, 
		         char * formatPrefix, int padding, int element)
{
    return realDateFormat(type, data, formatPrefix, padding, element, 
			  "%a %b %d %Y");
}

static char * shescapeFormat(int_32 type, const void * data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
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
	if (!headerGetEntry(headerFrom, *p, &type, (void **) &s, &count))
	    continue;
	headerAddEntry(headerTo, *p, type, s, count);
	if (s != NULL &&
	   (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE))
	    free(s);
    }
}
