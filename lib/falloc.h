#ifndef H_FALLOC
#define H_FALLOC

/* File space allocation routines. Best fit allocation is used, free blocks
   are compacted. Minimal fragmentation is more important then speed. This
   uses 32 bit offsets on all platforms and should be byte order independent */

typedef /*@abstract@*/ struct faFile_s {
    int fd;
    int readOnly;
    unsigned int firstFree;
    unsigned long fileSize;
} * faFile;

#ifdef	UNUSED
struct FaPlace_s;
typedef struct FaPlace * faPlace;
#endif

/* flags here is the same as for open(2) - NULL returned on error */
faFile faOpen(char * path, int flags, int perms);
unsigned int faAlloc(faFile fa, unsigned int size); /* returns 0 on failure */
void faFree(faFile fa, unsigned int offset);
void faClose(faFile fa);

int faFcntl(faFile fa, int op, void *lip);
int faLseek(faFile fa, off_t off, int op);
int faFileno(faFile);

int faFirstOffset(faFile fa);
int faNextOffset(faFile fa, unsigned int lastOffset);  /* 0 at end */

#endif	/* H_FALLOC */
