#ifndef H_FALLOC
#define H_FALLOC

/* File space allocation routines. Best fit allocation is used, free blocks
   are compacted. Minimal fragmentation is more important then speed. This
   uses 32 bit offsets on all platforms and should be byte order independent */

#ifdef __cplusplus
extern "C" {
#endif

static inline long int fadGetFileSize(FD_t fd) {
    return fd->fileSize;
}

static inline void fadSetFileSize(FD_t fd, long int fileSize) {
    fd->fileSize = fileSize;
}

static inline unsigned int fadGetFirstFree(FD_t fd) {
    return fd->firstFree;
}

static inline void fadSetFirstFree(FD_t fd, unsigned int firstFree) {
    fd->firstFree = firstFree;
}

/*@null@*/ FD_t	fadOpen		(const char * path, int flags, mode_t perms);
unsigned int	fadAlloc	(FD_t fd, unsigned int size); /* 0 on failure */
void		fadFree		(FD_t fd, unsigned int offset);

int		fadFirstOffset	(FD_t fd);
int		fadNextOffset	(FD_t fd, unsigned int lastoff); /* 0 at end */

#ifdef __cplusplus
}
#endif

#endif	/* H_FALLOC */
