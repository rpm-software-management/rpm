#include "system.h"

#include <rpmio.h>
#include "falloc.h"

#define FA_MAGIC      0x02050920

/* 
   The entire file space is thus divided into blocks with a "struct fablock"
   at the header of each. The size fields doubly link this block list.

   There is an additional free list weaved through the block list, which 
   keeps new allocations fast.

   Much of this was inspired by Knuth vol 1.
 */

struct faFileHeader{
    unsigned int magic;
    unsigned int firstFree;
};

struct faHeader {
    unsigned int size;				
    unsigned int freeNext; /* offset of the next free block, 0 if none */
    unsigned int freePrev; 
    unsigned int isFree;

    /* note that the u16's appear last for alignment/space reasons */
};

struct faFooter {
    unsigned int size;
    unsigned int isFree; 
} ;

FD_t faFileno(faFile fa) {
    return fa->fd;
}

static inline ssize_t faRead(faFile fa, /*@out@*/void *buf, size_t count) {
    return fdRead(faFileno(fa), buf, count);
}

static inline ssize_t faWrite(faFile fa, const void *buf, size_t count) {
    return fdWrite(faFileno(fa), buf, count);
}

off_t faLseek(faFile fa, off_t off, int op) {
    return fdLseek(faFileno(fa), off, op);
}

void faClose(faFile fa) {
    fdClose(fa->fd);
    free(fa);
}

int faFcntl(faFile fa, int op, void *lip) {
    return fcntl(fdFileno(faFileno(fa)), op, lip);
}

static inline ssize_t faPRead(faFile fa, /*@out@*/void *buf, size_t count, off_t offset) {
    if (faLseek(fa, offset, SEEK_SET) < 0)
	return -1;
    return faRead(fa, buf, count);
}

static inline ssize_t faPWrite(faFile fa, const void *buf, size_t count, off_t offset) {
    if (faLseek(fa, offset, SEEK_SET) < 0)
	return -1;
    return faWrite(fa, buf, count);
}

/* flags here is the same as for open(2) - NULL returned on error */
faFile faOpen(const char * path, int flags, int perms)
{
    struct faFileHeader newHdr;
    struct faFile_s fas;
    faFile fa;
    off_t end;

    if (flags & O_WRONLY) {
	return NULL;
    }
    if (flags & O_RDWR) {
	fas.readOnly = 0;
    } else {
	fas.readOnly = 1;
    }

    fas.fd = fdOpen(path, flags, perms);
    if (fdFileno(fas.fd) < 0) return NULL;
    fas.firstFree = 0;
    fas.fileSize = 0;

    /* is this file brand new? */
    end = faLseek(&fas, 0, SEEK_END);
    if (!end) {
	newHdr.magic = FA_MAGIC;
	newHdr.firstFree = 0;
	if (faWrite(&fas, &newHdr, sizeof(newHdr)) != sizeof(newHdr)) {
	    close(fdFileno(fas.fd));
	    return NULL;
	}
	fas.firstFree = 0;
	fas.fileSize = sizeof(newHdr);
    } else {
	if (faPRead(&fas, &newHdr, sizeof(newHdr), 0) != sizeof(newHdr)) {
	    fdClose(fas.fd);
	    return NULL;
	}
	if (newHdr.magic != FA_MAGIC) {
	    fdClose(fas.fd);
	    return NULL;
	}
	fas.firstFree = newHdr.firstFree;

	if (faLseek(&fas, 0, SEEK_END) < 0) {
	    fdClose(fas.fd);
	    return NULL;
	}
	
	fas.fileSize = faLseek(&fas, 0, SEEK_CUR);
    }

    if ((fa = malloc(sizeof(*fa))) != NULL) {
	fa->fd = fas.fd;
	fa->readOnly = fas.readOnly;
	fa->firstFree = fas.firstFree;
	fa->fileSize = fas.fileSize;
    }

    return fa;
}

/* returns 0 on failure */
unsigned int faAlloc(faFile fa, unsigned int size)
{
    unsigned int nextFreeBlock;
    unsigned int newBlockOffset;
    unsigned int footerOffset;
    int failed = 0;
    struct faFileHeader faHeader;
    struct faHeader header, origHeader;
    struct faHeader * restoreHeader = NULL;
    struct faHeader nextFreeHeader, origNextFreeHeader;
    struct faHeader * restoreNextHeader = NULL;
    struct faHeader prevFreeHeader, origPrevFreeHeader;
    struct faHeader * restorePrevHeader = NULL;
    struct faFooter footer, origFooter;
    struct faFooter * restoreFooter = NULL;
    int updateHeader = 0;

    memset(&header, 0, sizeof(header));

    /* our internal idea of size includes overhead */
    size += sizeof(struct faHeader) + sizeof(struct faFooter);

    /* Make sure they are allocing multiples of 64 bytes. It'll keep
       things less fragmented that way */
    (size % 64) ? size += (64 - (size % 64)) : 0;

    /* find a block via first fit - see Knuth vol 1 for why */
    /* XXX this could be optimized a bit still */

    nextFreeBlock =  fa->firstFree;
    newBlockOffset = 0;

    while (nextFreeBlock && !newBlockOffset) {
	if (faPRead(fa, &header, sizeof(header), nextFreeBlock) != sizeof(header)) return 0;

/* XXX W2DO? exit(EXIT_FAILURE) forces the user to discover rpm --rebuilddb */
	if (!header.isFree) {
	    fprintf(stderr, _("free list corrupt (%u)- please run\n"
			"\t\"rpm --rebuilddb\"\n"
			"More information is available from http://www.rpm.org "
			"or the rpm-list@redhat.com mailing list\n"
			"if \"rpm --rebuilddb\" fails to correct the problem.\n"),
			nextFreeBlock);

	    exit(EXIT_FAILURE);
	    /*@notreached@*/
	}

	if (header.size >= size) {
	    newBlockOffset = nextFreeBlock;
	} else {
	    nextFreeBlock = header.freeNext;
	}
    }

    if (newBlockOffset) {
	/* header should still be good from the search */
	origHeader = header;

	footerOffset = newBlockOffset + header.size - sizeof(footer);

	if (faPRead(fa, &footer, sizeof(footer), footerOffset) != sizeof(footer)) 
	    return 0;
	origFooter = footer;

	/* should we split this block into two? */
	/* XXX implement fragment creation here */

	footer.isFree = header.isFree = 0;

	/* remove it from the free list before */
	if (newBlockOffset == fa->firstFree) {
	    faHeader.magic = FA_MAGIC;
	    faHeader.firstFree = header.freeNext;
	    fa->firstFree = header.freeNext;
	    updateHeader = 1;
	} else {
	    if (faPRead(fa, &prevFreeHeader, sizeof(prevFreeHeader),
			header.freePrev) != sizeof(prevFreeHeader)) 
		return 0;
	    origPrevFreeHeader = prevFreeHeader;

	    prevFreeHeader.freeNext = header.freeNext;
	}

	/* and after */
	if (header.freeNext) {
	    if (faPRead(fa, &nextFreeHeader, sizeof(nextFreeHeader),
			header.freeNext) != sizeof(nextFreeHeader)) 
		return 0;
	    origNextFreeHeader = nextFreeHeader;

	    nextFreeHeader.freePrev = header.freePrev;
	}

	/* if any of these fail, try and restore everything before leaving */
	if (updateHeader) {
	    if (faPWrite(fa, &faHeader, sizeof(faHeader), 0) !=
			     sizeof(faHeader)) 
		return 0;
	} else {
	    if (faPWrite(fa, &prevFreeHeader, sizeof(prevFreeHeader),
			header.freePrev) != sizeof(prevFreeHeader))
		return 0;
	    restorePrevHeader = &origPrevFreeHeader;
	}

	if (header.freeNext) {
	    if (faPWrite(fa, &nextFreeHeader, sizeof(nextFreeHeader),
			header.freeNext) != sizeof(nextFreeHeader))
		return 0;

	    restoreNextHeader = &origNextFreeHeader;
	}

	if (!failed) {
	    if (faPWrite(fa, &header, sizeof(header), newBlockOffset) !=
			 sizeof(header)) {
		failed = 1;
		restoreHeader = &origHeader;
	    }
	}

	if (!failed) {
	    if (faPWrite(fa, &footer, sizeof(footer),
			footerOffset) != sizeof(footer)) {
		failed = 1;
		restoreFooter = &origFooter;
	    }
	}

	if (failed) {
	    if (updateHeader) {
		faHeader.firstFree = newBlockOffset;
		fa->firstFree = newBlockOffset;
	        (void)faPWrite(fa, &faHeader, sizeof(faHeader), 0);
	    } 

	    if (restorePrevHeader)
	    	(void)faPWrite(fa, restorePrevHeader, sizeof(*restorePrevHeader),
				header.freePrev);

	    if (restoreNextHeader)
	    	(void)faPWrite(fa, restoreNextHeader, sizeof(*restoreNextHeader),
				header.freeNext);

	    if (restoreHeader)
	    	(void)faPWrite(fa, restoreHeader, sizeof(header),
				newBlockOffset);

	    if (restoreFooter)
	    	(void)faPWrite(fa, restoreFooter, sizeof(footer),
				footerOffset);

	    return 0;
	}
    } else {
	char * space;

	/* make a new block */
	newBlockOffset = fa->fileSize;
	footerOffset = newBlockOffset + size - sizeof(footer);

	space = alloca(size);
	if (!space) return 0;

	footer.isFree = header.isFree = 0;
	footer.size = header.size = size;
	header.freePrev = header.freeNext = 0;

	/* reserve all space up front */
	if (faPWrite(fa, space, size, newBlockOffset) != size)
	    return 0;

	if (faPWrite(fa, &header, sizeof(header), newBlockOffset) != sizeof(header))
	    return 0;

	if (faPWrite(fa, &footer, sizeof(footer), footerOffset) != sizeof(footer))
	    return 0;

	fa->fileSize += size;
    }
    
    return newBlockOffset + sizeof(header); 
}

void faFree(faFile fa, unsigned int offset)
{
    struct faHeader header;
    struct faFooter footer;
    int footerOffset;
    int prevFreeOffset, nextFreeOffset;
    struct faHeader prevFreeHeader, nextFreeHeader;
    struct faFileHeader faHeader;

    /* any errors cause this to die, and thus result in lost space in the
       database. which is at least better then corruption */

    offset -= sizeof(header);

    /* find out where in the (sorted) free list to put this */
    prevFreeOffset = fa->firstFree;

    if (!prevFreeOffset || (prevFreeOffset > offset)) {
	nextFreeOffset = fa->firstFree;
	prevFreeOffset = 0;
    } else {
	if (faPRead(fa, &prevFreeHeader, sizeof(prevFreeHeader),
			prevFreeOffset) != sizeof(prevFreeHeader))
	    return;

	while (prevFreeHeader.freeNext && prevFreeHeader.freeNext < offset) {
	    prevFreeOffset = prevFreeHeader.freeNext;
	    if (faPRead(fa, &prevFreeHeader, sizeof(prevFreeHeader),
			prevFreeOffset) != sizeof(prevFreeHeader))
		return;
	} 

	nextFreeOffset = prevFreeHeader.freeNext;
    }

    if (nextFreeOffset) {
	if (faPRead(fa, &nextFreeHeader, sizeof(nextFreeHeader),
			nextFreeOffset) != sizeof(nextFreeHeader))
	    return;
    }

    if (faPRead(fa, &header, sizeof(header), offset) != sizeof(header))
	return;

    footerOffset = offset + header.size - sizeof(footer);

    if (faPRead(fa, &footer, sizeof(footer), footerOffset) != sizeof(footer))
	return;

    header.isFree = 1;
    header.freeNext = nextFreeOffset;
    header.freePrev = prevFreeOffset;
    footer.isFree = 1;

    (void)faPWrite(fa, &header, sizeof(header), offset);

    (void)faPWrite(fa, &footer, sizeof(footer), footerOffset);

    if (nextFreeOffset) {
	nextFreeHeader.freePrev = offset;
	if (faPWrite(fa, &nextFreeHeader, sizeof(nextFreeHeader),
			nextFreeOffset) != sizeof(nextFreeHeader))
	    return;
    }

    if (prevFreeOffset) {
	prevFreeHeader.freeNext = offset;
	if (faPWrite(fa, &prevFreeHeader, sizeof(prevFreeHeader),
			prevFreeOffset) != sizeof(prevFreeHeader))
	    return;
    } else {
	fa->firstFree = offset;

	faHeader.magic = FA_MAGIC;
	faHeader.firstFree = fa->firstFree;

	if (faPWrite(fa, &faHeader, sizeof(faHeader), 0) != sizeof(faHeader))
	    return;
    }
}

int faFirstOffset(faFile fa) {
    return faNextOffset(fa, 0);
}

int faNextOffset(faFile fa, unsigned int lastOffset)
{
    struct faHeader header;
    int offset;

    if (lastOffset) {
	offset = lastOffset - sizeof(header);
    } else {
	offset = sizeof(struct faFileHeader);
    }

    if (offset >= fa->fileSize) return 0;

    if (faPRead(fa, &header, sizeof(header), offset) != sizeof(header))
	return 0;
    if (!lastOffset && !header.isFree)
	return (offset + sizeof(header));

    do {
	offset += header.size;

	if (faPRead(fa, &header, sizeof(header), offset) != sizeof(header))
	    return 0;

	if (!header.isFree) break;
    } while (offset < fa->fileSize && header.isFree);

    if (offset < fa->fileSize) {
	/* Sanity check this to make sure we're not going in loops */
	offset += sizeof(header);

	if (offset <= lastOffset) return -1;

	return offset;
    } else
	return 0;
}
