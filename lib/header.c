/* RPM - Copyright (C) 1995 Red Hat Software
 *
 * header.c - routines for managing rpm headers
 */

/* Data written to file descriptors is in network byte order.    */
/* Data read from file descriptors is expected to be in          */
/* network byte order and is converted on the fly to host order. */

#include "config.h"
#include "miscfn.h"

#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "header.h"
#include "tread.h"

#define INDEX_MALLOC_SIZE 8

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
    void * data; 
    int length;			/* Computable, but why bother */
};

static int indexCmp(const void *ap, const void *bp);
static void *doHeaderUnload(Header h, int * lengthPtr);
static struct indexEntry *findEntry(Header h, int_32 tag, int_32 type);
static void * grabData(int_32 type, void * p, int_32 c, int * lengthPtr);
static int dataLength(int_32 type, void * p, int_32 count, int onDisk);
static void copyEntry(struct indexEntry * entry, 
			int_32 *type, void **p, int_32 *c);

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
    HeaderIterator hi = malloc(sizeof(struct headerIteratorS));

    headerSort(h);

    hi->h = h;
    hi->next_index = 0;
    return hi;
}

void headerFreeIterator(HeaderIterator iter)
{
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
    
    *tag = h->index[slot].info.tag;
    copyEntry(h->index + slot, type, p, c);

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

void headerSort(Header h) {
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
   
    /* Sort the index -- not really necessary but some old apps may depend
       on this and it certainly won't hurt anything */
    headerSort(h);
    headerIter = headerInitIterator(h);

    while (headerNextIterator(headerIter, &tag, &type, &ptr, &count)) {
	headerAddEntry(res, tag, type, ptr, count);
	if (type == RPM_STRING_ARRAY_TYPE) free(ptr);
    }

    res->sorted = 1;

    headerFreeIterator(headerIter);

    return res;
}

/********************************************************************/
/*                                                                  */
/* Reading and writing headers                                      */
/*                                                                  */
/********************************************************************/

void headerWrite(int fd, Header h, int magicp)
{
    void * p;
    int length;
    int_32 l;

    p = doHeaderUnload(h, &length);

    if (magicp) {
	write(fd, header_magic, sizeof(header_magic));
	l = htonl(0);
	write(fd, &l, sizeof(l));
    }
    
    write(fd, p, length);

    free(p);
}

Header headerRead(int fd, int magicp)
{
    int_32 reserved;
    int_32 * p;
    int_32 il, dl;
    int_32 magic;
    Header h;
    void * block;
    int totalSize;

    if (magicp == HEADER_MAGIC_YES) {
	if (timedRead(fd, &magic, sizeof(magic)) != sizeof(magic))
	    return NULL;
	if (memcmp(&magic, header_magic, sizeof(magic))) {
	    return NULL;
	}

	if (timedRead(fd, &reserved, sizeof(reserved)) != sizeof(reserved))
	    return NULL;
    }
    
    /* First read the index length (count of index entries) */
    if (timedRead(fd, &il, sizeof(il)) != sizeof(il)) 
	return NULL;

    il = ntohl(il);

    /* Then read the data length (number of bytes) */
    if (timedRead(fd, &dl, sizeof(dl)) != sizeof(dl)) 
	return NULL;

    dl = ntohl(dl);

    totalSize = sizeof(int_32) + sizeof(int_32) + 
		(il * sizeof(struct entryInfo)) + dl;

    block = p = malloc(totalSize);
    *p++ = htonl(il);
    *p++ = htonl(dl);

    totalSize -= sizeof(int_32) + sizeof(int_32);
    if (timedRead(fd, p, totalSize) != totalSize)
	return NULL;
    
    h = headerLoad(block);

    free(block);

    return h;
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
    char * dataStart;
    struct entryInfo * pe;
    struct indexEntry * entry; 
    struct headerToken *h = malloc(sizeof(struct headerToken));
    char * src, * dst;
    int i;
    int count;

    il = ntohl(*((int_32 *) p));
    p += sizeof(int_32);

    /* we can skip the data length -- we only store this to allow reading
       from disk */
    p += sizeof(int_32);

    h->indexAlloced = il;
    h->indexUsed = il;
    h->index = malloc(il * sizeof(struct indexEntry));

    /* This assumes you only headerLoad() something you headerUnload()-ed */
    h->sorted = 1;

    pe = (struct entryInfo *) p;
    dataStart = (char *) (pe + h->indexUsed);

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++, pe++) {
	entry->info.type = htonl(pe->type);
	entry->info.tag = htonl(pe->tag);
	entry->info.count = htonl(pe->count);
	entry->info.offset = -1;

	src = dataStart + htonl(pe->offset);
	entry->length = dataLength(entry->info.type, src, 
				   entry->info.count, 1);
	entry->data = dst = malloc(entry->length);

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
	}
    }

    return h;
}

static void *doHeaderUnload(Header h, int * lengthPtr)
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
    pi = p = malloc(*lengthPtr);

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
	}
    }
   
    return p;
}

void *headerUnload(Header h) {
    int length;

    return doHeaderUnload(h, &length);
}

/********************************************************************/
/*                                                                  */
/* Header dumping                                                   */
/*                                                                  */
/********************************************************************/

void headerDump(Header h, FILE *f, int flags, 
		const struct headerTagTableEntry * tags) {
    int i, c, ct;
    struct indexEntry *p;
    const struct headerTagTableEntry * tage;
    char *dp;
    char ch;
    char *type, *tag;

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
	    default:		    	type = "(unknown)";	break;
	}

	tage = tags;
	while (tage->name && tage->val != p->info.tag) tage++;

	if (!tage->name)
	    tag = "(unknown)";
	else
	    tag = tage->name;

	fprintf(f, "Entry      : %.3d (%d)%-14s %-18s 0x%.8x %.8d\n", i,
		p->info.tag, tag, type, (uint_32) p->info.offset, (uint_32) 
		p->info.count);

	if (flags & HEADER_DUMP_INLINE) {
	    /* Print the data inline */
	    dp = p->data;
	    c = p->info.count;
	    ct = 0;
	    switch (p->info.type) {
	    case RPM_INT32_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%08x (%d)\n", ct++,
			    (uint_32) *((int_32 *) dp),
			    (uint_32) *((int_32 *) dp));
		    dp += sizeof(int_32);
		}
		break;

	    case RPM_INT16_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%04x (%d)\n", ct++,
			    (short int) *((int_16 *) dp),
			    (short int) *((int_16 *) dp));
		    dp += sizeof(int_16);
		}
		break;
	    case RPM_INT8_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%02x (%d)\n", ct++,
			    (char) *((int_8 *) dp),
			    (char) *((int_8 *) dp));
		    dp += sizeof(int_8);
		}
		break;
	    case RPM_BIN_TYPE:
	      while (c > 0) {
		  fprintf(f, "       Data: %.3d ", ct);
		  while (c--) {
		      fprintf(f, "%02x ", (unsigned char) *(int_8 *)dp);
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
		    ch = (char) *((char *) dp);
		    fprintf(f, "       Data: %.3d 0x%2x %c (%d)\n", ct++,
			    ch,
			    (isprint(ch) ? ch : ' '),
			    (char) *((char *) dp));
		    dp += sizeof(char);
		}
		break;
	    case RPM_STRING_TYPE:
	    case RPM_STRING_ARRAY_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d %s\n", ct++, (char *) dp);
		    dp = strchr(dp, 0);
		    dp++;
		}
		break;
	    default:
		fprintf(stderr, "Data type %d not supprted\n", 
			(int) p->info.type);
		exit(1);
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

static void copyEntry(struct indexEntry * entry, 
			int_32 *type, void **p, int_32 *c) {
    int i, tableSize;
    char ** ptrEntry;
    char * chptr;

    if (type) 
	*type = entry->info.type;
    if (c) 
	*c = entry->info.count;

    /* Now look it up */
    switch (entry->info.type) {
      case RPM_STRING_TYPE:
	if (entry->info.count == 1) {
	    *p = entry->data;
	    break;
	}
	/* fallthrough */
      case RPM_STRING_ARRAY_TYPE:
	*type = RPM_STRING_ARRAY_TYPE;
	i = entry->info.count;
	tableSize = i * sizeof(char *);
	ptrEntry = *p = malloc(tableSize + entry->length);
	chptr = ((char *) *p) + tableSize;
	memcpy(chptr, entry->data, entry->length);
	while (i--) {
	    *ptrEntry++ = chptr;
	    chptr = strchr(chptr, 0);
	    chptr++;
	}
	break;

      default:
	*p = entry->data;
    }
}

int headerGetRawEntry(Header h, int_32 tag, int_32 *type, void **p, int_32 *c) {
    struct indexEntry * entry;

    if (!p) return headerIsEntry(h, tag);

    /* First find the tag */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) {
	*p = NULL;
	return 0;
    }

    copyEntry(entry, type, p, c);

    return 1;
}

int headerGetEntry(Header h, int_32 tag, int_32 * type, void **p, int_32 * c)
{
    int_32 t = RPM_NULL_TYPE;
    int rc;

    rc = headerGetRawEntry(h, tag, &t, p, c);
    if (rc && type) *type = t;

    return rc;
}

/********************************************************************/
/*                                                                  */
/* Header creation and deletion                                     */
/*                                                                  */
/********************************************************************/

Header headerNew()
{
    Header h = malloc(sizeof(struct headerToken));

    h->index = malloc(INDEX_MALLOC_SIZE * sizeof(struct indexEntry));
    h->indexAlloced = INDEX_MALLOC_SIZE;
    h->indexUsed = 0;

    h->sorted = 0;

    return (Header) h;
}

void headerFree(Header h)
{
    int i;

    for (i = 0; i < h->indexUsed; i++)
	free(h->index[i].data);

    free(h->index);
    free(h);
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

static int dataLength(int_32 type, void * p, int_32 count, int onDisk) {
    int thisLen, length, i;
    char ** src, * chptr;

    switch (type) {
      case RPM_STRING_TYPE:
	if (count == 1) {
	    /* Special case -- p is just the string */
	    length = strlen(p) + 1;
	    break;
	}
        /* This should not be allowed */
	fprintf(stderr, "grabData() RPM_STRING_TYPE count must be 1.\n");
	exit(1);

      case RPM_STRING_ARRAY_TYPE:
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
	    fprintf(stderr, "Data type %d not supported\n", (int) type);
	    exit(1);
	}
    }

    return length;
}

static void copyData(int_32 type, void * dstPtr, void * srcPtr, int_32 c, 
			int dataLength) {
    char ** src, * dst;
    int i, len;

    switch (type) {
      case RPM_STRING_ARRAY_TYPE:
	/* Otherwise, p is char** */
	i = c;
	src = (char **) srcPtr;
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
    }
}

static void * grabData(int_32 type, void * p, int_32 c, int * lengthPtr) {
    int length;
    void * data;

    length = dataLength(type, p, c, 0);
    data = malloc(length);

    copyData(type, data, p, c, length);

    *lengthPtr = length;
    return data;
}

/********************************************************************/
/*                                                                  */
/* Adding and modifying entries                                     */
/*                                                                  */
/********************************************************************/

int headerAddEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c)
{
    struct indexEntry *entry;

    h->sorted = 0;

    if (c <= 0) {
	fprintf(stderr, "Bad count for headerAddEntry(): %d\n", (int) c);
	exit(1);
    }

    /* Allocate more index space if necessary */
    if (h->indexUsed == h->indexAlloced) {
	h->indexAlloced += INDEX_MALLOC_SIZE;
	h->index = realloc(h->index,
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

int headerModifyEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c)
{
    struct indexEntry *entry;
    void * oldData;

    /* First find the tag */
    entry = findEntry(h, tag, type);
    if (! entry) {
	return 0;
    }

    /* free after we've grabbed the new data in case the two are intertwined 
       -- that's a bad idea but at least we won't break */
    oldData = entry->data;

    entry->info.count = c;
    entry->info.type = type;
    entry->data = grabData(type, p, c, &entry->length);

    free(oldData);
    
    return 1;
}

int headerAppendEntry(Header h, int_32 tag, int_32 type, void * p, int_32 c) {
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

    entry->data = realloc(entry->data, entry->length + length);
    memcpy(((char *) entry->data) + entry->length, p, length);

    copyData(type, entry->data + entry->length, p, c, length);

    entry->length += length;

    entry->info.count += c;

    return 0;
}

int headerRemoveEntry(Header h, int_32 tag) {
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
	*(entry++) = *(--last);
    }
    h->indexUsed = last - h->index;

    h->sorted = 0;

    return 0;
}
