/** \ingroup header
 * \file lib/hdrinline.c
 */

#include "system.h"

#include <header_internal.h>

#include "debug.h"

int _hdrinline_debug = 0;

/*@access Header @*/
/*@access entryInfo @*/
/*@access indexEntry @*/

/*@access extensionCache @*/
/*@access sprintfTag @*/
/*@access sprintfToken @*/

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ const void * p) /*@modifies *p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

Header headerNew()
{
    Header h = xcalloc(1, sizeof(*h));

    /*@-assignexpose@*/
    h->hv = hv;		/* structure assignment */
    /*@=assignexpose@*/
    h->indexAlloced = INDEX_MALLOC_SIZE;
    h->indexUsed = 0;
    h->region_allocated = 0;
    h->sorted = 1;
    h->legacy = 0;
    h->nrefs = 1;

    h->index = (h->indexAlloced
	? xcalloc(h->indexAlloced, sizeof(*h->index))
	: NULL);

    /*@-globstate@*/
    return h;
    /*@=globstate@*/
}

Header headerLoad(void * uh)
{
    int_32 * ei = (int_32 *) uh;
    int_32 il = ntohl(ei[0]);		/* index length */
    int_32 dl = ntohl(ei[1]);		/* data length */
    int pvlen = sizeof(il) + sizeof(dl) +
               (il * sizeof(struct entryInfo)) + dl;
    void * pv = uh;
    Header h = xcalloc(1, sizeof(*h));
    entryInfo pe;
    char * dataStart;
    indexEntry entry; 
    int rdlen;
    int i;

    ei = (int_32 *) pv;
    /*@-castexpose@*/
    pe = (entryInfo) &ei[2];
    /*@=castexpose@*/
    dataStart = (char *) (pe + il);

    /*@-assignexpose@*/
    h->hv = hv;		/* structure assignment */
    /*@=assignexpose@*/
    h->indexAlloced = il + 1;
    h->indexUsed = il;
    h->index = xcalloc(h->indexAlloced, sizeof(*h->index));
    h->sorted = 1;
    h->region_allocated = 0;
    h->nrefs = 1;

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
	h->legacy = 1;
	entry->info.type = REGION_TAG_TYPE;
	entry->info.tag = HEADER_IMAGE;
	entry->info.count = REGION_TAG_COUNT;
	entry->info.offset = ((char *)pe - dataStart); /* negative offset */

	/*@-assignexpose@*/
	entry->data = pe;
	/*@=assignexpose@*/
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, il, 0, pe, dataStart, entry->info.offset);
	if (rdlen != dl) goto errxit;
	entry->rdlen = rdlen;
	entry++;
	h->indexUsed++;
    } else {
	int nb = ntohl(pe->count);
	int_32 rdl;
	int_32 ril;

	h->legacy = 0;
	entry->info.type = htonl(pe->type);
	if (entry->info.type < RPM_MIN_TYPE || entry->info.type > RPM_MAX_TYPE)
	    goto errxit;
	entry->info.count = htonl(pe->count);

	{  int off = ntohl(pe->offset);
	   if (off) {
		int_32 * stei = memcpy(alloca(nb), dataStart + off, nb);
		rdl = -ntohl(stei[2]);	/* negative offset */
		ril = rdl/sizeof(*pe);
		entry->info.tag = htonl(pe->tag);
	    } else {
		ril = il;
		rdl = (ril * sizeof(struct entryInfo));
		entry->info.tag = HEADER_IMAGE;
	    }
	}
	entry->info.offset = -rdl;	/* negative offset */

	/*@-assignexpose@*/
	entry->data = pe;
	/*@=assignexpose@*/
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, ril-1, 0, pe+1, dataStart, entry->info.offset);
	if (rdlen < 0) goto errxit;
	entry->rdlen = rdlen;

	if (ril < h->indexUsed) {
	    indexEntry newEntry = entry + ril;
	    int ne = (h->indexUsed - ril);
	    int rid = entry->info.offset+1;
	    int rc;

	    /* Load dribble entries from region. */
	    rc = regionSwab(newEntry, ne, 0, pe+ril, dataStart, rid);
	    if (rc < 0) goto errxit;
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

    h->sorted = 0;
    headerSort(h);

    /*@-globstate@*/
    return h;
    /*@=globstate@*/

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

int headerWrite(FD_t fd, Header h, enum hMagic magicp)
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

Header headerCopyLoad(void * uh)
{
    int_32 * ei = (int_32 *) uh;
    int_32 il = ntohl(ei[0]);		/* index length */
    int_32 dl = ntohl(ei[1]);		/* data length */
    int pvlen = sizeof(il) + sizeof(dl) +
               (il * sizeof(struct entryInfo)) + dl;
    void * nuh = memcpy(xmalloc(pvlen), uh, pvlen);
    Header h;

    h = headerLoad(nuh);
    if (h == NULL) {
	nuh = _free(nuh);
	return h;
    }
    h->region_allocated = 1;
    return h;
}

Header headerRead(FD_t fd, enum hMagic magicp)
{
    int_32 block[4];
    int_32 reserved;
    int_32 * ei = NULL;
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

    if (timedRead(fd, (char *)&ei[2], len) != len)
	goto exit;
    
    h = headerLoad(ei);

exit:
    if (h) {
	if (h->region_allocated)
	    ei = _free(ei);
	h->region_allocated = 1;
    } else if (ei)
	ei = _free(ei);
    return h;
}

void headerCopyTags(Header headerFrom, Header headerTo, hTAG_t tagstocopy)
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

void headerFreeIterator(HeaderIterator hi)
{
    hi->h = headerFree(hi->h);
    hi = _free(hi);
}

HeaderIterator headerInitIterator(Header h)
{
    HeaderIterator hi = xmalloc(sizeof(struct headerIteratorS));

    headerSort(h);

    hi->h = headerLink(h);
    hi->next_index = 0;
    return hi;
}

int headerNextIterator(HeaderIterator hi,
		 hTAG_t tag, hTYP_t type, hPTR_t * p, hCNT_t c)
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
