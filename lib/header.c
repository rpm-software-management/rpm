/* RPM - Copyright (C) 1995 Red Hat Software

 * header.c - routines for managing rpm headers
 */

#include <asm/byteorder.h>
#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "header.h"
#include "rpmlib.h"		/* necessary only for dumpHeader() */

#define INDEX_MALLOC_SIZE 8
#define DATA_MALLOC_SIZE 1024

struct headerToken {
    struct indexEntry *index;
    int entries_malloced;
    int entries_used;

    char *data;
    int data_malloced;
    int data_used;

    caddr_t mmapped_address;

    int mutable;
};

/* All this is in network byte order! */
struct indexEntry {
    int_32 tag;
    int_32 type;
    int_32 offset;		/* Offset from beginning of data segment */
    int_32 count;
};

struct headerIteratorS {
    Header h;
    int next_index;
};

HeaderIterator initIterator(Header h)
{
    HeaderIterator hi = malloc(sizeof(struct headerIteratorS));
    hi->h = h;
    hi->next_index = 0;
    return hi;
}

void freeIterator(HeaderIterator iter)
{
    free(iter);
}

int nextIterator(HeaderIterator iter,
		 int_32 *tag, int_32 *type, void **p, int_32 *c)
{
    struct headerToken *h = iter->h;
    struct indexEntry *index = h->index;
    int x = h->entries_used;
    int slot = iter->next_index;
    char **spp;
    char *sp;

    if (slot == h->entries_used) {
	return 0;
    }
    iter->next_index++;
    
    *tag = ntohl(index[slot].tag);
    *type = ntohl(index[slot].type);
    *c = ntohl(index[slot].count);

    /* Now look it up */
    switch (*type) {
    case INT64_TYPE:
    case INT32_TYPE:
    case INT16_TYPE:
    case INT8_TYPE:
    case BIN_TYPE:
    case CHAR_TYPE:
	*p = h->data + ntohl(index[slot].offset);
	break;
    case STRING_TYPE:
	if (*c == 1) {
	    /* Special case -- just return a pointer to the string */
	    *p = h->data + ntohl(index[slot].offset);
	} else {
	    /* Otherwise, build up an array of char* to return */
	    x = ntohl(index[slot].count);
	    *p = malloc(x * sizeof(char *));
	    spp = (char **) *p;
	    sp = h->data + ntohl(index[slot].offset);
	    while (x--) {
		*spp++ = sp;
		sp = strchr(sp, 0);
		sp++;
	    }
	}
	break;
    default:
	fprintf(stderr, "Data type %d not supprted\n", (int) *type);
	exit(1);
    }

    return 1;
}

Header copyHeader(Header h)
{
    int_32 tag, type, count;
    void *ptr;
    HeaderIterator headerIter = initIterator(h);
    Header res = newHeader();

    while (nextIterator(headerIter, &tag, &type, &ptr, &count)) {
	addEntry(res, tag, type, ptr, count);
    }

    return res;
}

/********************************************************************/

unsigned int sizeofHeader(Header h)
{
    unsigned int size;

    size = sizeof(int_32);	/* count of index entries */
    size += sizeof(int_32);	/* length of data */
    size += sizeof(struct indexEntry) * h->entries_used;
    size += h->data_used;

    return size;
}

void writeHeader(int fd, Header h)
{
    int_32 l;

    /* First write out the length of the index (count of index entries) */
    l = htonl(h->entries_used);
    write(fd, &l, sizeof(l));

    /* And the length of the data (number of bytes) */
    l = htonl(h->data_used);
    write(fd, &l, sizeof(l));

    /* Now write the index */
    write(fd, h->index, sizeof(struct indexEntry) * h->entries_used);

    /* Finally write the data */
    write(fd, h->data, h->data_used);
}

Header mmapHeader(int fd, long offset)
{
    struct headerToken *h = malloc(sizeof(struct headerToken));
    int_32 *p1, il, dl;
    caddr_t p;
    size_t bytes = 2 * sizeof(int_32);

    p = mmap(0, bytes, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, offset);
    if (!p)
	return NULL;

    p1 = (int_32 *) p;

    printf("here p1 = %p\n", p1);
    il = ntohl(*p1++);
    printf("il = %d\n", il);
    dl = ntohl(*p1++);
    if (munmap((caddr_t) p, 0)) {
	return NULL;
    }
    bytes += il * sizeof(struct indexEntry) + dl;
    p = mmap(0, bytes, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
    h->index = (void *) (p + 2 * sizeof(int_32));
    h->data = (void *) (p + 2 * sizeof(int_32) + il * sizeof(struct indexEntry));

    h->entries_malloced = il;
    h->entries_used = il;
    h->data_malloced = dl;
    h->data_used = dl;
    h->mutable = 0;
    h->mmapped_address = p;

    return h;
}

Header readHeader(int fd)
{
    int_32 il, dl;

    struct headerToken *h = (struct headerToken *)
    malloc(sizeof(struct headerToken));

    /* First read the index length (count of index entries) */
    read(fd, &il, sizeof(il));
    il = ntohl(il);

    /* Then read the data length (number of bytes) */
    read(fd, &dl, sizeof(dl));
    dl = ntohl(dl);

    /* Next read the index */
    h->index = malloc(il * sizeof(struct indexEntry));
    h->entries_malloced = il;
    h->entries_used = il;
    read(fd, h->index, sizeof(struct indexEntry) * il);

    /* Finally, read the data */
    h->data = malloc(dl);
    h->data_malloced = dl;
    h->data_used = dl;
    read(fd, h->data, dl);

    h->mutable = 0;

    return h;
}

Header loadHeader(void *pv)
{
    int_32 il, dl;
    char *p = pv;
    struct headerToken *h = malloc(sizeof(struct headerToken));

    il = ntohl(*((int_32 *) p));
    p += sizeof(int_32);
    dl = ntohl(*((int_32 *) p));
    p += sizeof(int_32);

    h->entries_malloced = il;
    h->entries_used = il;
    h->index = (struct indexEntry *) p;
    p += il * sizeof(struct indexEntry);

    h->data_malloced = dl;
    h->data_used = dl;
    h->data = p;

    h->mutable = 0;

    return h;
}

void *unloadHeader(Header h)
{
    void *p;
    int_32 *pi;

    pi = p = malloc(2 * sizeof(int_32) +
		    h->entries_used * sizeof(struct indexEntry) +
		    h->data_used);

    *pi++ = h->entries_used;
    *pi++ = h->data_used;
    memcpy(pi, h->index, h->entries_used * sizeof(struct indexEntry));
    pi += h->entries_used * sizeof(struct indexEntry);
    memcpy(pi, h->data, h->data_used);

    return p;
}

void dumpHeader(Header h, FILE * f, int flags)
{
    int i, c, ct;
    struct indexEntry *p;
    char *dp;
    char ch;
    char *type, *tag;

    /* First write out the length of the index (count of index entries) */
    fprintf(f, "Entry count: %d\n", h->entries_used);

    /* And the length of the data (number of bytes) */
    fprintf(f, "Data count : %d\n", h->data_used);

    /* Now write the index */
    p = h->index;
    fprintf(f, "\n             CT  TAG                  TYPE         "
		"OFSET     COUNT\n");
    for (i = 0; i < h->entries_used; i++) {
	switch (ntohl(p->type)) {
	    case NULL_TYPE:   	type = "NULL_TYPE"; 	break;
	    case CHAR_TYPE:   	type = "CHAR_TYPE"; 	break;
	    case BIN_TYPE:   	type = "BIN_TYPE"; 	break;
	    case INT8_TYPE:   	type = "INT8_TYPE"; 	break;
	    case INT16_TYPE:  	type = "INT16_TYPE"; 	break;
	    case INT32_TYPE:  	type = "INT32_TYPE"; 	break;
	    case INT64_TYPE:  	type = "INT64_TYPE"; 	break;
	    case STRING_TYPE: 	type = "STRING_TYPE"; 	break;
	    default:		type = "(unknown)";	break;
	}

	switch (ntohl(p->tag)) {
	    case RPMTAG_NAME:		tag = "RPMTAG_NAME";		break;
	    case RPMTAG_VERSION:	tag = "RPMTAG_VERSION";		break;
	    case RPMTAG_RELEASE:	tag = "RPMTAG_RELEASE";		break;
	    case RPMTAG_SERIAL:		tag = "RPMTAG_SERIAL";		break;
	    case RPMTAG_SUMMARY:	tag = "RPMTAG_SUMMARY";		break;
	    case RPMTAG_DESCRIPTION:	tag = "RPMTAG_DESCRIPTION";	break;
	    case RPMTAG_BUILDTIME:	tag = "RPMTAG_BUILDTIME";	break;
	    case RPMTAG_BUILDHOST:	tag = "RPMTAG_BUILDHOST";	break;
	    case RPMTAG_INSTALLTIME:	tag = "RPMTAG_INSTALLTIME";	break;
	    case RPMTAG_SIZE:		tag = "RPMTAG_SIZE";		break;
	    case RPMTAG_DISTRIBUTION:	tag = "RPMTAG_DISTRIBUTION";	break;
	    case RPMTAG_VENDOR:		tag = "RPMTAG_VENDOR";		break;
	    case RPMTAG_GIF:		tag = "RPMTAG_GIF";		break;
	    case RPMTAG_XPM:		tag = "RPMTAG_XPM";		break;
	    case RPMTAG_COPYRIGHT:	tag = "RPMTAG_COPYRIGHT";	break;
	    case RPMTAG_PACKAGER:	tag = "RPMTAG_PACKAGER";	break;
	    case RPMTAG_GROUP:		tag = "RPMTAG_GROUP";		break;
	    case RPMTAG_CHANGELOG:	tag = "RPMTAG_CHANGELOG";	break;
	    case RPMTAG_SOURCE:		tag = "RPMTAG_SOURCE";		break;
	    case RPMTAG_PATCH:		tag = "RPMTAG_PATCH";		break;
	    case RPMTAG_URL:		tag = "RPMTAG_URL";		break;
	    case RPMTAG_OS:		tag = "RPMTAG_OS";		break;
	    case RPMTAG_ARCH:		tag = "RPMTAG_ARCH";		break;
	    case RPMTAG_PREIN:		tag = "RPMTAG_PREIN";		break;
	    case RPMTAG_POSTIN:		tag = "RPMTAG_POSTIN";		break;
	    case RPMTAG_PREUN:		tag = "RPMTAG_PREUN";		break;
	    case RPMTAG_POSTUN:		tag = "RPMTAG_POSTUN";		break;
	    case RPMTAG_FILENAMES:	tag = "RPMTAG_FILES";		break;
	    case RPMTAG_FILESIZES:	tag = "RPMTAG_SIZES";		break;
	    case RPMTAG_FILESTATES:	tag = "RPMTAG_FILESTATES";	break;
	    case RPMTAG_FILEMODES:	tag = "RPMTAG_FILEMODES";	break;
	    case RPMTAG_FILEUIDS:	tag = "RPMTAG_FILEUIDS";	break;
	    case RPMTAG_FILEGIDS:	tag = "RPMTAG_FILEGIDS";	break;
	    case RPMTAG_FILERDEVS:	tag = "RPMTAG_FILERDEVS";	break;
	    case RPMTAG_FILEMTIMES:	tag = "RPMTAG_FILEMTIMES";	break;
	    case RPMTAG_FILEMD5S:	tag = "RPMTAG_FILEMD5S";	break;
	    case RPMTAG_FILELINKTOS:	tag = "RPMTAG_FILELINKTOS";	break;
	    case RPMTAG_FILEFLAGS:	tag = "RPMTAG_FILEFLAGS";	break;
	    default:			tag = "(unknown)";		break;
	}

	fprintf(f, "Entry      : %.3d %-20s %-12s 0x%.8x %.8d\n", i, tag, type,
		(uint_32) ntohl(p->offset), (uint_32) ntohl(p->count));

	if (flags & DUMP_INLINE) {
	    /* Print the data inline */
	    dp = h->data + ntohl(p->offset);
	    c = ntohl(p->count);
	    ct = 0;
	    switch (ntohl(p->type)) {
	    case INT32_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%.8x (%d)\n", ct++,
			    (uint_32) ntohl(*((int_32 *) dp)),
			    (uint_32) ntohl(*((int_32 *) dp)));
		    dp += sizeof(int_32);
		}
		break;

	    case INT16_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%.4x (%d)\n", ct++,
			    (short int) ntohs(*((int_16 *) dp)),
			    (short int) ntohs(*((int_16 *) dp)));
		    dp += sizeof(int_16);
		}
		break;
	    case INT8_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d 0x%.2x (%d)\n", ct++,
			    (char) *((int_8 *) dp),
			    (char) *((int_8 *) dp));
		    dp += sizeof(int_8);
		}
		break;
	    case BIN_TYPE:
		break;
	    case CHAR_TYPE:
		while (c--) {
		    ch = (char) *((char *) dp);
		    fprintf(f, "       Data: %.3d 0x%2x %c (%d)\n", ct++,
			    ch,
			    (isprint(ch) ? ch : ' '),
			    (char) *((char *) dp));
		    dp += sizeof(char);
		}
		break;
	    case STRING_TYPE:
		while (c--) {
		    fprintf(f, "       Data: %.3d %s\n", ct++, (char *) dp);
		    dp = strchr(dp, 0);
		    dp++;
		}
		break;
	    default:
		fprintf(stderr, "Data type %d not supprted\n", (int) ntohl(p->type));
		exit(1);
	    }
	}
	p++;
    }
}

void freeHeader(Header h)
{
    if (h->mutable) {
	free(h->index);
	free(h->data);
    }
    if (h->mmapped_address) {
	munmap(h->mmapped_address, 0);
    }
    free(h);
}

int isEntry(Header h, int_32 tag)
{
    struct indexEntry *index = h->index;
    int x = h->entries_used;

    tag = htonl(tag);
    while (x && (tag != index->tag)) {
	index++;
	x--;
    }
    if (x == 0) {
	return 0;
    } else {
	return 1;
    }
}

int getEntry(Header h, int_32 tag, int_32 * type, void **p, int_32 * c)
{
    struct indexEntry *index = h->index;
    int x = h->entries_used;
    char **spp;
    char *sp;
    int t;

    /* First find the tag */
    tag = htonl(tag);
    while (x && (tag != index->tag)) {
	index++;
	x--;
    }
    if (x == 0) {
	return 0;
    }
    t = (int) ntohl(index->type);
    if (type) {
	*type = t;
    }
    if (c) {
	*c = ntohl(index->count);
    }

    /* Now look it up */
    switch (t) {
    case INT64_TYPE:
    case INT32_TYPE:
    case INT16_TYPE:
    case INT8_TYPE:
    case BIN_TYPE:
    case CHAR_TYPE:
	*p = h->data + ntohl(index->offset);
	break;
    case STRING_TYPE:
	if (ntohl(index->count) == 1) {
	    /* Special case -- just return a pointer to the string */
	    *p = h->data + ntohl(index->offset);
	} else {
	    /* Otherwise, build up an array of char* to return */
	    x = ntohl(index->count);
	    *p = malloc(x * sizeof(char *));
	    spp = (char **) *p;
	    sp = h->data + ntohl(index->offset);
	    while (x--) {
		*spp++ = sp;
		sp = strchr(sp, 0);
		sp++;
	    }
	}
	break;
    default:
	fprintf(stderr, "Data type %d not supprted\n", t);
	exit(1);
    }

    return 1;
}

/********************************************************************/

/*
 *  The following routines are used to build up a header.
 */

Header newHeader()
{
    struct headerToken *h = (struct headerToken *)
    malloc(sizeof(struct headerToken));

    h->data = malloc(DATA_MALLOC_SIZE);
    h->data_malloced = DATA_MALLOC_SIZE;
    h->data_used = 0;

    h->index = malloc(INDEX_MALLOC_SIZE * sizeof(struct indexEntry));
    h->entries_malloced = INDEX_MALLOC_SIZE;
    h->entries_used = 0;

    h->mutable = 1;
    h->mmapped_address = (caddr_t) 0;

    return (Header) h;
}

int addEntry(Header h, int_32 tag, int_32 type, void *p, int_32 c)
{
    struct indexEntry *entry;
    void *ptr;
    char **spp;
    char *sp;
    int_32 *i32p;
    int_16 *i16p;
    int i, length;

    if (c <= 0) {
	fprintf(stderr, "Bad count for addEntry(): %d\n", (int) c);
	exit(1);
    }
    if (h->mutable == 0) {
	fprintf(stderr, "Attempted addEntry() to immutable header.\n");
	exit(1);
    }
    /* Allocate more index space if necessary */
    if (h->entries_used == h->entries_malloced) {
	h->entries_malloced += INDEX_MALLOC_SIZE;
	h->index = realloc(h->index,
			h->entries_malloced * sizeof(struct indexEntry));
    }
    /* Fill in the index */
    i = h->entries_used++;
    entry = &((h->index)[i]);
    entry->tag = htonl(tag);
    entry->type = htonl(type);
    entry->count = htonl(c);
    entry->offset = htonl(h->data_used);

    /* Compute length of data to add */
    switch (type) {
    case INT64_TYPE:
	length = sizeof(int_64) * c;
	break;
    case INT32_TYPE:
	length = sizeof(int_32) * c;
	break;
    case INT16_TYPE:
	length = sizeof(int_16) * c;
	break;
    case INT8_TYPE:
	length = sizeof(int_8) * c;
	break;
    case BIN_TYPE:
    case CHAR_TYPE:
	length = sizeof(char) * c;
	break;
    case STRING_TYPE:
	if (c == 1) {
	    /* Special case -- p is just the string */
	    length = strlen(p) + 1;
	} else {
	    /* Compute sum of length of all strings, including null terminators */
	    i = c;
	    spp = p;
	    length = 0;
	    while (i--) {
		/* add one for null termination */
		length += strlen(*spp++) + 1;
	    }
	}
	break;
    default:
	fprintf(stderr, "Data type %d not supprted\n", (int) type);
	exit(1);
    }

    /* Allocate more data space if necessary */
    while ((length + h->data_used) > h->data_malloced) {
	h->data_malloced += DATA_MALLOC_SIZE;
	h->data = realloc(h->data, h->data_malloced);
    }
    /* Fill in the data */
    ptr = h->data + h->data_used;
    switch (type) {
      case INT32_TYPE:
	memcpy(ptr, p, length);
	i = c;
	i32p = (int_32 *) ptr;
	while (i--) {
	    *i32p = htonl(*i32p);
	    i32p++;
	}
	break;
      case INT16_TYPE:
	memcpy(ptr, p, length);
	i = c;
	i16p = (int_16 *) ptr;
	while (i--) {
	    *i16p = htons(*i16p);
	    i16p++;
	}
	break;
      case INT8_TYPE:
      case BIN_TYPE:
      case CHAR_TYPE:
	memcpy(ptr, p, length);
	break;
      case STRING_TYPE:
	if (c == 1) {
	    /* Special case -- p is just the string */
	    strcpy(ptr, p);
	} else {
	    /* Otherwise, p is char** */
	    i = c;
	    spp = p;
	    sp = (char *) ptr;
	    while (i--) {
		strcpy(sp, *spp);
		sp += strlen(*spp++) + 1;
	    }
	}
	break;
      default:
	fprintf(stderr, "Data type %d not supprted\n", (int) type);
	exit(1);
    }

    h->data_used += length;

    return 1;
}
