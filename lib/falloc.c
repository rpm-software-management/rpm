#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "falloc.h"

#define FA_MAGIC	0x02050920

typedef unsigned int u32;		/* this could be wrong someday */

struct faFileHeader{
    u32 magic;
    u32 firstFree;
};

/* the free list is kept *sorted* to keep fragment compaction fast */

struct faBlock {
    u32 isFree;
    u32 next;				/* only meaningful if free */
    u32 prev;				/* only meaningful if free */
    u32 size;				
};

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
    u32 nextFreeBlock;
    u32 bestFreeBlock = 0;
    u32 bestFreeSize = 0;
    unsigned int newBlock;
    struct faBlock block, prevBlock, nextBlock;
    int failed = 0;

    /* Make sure they are allocing multiples of four bytes. It'll keep
       things smoother that way */
    (size % 4) ? size += (4 - (size % 4)) : 0;

    /* first, look through the free list for the best fit */
    nextFreeBlock =  fa->firstFree;
    while (nextFreeBlock) {
	if (lseek(fa->fd, nextFreeBlock, SEEK_SET) < 0) return 0;
	if (read(fa->fd, &block, sizeof(block)) != sizeof(block)) return 0;

	if (block.size >= size) {
	    if (bestFreeBlock) {
		if (block.size < bestFreeSize) {
		    bestFreeSize = block.size;
		    bestFreeBlock = nextFreeBlock;
		}
	    } else {
		bestFreeSize = block.size;
		bestFreeBlock = nextFreeBlock;
	    }
	}

	nextFreeBlock = block.next;
    }

    if (bestFreeBlock) {
	if (lseek(fa->fd, bestFreeBlock, SEEK_SET) < 0) return 0;
	if (read(fa->fd, &block, sizeof(block)) != sizeof(block)) 
	    return 0;

	/* update the free list chain */
	if (lseek(fa->fd, block.prev, SEEK_SET) < 0) return 0;
	if (read(fa->fd, &prevBlock, sizeof(prevBlock)) != sizeof(prevBlock)) 
	    return 0;
	if (lseek(fa->fd, block.next, SEEK_SET) < 0) return 0;
	if (read(fa->fd, &nextBlock, sizeof(nextBlock)) != sizeof(nextBlock)) 
	    return 0;

	prevBlock.next = block.next;
	nextBlock.prev = block.prev;

	if (lseek(fa->fd, block.prev, SEEK_SET) < 0) return 0;
	if (write(fa->fd, &prevBlock, sizeof(prevBlock)) != sizeof(prevBlock)) 
	    return 0;

	if (lseek(fa->fd, block.next, SEEK_SET) < 0) {
	    failed = 1;
	} else {
	    if (write(fa->fd, &nextBlock, sizeof(nextBlock)) != 
		sizeof(nextBlock)) {
		failed = 1;
	    }
	}

	if (failed) {
	    /* try and restore the "prev" block, this won't be a complete
		disaster */
	    prevBlock.next = bestFreeBlock;

	    lseek(fa->fd, block.prev, SEEK_SET);
	    write(fa->fd, &prevBlock, sizeof(prevBlock));
		
	    return 0;
	}

	block.isFree = 0; 		/* mark it as used */
	block.prev = block.next = 0;	
	
	/* At some point, we should split this block into two if it's
	   bigger then the amount that's being allocated. Any space left
	   at the end of this block is wasted right now ***/

	if (lseek(fa->fd, bestFreeBlock, SEEK_SET) < 0) {
	    failed = 1;
	} else {
	    if (write(fa->fd, &block, sizeof(block)) != sizeof(block)) {
		failed = 1;
	    }
	}

	if (failed) {
	    /* this space is gone :-( this really shouldn't ever happen. It
	       won't result in furthur date coruption though, so lets not
	       make it worse! */
	    return 0;
	}

	newBlock = bestFreeBlock;
    } else {
	char * space;

	/* make a new block */
	newBlock = fa->fileSize;

	space = calloc(1, size);
	if (!space) return 0;

	block.next = block.prev = 0;
	block.size = size;
	block.isFree = 0;

        lseek(fa->fd, newBlock, SEEK_SET);
	if (write(fa->fd, &block, sizeof(block)) != sizeof(block)) {
	    free(space);	
	    return 0;
	}
	if (write(fa->fd, space, size) != size) {
	    free(space);	
	    return 0;
	}
	free(space);

	fa->fileSize += sizeof(block) + size;
    }
    
    return newBlock + sizeof(block); 
}

int faFree(faFile fa, unsigned int offset) {
    struct faBlock block;

    /* this is *really* bad ****/

    offset -= sizeof(block);
    if (lseek(fa->fd, offset, SEEK_SET) < 0) return 0;
    if (read(fa->fd, &block, sizeof(block)) != sizeof(block)) return 0;

    block.isFree = 1;
    if (lseek(fa->fd, offset, SEEK_SET) < 0) return 0;
    if (write(fa->fd, &block, sizeof(block)) != sizeof(block)) return 0;

    return 1;
}

void faClose(faFile fa) {
    close(fa->fd);
    free(fa);
}

unsigned int faFirstOffset(faFile fa) {
    return faNextOffset(fa, 0);
}

unsigned int faNextOffset(faFile fa, unsigned int lastOffset) {
    struct faBlock block;
    int offset;

    if (lastOffset) {
	offset = lastOffset - sizeof(block);
    } else {
	offset = sizeof(struct faFileHeader);
    }

    if (offset >= fa->fileSize) return 0;

    lseek(fa->fd, offset, SEEK_SET);
    if (read(fa->fd, &block, sizeof(block)) != sizeof(block)) {
	return 0;
    }
    if (!lastOffset && !block.isFree) return (offset + sizeof(block));

    do {
	offset += sizeof(block) + block.size;

	lseek(fa->fd, offset, SEEK_SET);
	if (read(fa->fd, &block, sizeof(block)) != sizeof(block)) {
	    return 0;
	}

	if (!block.isFree) break;
    } while (offset < fa->fileSize && block.isFree);

    if (offset < fa->fileSize)
	return (offset + sizeof(block));
    else
	return 0;
}
