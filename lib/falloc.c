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

off_t faLseek(faFile fa, off_t off, int op) {
    return fdLseek(faFileno(fa), off, op);
}

static inline ssize_t faRead(faFile fa, void *buf, size_t count) {
    return fdRead(faFileno(fa), buf, count);
}

static inline ssize_t faWrite(faFile fa, const void *buf, size_t count) {
    return fdWrite(faFileno(fa), buf, count);
}

int faFcntl(faFile fa, int op, void *lip) {
    return fcntl(fdFileno(faFileno(fa)), op, lip);
}

/* flags here is the same as for open(2) - NULL returned on error */
faFile faOpen(char * path, int flags, int perms) {
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
    }
    else {
	(void)faLseek(&fas, 0, SEEK_SET);
	if (faRead(&fas, &newHdr, sizeof(newHdr)) != sizeof(newHdr)) {
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

    fa = malloc(sizeof(*fa));
    if (fa) *fa = fas;

    return fa;
}

unsigned int faAlloc(faFile fa, unsigned int size) { /* returns 0 on failure */
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
	if (faLseek(fa, nextFreeBlock, SEEK_SET) < 0) return 0;
	if (faRead(fa, &header, sizeof(header)) != sizeof(header)) return 0;

/* XXX W2DO? exit(1) forces the user to discover rpm --rebuilddb */
	if (!header.isFree) {
	    fprintf(stderr, _("free list corrupt (%u)- contact "
			"rpm-list@redhat.com\n"), nextFreeBlock);
	    exit(1);
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

	if (faLseek(fa, footerOffset, SEEK_SET) < 0) return 0;
	if (faRead(fa, &footer, sizeof(footer)) != sizeof(footer)) 
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
	    if (faLseek(fa, header.freePrev, SEEK_SET) < 0) return 0;
	    if (faRead(fa, &prevFreeHeader, sizeof(prevFreeHeader)) !=
			 sizeof(prevFreeHeader)) 
		return 0;
	    origPrevFreeHeader = prevFreeHeader;

	    prevFreeHeader.freeNext = header.freeNext;
	}

	/* and after */
	if (header.freeNext) {
	    if (faLseek(fa, header.freeNext, SEEK_SET) < 0) return 0;
	    if (faRead(fa, &nextFreeHeader, sizeof(nextFreeHeader)) !=
			 sizeof(nextFreeHeader)) 
		return 0;
	    origNextFreeHeader = nextFreeHeader;

	    nextFreeHeader.freePrev = header.freePrev;
	}

	/* if any of these fail, try and restore everything before leaving */
	if (updateHeader) {
	    if (faLseek(fa, 0, SEEK_SET) < 0) 
		return 0;
	    else if (faWrite(fa, &faHeader, sizeof(faHeader)) !=
			     sizeof(faHeader)) 
		return 0;
	} else {
	    if (faLseek(fa, header.freePrev, SEEK_SET) < 0)
		return 0;
	    else if (faWrite(fa, &prevFreeHeader, sizeof(prevFreeHeader)) !=
			 sizeof(prevFreeHeader))
		return 0;
	    restorePrevHeader = &origPrevFreeHeader;
	}

	if (header.freeNext) {
	    if (faLseek(fa, header.freeNext, SEEK_SET) < 0)
		return 0;
	    else if (faWrite(fa, &nextFreeHeader, sizeof(nextFreeHeader)) !=
			 sizeof(nextFreeHeader))
		return 0;

	    restoreNextHeader = &origNextFreeHeader;
	}

	if (!failed) {
	    if (faLseek(fa, newBlockOffset, SEEK_SET) < 0) 
		failed = 1;
	    else if (faWrite(fa, &header, sizeof(header)) !=
			 sizeof(header)) {
		failed = 1;
		restoreHeader = &origHeader;
	    }
	}

	if (!failed) {
	    if (faLseek(fa, footerOffset, SEEK_SET) < 0) 
		failed = 1;
	    else if (faWrite(fa, &footer, sizeof(footer)) !=
			 sizeof(footer)) {
		failed = 1;
		restoreFooter = &origFooter;
	    }
	}

	if (failed) {
	    if (updateHeader) {
		faHeader.firstFree = newBlockOffset;
		fa->firstFree = newBlockOffset;
	    	(void)faLseek(fa, 0, SEEK_SET);
	        (void)faWrite(fa, &faHeader, sizeof(faHeader));
	    } 

	    if (restorePrevHeader) {
	    	(void)faLseek(fa, header.freePrev, SEEK_SET);
	    	(void)faWrite(fa, restorePrevHeader, sizeof(*restorePrevHeader));
	    }

	    if (restoreNextHeader) {
	    	(void)faLseek(fa, header.freeNext, SEEK_SET);
	    	(void)faWrite(fa, restoreNextHeader, sizeof(*restoreNextHeader));
	    }

	    if (restoreHeader) {
	    	(void)faLseek(fa, newBlockOffset, SEEK_SET);
	    	(void)faWrite(fa, restoreHeader, sizeof(header));
	    }

	    if (restoreFooter) {
	    	(void)faLseek(fa, footerOffset, SEEK_SET);
	    	(void)faWrite(fa, restoreFooter, sizeof(footer));
	    }

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
        (void)faLseek(fa, newBlockOffset, SEEK_SET);
	if (faWrite(fa, space, size) != size) {
	    return 0;
	}

        (void)faLseek(fa, newBlockOffset, SEEK_SET);
	if (faWrite(fa, &header, sizeof(header)) != sizeof(header)) {
	    return 0;
	}

        (void)faLseek(fa, footerOffset, SEEK_SET);
	if (faWrite(fa, &footer, sizeof(footer)) != sizeof(footer)) {
	    return 0;
	}

	fa->fileSize += size;
    }
    
    return newBlockOffset + sizeof(header); 
}

void faFree(faFile fa, unsigned int offset) {
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
	if (faLseek(fa, prevFreeOffset, SEEK_SET) < 0) return;
	if (faRead(fa, &prevFreeHeader, sizeof(prevFreeHeader)) != 
		sizeof(prevFreeHeader)) return;

	while (prevFreeHeader.freeNext && prevFreeHeader.freeNext < offset) {
	    prevFreeOffset = prevFreeHeader.freeNext;
	    if (faLseek(fa, prevFreeOffset, SEEK_SET) < 0) return;
	    if (faRead(fa, &prevFreeHeader, sizeof(prevFreeHeader)) != 
		    sizeof(prevFreeHeader)) return;
	} 

	nextFreeOffset = prevFreeHeader.freeNext;
    }

    if (nextFreeOffset) {
	if (faLseek(fa, nextFreeOffset, SEEK_SET) < 0) return;
	if (faRead(fa, &nextFreeHeader, sizeof(nextFreeHeader)) != 
		sizeof(nextFreeHeader)) return;
    }

    if (faLseek(fa, offset, SEEK_SET) < 0)
	return;
    if (faRead(fa, &header, sizeof(header)) != sizeof(header))
	return;

    footerOffset = offset + header.size - sizeof(footer);

    if (faLseek(fa, footerOffset, SEEK_SET) < 0)
	return;
    if (faRead(fa, &footer, sizeof(footer)) != sizeof(footer))
	return;

    header.isFree = 1;
    header.freeNext = nextFreeOffset;
    header.freePrev = prevFreeOffset;
    footer.isFree = 1;

    (void)faLseek(fa, offset, SEEK_SET);
    (void)faWrite(fa, &header, sizeof(header));

    (void)faLseek(fa, footerOffset, SEEK_SET);
    (void)faWrite(fa, &footer, sizeof(footer));

    if (nextFreeOffset) {
	nextFreeHeader.freePrev = offset;
	if (faLseek(fa, nextFreeOffset, SEEK_SET) < 0) return;
	if (faWrite(fa, &nextFreeHeader, sizeof(nextFreeHeader)) != 
		sizeof(nextFreeHeader)) return;
    }

    if (prevFreeOffset) {
	prevFreeHeader.freeNext = offset;
	if (faLseek(fa, prevFreeOffset, SEEK_SET) < 0) return;
	(void)faWrite(fa, &prevFreeHeader, sizeof(prevFreeHeader));
    } else {
	fa->firstFree = offset;

	faHeader.magic = FA_MAGIC;
	faHeader.firstFree = fa->firstFree;

	if (faLseek(fa, 0, SEEK_SET) < 0) return;
	(void)faWrite(fa, &faHeader, sizeof(faHeader));
    }
}

void faClose(faFile fa) {
    fdClose(fa->fd);
    free(fa);
}

int faFirstOffset(faFile fa) {
    return faNextOffset(fa, 0);
}

int faNextOffset(faFile fa, unsigned int lastOffset) {
    struct faHeader header;
    int offset;

    if (lastOffset) {
	offset = lastOffset - sizeof(header);
    } else {
	offset = sizeof(struct faFileHeader);
    }

    if (offset >= fa->fileSize) return 0;

    (void)faLseek(fa, offset, SEEK_SET);
    if (faRead(fa, &header, sizeof(header)) != sizeof(header)) {
	return 0;
    }
    if (!lastOffset && !header.isFree) return (offset + sizeof(header));

    do {
	offset += header.size;

	(void)faLseek(fa, offset, SEEK_SET);
	if (faRead(fa, &header, sizeof(header)) != sizeof(header)) {
	    return 0;
	}

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
