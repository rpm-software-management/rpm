#ifndef H_FALLOC
#define H_FALLOC

/* File space allocation routines. Best fit allocation is used, free blocks
   are compacted. Minimal fragmentation is more important then speed. This
   uses 32 bit offsets on all platforms and should be byte order independent */

#if 0
typedef /*@abstract@*/ struct faFile_s {
    /*@owned@*/ FD_t fd;
    int readOnly;
    unsigned int firstFree;
    unsigned long fileSize;
} * faFile;
#else
typedef FD_t faFile;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* flags here is the same as for open(2) - NULL returned on error */
/*@only@*/ faFile faOpen(const char * path, int flags, int perms);
unsigned int faAlloc(faFile fa, unsigned int size); /* returns 0 on failure */
void faFree(faFile fa, unsigned int offset);

FD_t faFileno(faFile fa);
int faSeek(faFile fa, off_t pos, int whence);
int faClose( /*@only@*/ faFile fa);

int faFcntl(faFile fa, int op, void *lip);

int faFirstOffset(faFile fa);
int faNextOffset(faFile fa, unsigned int lastOffset);  /* 0 at end */

#ifdef __cplusplus
}
#endif

#endif	/* H_FALLOC */
