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

/* flags here is the same as for open(2) - NULL returned on error */
FD_t fadOpen(const char * path, int flags, int perms)
{
    struct faFileHeader newHdr;
    FD_t fd;

    if (flags & O_WRONLY)
	return NULL;

    fd = ufdio->open(path, flags, perms);
    if (Ferror(fd))
	/* XXX Fstrerror */
	return NULL;

    fdSetIoCookie(fd, fadio);
    fadSetFirstFree(fd, 0);
    fadSetFileSize(fd, Fseek(fd, 0, SEEK_END));

    /* is this file brand new? */
    if (fadGetFileSize(fd) == 0) {
	newHdr.magic = FA_MAGIC;
	newHdr.firstFree = 0;
	if (Fwrite(&newHdr, sizeof(newHdr), 1, fd) != sizeof(newHdr)) {
	    Fclose(fd);
	    return NULL;
	}
	fadSetFirstFree(fd, 0);
	fadSetFileSize(fd, sizeof(newHdr));
    } else {
	if (Pread(fd, &newHdr, sizeof(newHdr), 0) != sizeof(newHdr)) {
	    Fclose(fd);
	    return NULL;
	}
	if (newHdr.magic != FA_MAGIC) {
	    Fclose(fd);
	    return NULL;
	}
	fadSetFirstFree(fd, newHdr.firstFree);
	fadSetFileSize(fd, Fseek(fd, 0, SEEK_END));

	if (fadGetFileSize(fd) < 0) {
	    Fclose(fd);
	    return NULL;
	}
    }

    return fd;
}

/* returns 0 on failure */
unsigned int fadAlloc(FD_t fd, unsigned int size)
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

    nextFreeBlock = fadGetFirstFree(fd);
    newBlockOffset = 0;

    while (nextFreeBlock && !newBlockOffset) {
	if (Pread(fd, &header, sizeof(header), nextFreeBlock) != sizeof(header)) return 0;

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

	if (Pread(fd, &footer, sizeof(footer), footerOffset) != sizeof(footer)) 
	    return 0;
	origFooter = footer;

	/* should we split this block into two? */
	/* XXX implement fragment creation here */

	footer.isFree = header.isFree = 0;

	/* remove it from the free list before */
	if (newBlockOffset == fadGetFirstFree(fd)) {
	    faHeader.magic = FA_MAGIC;
	    faHeader.firstFree = header.freeNext;
	    fadSetFirstFree(fd, header.freeNext);
	    updateHeader = 1;
	} else {
	    if (Pread(fd, &prevFreeHeader, sizeof(prevFreeHeader),
			header.freePrev) != sizeof(prevFreeHeader)) 
		return 0;
	    origPrevFreeHeader = prevFreeHeader;

	    prevFreeHeader.freeNext = header.freeNext;
	}

	/* and after */
	if (header.freeNext) {
	    if (Pread(fd, &nextFreeHeader, sizeof(nextFreeHeader),
			header.freeNext) != sizeof(nextFreeHeader)) 
		return 0;
	    origNextFreeHeader = nextFreeHeader;

	    nextFreeHeader.freePrev = header.freePrev;
	}

	/* if any of these fail, try and restore everything before leaving */
	if (updateHeader) {
	    if (Pwrite(fd, &faHeader, sizeof(faHeader), 0) !=
			     sizeof(faHeader)) 
		return 0;
	} else {
	    if (Pwrite(fd, &prevFreeHeader, sizeof(prevFreeHeader),
			header.freePrev) != sizeof(prevFreeHeader))
		return 0;
	    restorePrevHeader = &origPrevFreeHeader;
	}

	if (header.freeNext) {
	    if (Pwrite(fd, &nextFreeHeader, sizeof(nextFreeHeader),
			header.freeNext) != sizeof(nextFreeHeader))
		return 0;

	    restoreNextHeader = &origNextFreeHeader;
	}

	if (!failed) {
	    if (Pwrite(fd, &header, sizeof(header), newBlockOffset) !=
			 sizeof(header)) {
		failed = 1;
		restoreHeader = &origHeader;
	    }
	}

	if (!failed) {
	    if (Pwrite(fd, &footer, sizeof(footer),
			footerOffset) != sizeof(footer)) {
		failed = 1;
		restoreFooter = &origFooter;
	    }
	}

	if (failed) {
	    if (updateHeader) {
		faHeader.firstFree = newBlockOffset;
		fadSetFirstFree(fd, newBlockOffset);
	        (void)Pwrite(fd, &faHeader, sizeof(faHeader), 0);
	    } 

	    if (restorePrevHeader)
	    	(void)Pwrite(fd, restorePrevHeader, sizeof(*restorePrevHeader),
				header.freePrev);

	    if (restoreNextHeader)
	    	(void)Pwrite(fd, restoreNextHeader, sizeof(*restoreNextHeader),
				header.freeNext);

	    if (restoreHeader)
	    	(void)Pwrite(fd, restoreHeader, sizeof(header),
				newBlockOffset);

	    if (restoreFooter)
	    	(void)Pwrite(fd, restoreFooter, sizeof(footer),
				footerOffset);

	    return 0;
	}
    } else {
	char * space;

	/* make a new block */
	newBlockOffset = fadGetFileSize(fd);
	footerOffset = newBlockOffset + size - sizeof(footer);

	space = alloca(size);
	if (space == NULL) return 0;
	memset(space, 0, size);

	footer.isFree = header.isFree = 0;
	footer.size = header.size = size;
	header.freePrev = header.freeNext = 0;

	/* reserve all space up front */
	/* XXX TODO: check max. no. of bytes to write */
	if (Pwrite(fd, space, size, newBlockOffset) != size)
	    return 0;

	if (Pwrite(fd, &header, sizeof(header), newBlockOffset) != sizeof(header))
	    return 0;

	if (Pwrite(fd, &footer, sizeof(footer), footerOffset) != sizeof(footer))
	    return 0;

	fadSetFileSize(fd, fadGetFileSize(fd) + size);
    }
    
    return newBlockOffset + sizeof(header); 
}

void fadFree(FD_t fd, unsigned int offset)
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
    prevFreeOffset = fadGetFirstFree(fd);

    if (!prevFreeOffset || (prevFreeOffset > offset)) {
	nextFreeOffset = fadGetFirstFree(fd);
	prevFreeOffset = 0;
    } else {
	if (Pread(fd, &prevFreeHeader, sizeof(prevFreeHeader),
			prevFreeOffset) != sizeof(prevFreeHeader))
	    return;

	while (prevFreeHeader.freeNext && prevFreeHeader.freeNext < offset) {
	    prevFreeOffset = prevFreeHeader.freeNext;
	    if (Pread(fd, &prevFreeHeader, sizeof(prevFreeHeader),
			prevFreeOffset) != sizeof(prevFreeHeader))
		return;
	} 

	nextFreeOffset = prevFreeHeader.freeNext;
    }

    if (nextFreeOffset) {
	if (Pread(fd, &nextFreeHeader, sizeof(nextFreeHeader),
			nextFreeOffset) != sizeof(nextFreeHeader))
	    return;
    }

    if (Pread(fd, &header, sizeof(header), offset) != sizeof(header))
	return;

    footerOffset = offset + header.size - sizeof(footer);

    if (Pread(fd, &footer, sizeof(footer), footerOffset) != sizeof(footer))
	return;

    header.isFree = 1;
    header.freeNext = nextFreeOffset;
    header.freePrev = prevFreeOffset;
    footer.isFree = 1;

    /* XXX TODO: set max. no. of bytes to write */
    (void)Pwrite(fd, &header, sizeof(header), offset);

    (void)Pwrite(fd, &footer, sizeof(footer), footerOffset);

    if (nextFreeOffset) {
	nextFreeHeader.freePrev = offset;
	if (Pwrite(fd, &nextFreeHeader, sizeof(nextFreeHeader),
			nextFreeOffset) != sizeof(nextFreeHeader))
	    return;
    }

    if (prevFreeOffset) {
	prevFreeHeader.freeNext = offset;
	if (Pwrite(fd, &prevFreeHeader, sizeof(prevFreeHeader),
			prevFreeOffset) != sizeof(prevFreeHeader))
	    return;
    } else {
	fadSetFirstFree(fd, offset);

	faHeader.magic = FA_MAGIC;
	faHeader.firstFree = fadGetFirstFree(fd);

	/* XXX TODO: set max. no. of bytes to write */
	if (Pwrite(fd, &faHeader, sizeof(faHeader), 0) != sizeof(faHeader))
	    return;
    }
}

int fadFirstOffset(FD_t fd)
{
    return fadNextOffset(fd, 0);
}

int fadNextOffset(FD_t fd, unsigned int lastOffset)
{
    struct faHeader header;
    int offset;

    offset = (lastOffset)
	? (lastOffset - sizeof(header))
	: sizeof(struct faFileHeader);

    if (offset >= fadGetFileSize(fd))
	return 0;

    if (Pread(fd, &header, sizeof(header), offset) != sizeof(header))
	return 0;

    if (!lastOffset && !header.isFree)
	return (offset + sizeof(header));

    do {
	offset += header.size;

	if (Pread(fd, &header, sizeof(header), offset) != sizeof(header))
	    return 0;

	if (!header.isFree) break;
    } while (offset < fadGetFileSize(fd) && header.isFree);

    if (offset < fadGetFileSize(fd)) {
	/* Sanity check this to make sure we're not going in loops */
	offset += sizeof(header);

	if (offset <= lastOffset) return -1;

	return offset;
    } else
	return 0;
}
