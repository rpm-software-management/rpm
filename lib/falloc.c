#include "system.h"

#include "falloc.h"
#include "intl.h"

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

    fas.fd = open(path, flags, perms);
    if (fas.fd < 0) return NULL;

    /* is this file brand new? */
    end = lseek(fas.fd, 0, SEEK_END);
    if (!end) {
	newHdr.magic = FA_MAGIC;
	newHdr.firstFree = 0;
	if (write(fas.fd, &newHdr, sizeof(newHdr)) != sizeof(newHdr)) {
	    close(fas.fd);
	    return NULL;
	}
	fas.firstFree = 0;
	fas.fileSize = sizeof(newHdr);
    }
    else {
	lseek(fas.fd, 0, SEEK_SET);
	if (read(fas.fd, &newHdr, sizeof(newHdr)) != sizeof(newHdr)) {
	    close(fas.fd);
	    return NULL;
	}
	if (newHdr.magic != FA_MAGIC) {
	    close(fas.fd);
	    return NULL;
	}
	fas.firstFree = newHdr.firstFree;

	if (lseek(fas.fd, 0, SEEK_END) < 0) {
	    close(fas.fd);
	    return NULL;
	}
	
	fas.fileSize = lseek(fas.fd, 0, SEEK_CUR);
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
	if (lseek(fa->fd, nextFreeBlock, SEEK_SET) < 0) return 0;
	if (read(fa->fd, &header, sizeof(header)) != sizeof(header)) return 0;

	if (!header.isFree) {
	    fprintf(stderr, _("free list corrupt (%d)- contact "
			"support@redhat.com\n"), nextFreeBlock);
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

	if (lseek(fa->fd, footerOffset, SEEK_SET) < 0) return 0;
	if (read(fa->fd, &footer, sizeof(footer)) != sizeof(footer)) 
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
	    if (lseek(fa->fd, header.freePrev, SEEK_SET) < 0) return 0;
	    if (read(fa->fd, &prevFreeHeader, sizeof(prevFreeHeader)) !=
			 sizeof(prevFreeHeader)) 
		return 0;
	    origPrevFreeHeader = prevFreeHeader;

	    prevFreeHeader.freeNext = header.freeNext;
	}

	/* and after */
	if (header.freeNext) {
	    if (lseek(fa->fd, header.freeNext, SEEK_SET) < 0) return 0;
	    if (read(fa->fd, &nextFreeHeader, sizeof(nextFreeHeader)) !=
			 sizeof(nextFreeHeader)) 
		return 0;
	    origNextFreeHeader = nextFreeHeader;

	    nextFreeHeader.freePrev = header.freePrev;
	}

	/* if any of these fail, try and restore everything before leaving */
	if (updateHeader) {
	    if (lseek(fa->fd, 0, SEEK_SET) < 0) 
		return 0;
	    else if (write(fa->fd, &faHeader, sizeof(faHeader)) !=
			     sizeof(faHeader)) 
		return 0;
	} else {
	    if (lseek(fa->fd, header.freePrev, SEEK_SET) < 0)
		return 0;
	    else if (write(fa->fd, &prevFreeHeader, sizeof(prevFreeHeader)) !=
			 sizeof(prevFreeHeader))
		return 0;
	    restorePrevHeader = &origPrevFreeHeader;
	}

	if (header.freeNext) {
	    if (lseek(fa->fd, header.freeNext, SEEK_SET) < 0)
		return 0;
	    else if (write(fa->fd, &nextFreeHeader, sizeof(nextFreeHeader)) !=
			 sizeof(nextFreeHeader))
		return 0;

	    restoreNextHeader = &origNextFreeHeader;
	}

	if (!failed) {
	    if (lseek(fa->fd, newBlockOffset, SEEK_SET) < 0) 
		failed = 1;
	    else if (write(fa->fd, &header, sizeof(header)) !=
			 sizeof(header)) {
		failed = 1;
		restoreHeader = &origHeader;
	    }
	}

	if (!failed) {
	    if (lseek(fa->fd, footerOffset, SEEK_SET) < 0) 
		failed = 1;
	    else if (write(fa->fd, &footer, sizeof(footer)) !=
			 sizeof(footer)) {
		failed = 1;
		restoreFooter = &origFooter;
	    }
	}

	if (failed) {
	    if (updateHeader) {
		faHeader.firstFree = newBlockOffset;
		fa->firstFree = newBlockOffset;
	    	lseek(fa->fd, 0, SEEK_SET);
	        write(fa->fd, &faHeader, sizeof(faHeader));
	    } 

	    if (restorePrevHeader) {
	    	lseek(fa->fd, header.freePrev, SEEK_SET);
	    	write(fa->fd, restorePrevHeader, sizeof(*restorePrevHeader));
	    }

	    if (restoreNextHeader) {
	    	lseek(fa->fd, header.freeNext, SEEK_SET);
	    	write(fa->fd, restoreNextHeader, sizeof(*restoreNextHeader));
	    }

	    if (restoreHeader) {
	    	lseek(fa->fd, newBlockOffset, SEEK_SET);
	    	write(fa->fd, restoreHeader, sizeof(header));
	    }

	    if (restoreFooter) {
	    	lseek(fa->fd, footerOffset, SEEK_SET);
	    	write(fa->fd, restoreFooter, sizeof(footer));
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
        lseek(fa->fd, newBlockOffset, SEEK_SET);
	if (write(fa->fd, space, size) != size) {
	    return 0;
	}

        lseek(fa->fd, newBlockOffset, SEEK_SET);
	if (write(fa->fd, &header, sizeof(header)) != sizeof(header)) {
	    return 0;
	}

        lseek(fa->fd, footerOffset, SEEK_SET);
	if (write(fa->fd, &footer, sizeof(footer)) != sizeof(footer)) {
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
	if (lseek(fa->fd, prevFreeOffset, SEEK_SET) < 0) return;
	if (read(fa->fd, &prevFreeHeader, sizeof(prevFreeHeader)) != 
		sizeof(prevFreeHeader)) return;

	while (prevFreeHeader.freeNext && prevFreeHeader.freeNext < offset) {
	    prevFreeOffset = prevFreeHeader.freeNext;
	    if (lseek(fa->fd, prevFreeOffset, SEEK_SET) < 0) return;
	    if (read(fa->fd, &prevFreeHeader, sizeof(prevFreeHeader)) != 
		    sizeof(prevFreeHeader)) return;
	} 

	nextFreeOffset = prevFreeHeader.freeNext;
    }

    if (nextFreeOffset) {
	if (lseek(fa->fd, nextFreeOffset, SEEK_SET) < 0) return;
	if (read(fa->fd, &nextFreeHeader, sizeof(nextFreeHeader)) != 
		sizeof(nextFreeHeader)) return;
    }

    if (lseek(fa->fd, offset, SEEK_SET) < 0)
	return;
    if (read(fa->fd, &header, sizeof(header)) != sizeof(header))
	return;

    footerOffset = offset + header.size - sizeof(footer);

    if (lseek(fa->fd, footerOffset, SEEK_SET) < 0)
	return;
    if (read(fa->fd, &footer, sizeof(footer)) != sizeof(footer))
	return;

    header.isFree = 1;
    header.freeNext = nextFreeOffset;
    header.freePrev = prevFreeOffset;
    footer.isFree = 1;

    lseek(fa->fd, offset, SEEK_SET);
    write(fa->fd, &header, sizeof(header));

    lseek(fa->fd, footerOffset, SEEK_SET);
    write(fa->fd, &footer, sizeof(footer));

    if (nextFreeOffset) {
	nextFreeHeader.freePrev = offset;
	if (lseek(fa->fd, nextFreeOffset, SEEK_SET) < 0) return;
	if (write(fa->fd, &nextFreeHeader, sizeof(nextFreeHeader)) != 
		sizeof(nextFreeHeader)) return;
    }

    if (prevFreeOffset) {
	prevFreeHeader.freeNext = offset;
	if (lseek(fa->fd, prevFreeOffset, SEEK_SET) < 0) return;
	write(fa->fd, &prevFreeHeader, sizeof(prevFreeHeader));
    } else {
	fa->firstFree = offset;

	faHeader.magic = FA_MAGIC;
	faHeader.firstFree = fa->firstFree;

	if (lseek(fa->fd, 0, SEEK_SET) < 0) return;
	write(fa->fd, &faHeader, sizeof(faHeader));
    }
}

void faClose(faFile fa) {
    close(fa->fd);
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

    lseek(fa->fd, offset, SEEK_SET);
    if (read(fa->fd, &header, sizeof(header)) != sizeof(header)) {
	return 0;
    }
    if (!lastOffset && !header.isFree) return (offset + sizeof(header));

    do {
	offset += header.size;

	lseek(fa->fd, offset, SEEK_SET);
	if (read(fa->fd, &header, sizeof(header)) != sizeof(header)) {
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
